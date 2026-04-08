#pragma once
#include "desktop/LinuxDesktopBase.h"

namespace logitune {

class GnomeDesktop : public LinuxDesktopBase {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "com.logitune.FocusWatcher")
public:
    explicit GnomeDesktop(QObject *parent = nullptr);

    void start() override;
    bool available() const override;
    QString desktopName() const override;
    QStringList detectedCompositors() const override;
    void blockGlobalShortcuts(bool block) override;

public slots:
    void focusChanged(const QString &appId, const QString &title);

private:
    bool ensureExtensionInstalled();
    int detectShellMajorVersion();

    QString m_lastAppId;
    bool m_available = false;
};

} // namespace logitune
