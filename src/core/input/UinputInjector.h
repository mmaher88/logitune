#pragma once
#include "interfaces/IInputInjector.h"
#include <set>
#include <vector>

namespace logitune {

class UinputInjector : public IInputInjector {
    Q_OBJECT
public:
    explicit UinputInjector(QObject *parent = nullptr);
    ~UinputInjector() override;

    bool init() override;
    void injectKeystroke(const QString &combo) override;
    bool toggleModifierLatch(const QString &combo) override;
    void injectCtrlScroll(int direction) override;
    void injectHorizontalScroll(int direction) override;
    void sendDBusCall(const QString &spec) override;
    void launchApp(const QString &command) override;

    void shutdown();

    // Static helpers (testable)
    static std::vector<int> parseKeystroke(const QString &combo);

    /// The keycodes of a combo made *only* of modifiers, or an empty vector
    /// for anything else. A latch is meaningful for modifiers alone: the
    /// compositor autorepeats an ordinary key that stays down, so a combo
    /// mixing the two is refused whole rather than latched in part.
    static std::vector<int> parseModifierCombo(const QString &combo);

private:
    int m_uinputFd = -1;

    /// Keys the latch is holding down. The virtual keyboard must not outlive
    /// its own pressed keys, so shutdown() drains this set.
    std::set<int> m_latched;

    void emitKey(int keycode, bool press);
    void emitSync();
    void releaseLatchedModifiers();
};

} // namespace logitune
