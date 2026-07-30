#pragma once
#include <QString>

namespace logitune {

struct ButtonAction {
    enum Type {
        Default,
        Keystroke,
        /// Latch a modifier-only combo down until the action fires again,
        /// which is what lets a pointer drag carry the modifier.
        StickyModifier,
        GestureTrigger,
        SmartShiftToggle,
        DpiCycle,
        AppLaunch,
        DBus,
        Media,
        PresetRef,
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
