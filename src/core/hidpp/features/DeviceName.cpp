#include "hidpp/features/DeviceName.h"

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

QString DeviceName::parseSerial(const Report &r)
{
    // Serial is ASCII, same layout as a name chunk
    return parseNameChunk(r);
}

} // namespace logitune::hidpp::features
