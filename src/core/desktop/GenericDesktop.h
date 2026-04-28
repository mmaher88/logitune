#pragma once
#include "desktop/LinuxDesktopBase.h"
#include <optional>

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
    QString variantKey() const override;
    std::optional<ButtonAction> resolveNamedAction(const QString &id) const override;
};

} // namespace logitune
