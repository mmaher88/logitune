#pragma once
#include <QString>

namespace logitune {

struct ButtonAction {
    enum Type {
        Default,
        Keystroke,
        GestureTrigger,
        SmartShiftToggle,
        DpiCycle,
        AppLaunch,
        DBus,
        Media,
    };

    Type type = Default;
    QString payload;

    static ButtonAction parse(const QString &str);
    QString serialize() const;

    bool operator==(const ButtonAction &o) const {
        return type == o.type && payload == o.payload;
    }
    bool operator!=(const ButtonAction &o) const { return !(*this == o); }
};

} // namespace logitune
