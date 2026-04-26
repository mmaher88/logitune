#pragma once
#include <QString>
#include <QJsonObject>

namespace logitune {

class DescriptorWriter {
public:
    enum Result { Ok, IoError, JsonError };

    Result write(const QString &dirPath, const QJsonObject &obj, QString *errorOut = nullptr);
};

} // namespace logitune
