#pragma once
#include "interfaces/IDesktopIntegration.h"
#include <QHash>

namespace logitune {

class LinuxDesktopBase : public IDesktopIntegration {
    Q_OBJECT
public:
    using IDesktopIntegration::IDesktopIntegration;
    QVariantList runningApplications() const override;
protected:
    static const QStringList &desktopDirs();
    QString resolveDesktopFile(const QString &resourceClass) const;
    mutable QHash<QString, QString> m_resolveCache;
};

} // namespace logitune
