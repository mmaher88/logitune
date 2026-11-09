#include "hidpp/features/DeviceName.h"

#include <QByteArray>
#include <algorithm>

namespace logitune::hidpp::features {

int DeviceName::parseNameLength(const Report &r)
{
    return static_cast<int>(r.params[0]);
}

QString DeviceName::parseNameChunk(const Report &r)
{
    // Read up to paramLength ASCII chars from params
    QByteArray bytes;
    bytes.reserve(r.paramLength);
    for (int i = 0; i < r.paramLength; ++i)
        bytes.append(static_cast<char>(r.params[i]));
    return QString::fromLatin1(bytes);
}

QString DeviceName::readName(int nameLen, const NameFragmentFetcher &fetch)
{
    // getName takes a single byte: the index of the first character to
    // return. Each reply carries as many characters as fit in the report,
    // so ask again from wherever the previous fragment ended (Solaar's
    // hidpp20.get_name() does the same).
    QByteArray name;
    while (name.size() < nameLen) {
        const uint8_t params[1] = { static_cast<uint8_t>(name.size()) };
        auto fragment = fetch(params);
        if (!fragment.has_value())
            break;
        const int take = std::min(fragment->paramLength,
                                  nameLen - static_cast<int>(name.size()));
        if (take <= 0)
            break;
        name.append(reinterpret_cast<const char *>(fragment->params.data()), take);
    }
    return QString::fromUtf8(name);
}

QString DeviceName::parseSerial(const Report &r)
{
    // Serial is ASCII, same layout as a name chunk
    return parseNameChunk(r);
}

} // namespace logitune::hidpp::features
