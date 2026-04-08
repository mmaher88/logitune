#pragma once
#include "desktop/LinuxDesktopBase.h"

namespace logitune {

class GenericDesktop : public LinuxDesktopBase {
    Q_OBJECT
public:
    explicit GenericDesktop(QObject *parent = nullptr);

    void start() override;
    bool available() const override;
    QString desktopName() const override;
    QStringList detectedCompositors() const override;
    void blockGlobalShortcuts(bool block) override;
};

} // namespace logitune
