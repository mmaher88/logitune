#include "DescriptorWriter.h"
#include <QSaveFile>
#include <QJsonDocument>
#include <QDir>

namespace logitune {

DescriptorWriter::Result DescriptorWriter::write(
    const QString &dirPath, const QJsonObject &obj, QString *errorOut) {
    if (errorOut) errorOut->clear();

    if (!QDir(dirPath).exists()) {
        if (errorOut) *errorOut = QStringLiteral("Directory does not exist: ") + dirPath;
        return IoError;
    }

    const QString filePath = QDir(dirPath).absoluteFilePath(QStringLiteral("descriptor.json"));
    QSaveFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (errorOut) *errorOut = file.errorString();
        return IoError;
    }

    const QJsonDocument doc(obj);
    const QByteArray bytes = doc.toJson(QJsonDocument::Indented);
    if (file.write(bytes) != bytes.size()) {
        file.cancelWriting();
        if (errorOut) *errorOut = QStringLiteral("short write");
        return IoError;
    }

    if (!file.commit()) {
        if (errorOut) *errorOut = file.errorString();
        return IoError;
    }
    return Ok;
}

} // namespace logitune
