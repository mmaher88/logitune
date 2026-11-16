#pragma once
#include "interfaces/IDesktopIntegration.h"
#include <QHash>

namespace logitune {

class LinuxDesktopBase : public IDesktopIntegration {
    Q_OBJECT
public:
    using IDesktopIntegration::IDesktopIntegration;
    QVariantList runningApplications() const override;

    /// Test seam: scan these directories for .desktop files instead of the
    /// system application directories.
    void setDesktopDirsForTest(const QStringList &dirs)
    {
        m_desktopDirsOverride = dirs;
        m_resolveCache.clear();
    }

protected:
    QStringList desktopDirs() const;
    /// The .desktop file ID (completeBaseName) that `key` identifies, or an
    /// empty string if no installed .desktop file claims it. `key` is a
    /// .desktop ID itself, a StartupWMClass, or the last component of a
    /// reverse-DNS ID ("zoom" -> "us.zoom.Zoom").
    QString findDesktopFile(const QString &key) const;
    /// findDesktopFile(), falling back to `resourceClass` unchanged.
    QString resolveDesktopFile(const QString &resourceClass) const;
    mutable QHash<QString, QString> m_resolveCache;

private:
    QStringList m_desktopDirsOverride;
};

} // namespace logitune
