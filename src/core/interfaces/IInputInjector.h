#pragma once
#include <QObject>
#include <QString>

namespace logitune {

class IInputInjector : public QObject {
    Q_OBJECT
public:
    using QObject::QObject;
    virtual ~IInputInjector() = default;

    virtual bool init() = 0;
    virtual void injectKeystroke(const QString &combo) = 0;

    /// Flip the latch on a modifier-only combo: hold its keys down until the
    /// same combo is toggled again, so a pointer drag can carry the modifier.
    /// Returns whether the combo is held afterwards — false also covers a
    /// combo the injector refuses to latch.
    virtual bool toggleModifierLatch(const QString &combo) = 0;

    virtual void injectCtrlScroll(int direction) = 0;
    virtual void injectHorizontalScroll(int direction) = 0;
    virtual void sendDBusCall(const QString &spec) = 0;
    virtual void launchApp(const QString &command) = 0;
};

} // namespace logitune
