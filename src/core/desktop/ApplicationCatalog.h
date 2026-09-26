#pragma once
#include <QStringList>
#include <QVariantList>

namespace logitune {
// Read launcher metadata only. Exec expansion and activation belong to GIO.
class ApplicationCatalog {
public:
    static QStringList directories();
    static QVariantList applications(const QStringList &directories, const QString &locale,
                                     const QStringList &desktops);
    static QVariantList installedApplications();
    static QString launchCommand(const QString &desktopFile);
};
}
