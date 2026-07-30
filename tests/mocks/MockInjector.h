#pragma once
#include "interfaces/IInputInjector.h"
#include <QSet>
#include <QVector>
#include <QString>

namespace logitune::test {

class MockInjector : public logitune::IInputInjector {
    Q_OBJECT
public:
    struct Call {
        QString method;
        QString arg;
    };

    explicit MockInjector(QObject *parent = nullptr)
        : IInputInjector(parent)
    {}

    bool init() override { return true; }

    void injectKeystroke(const QString &combo) override {
        m_calls.append({QStringLiteral("injectKeystroke"), combo});
    }

    /// Mirrors UinputInjector's flip-flop at combo granularity, which is all
    /// a caller can observe: the real injector's key-level bookkeeping only
    /// matters where two latched combos overlap.
    bool toggleModifierLatch(const QString &combo) override {
        m_calls.append({QStringLiteral("toggleModifierLatch"), combo});
        if (m_latched.remove(combo))
            return false;
        m_latched.insert(combo);
        return true;
    }

    void injectCtrlScroll(int direction) override {
        m_calls.append({QStringLiteral("injectCtrlScroll"), QString::number(direction)});
    }

    void injectHorizontalScroll(int direction) override {
        m_calls.append({QStringLiteral("injectHorizontalScroll"), QString::number(direction)});
    }

    void sendDBusCall(const QString &spec) override {
        m_calls.append({QStringLiteral("sendDBusCall"), spec});
    }

    void launchApp(const QString &command) override {
        m_calls.append({QStringLiteral("launchApp"), command});
    }

    // --- Test helpers ---

    void clear() { m_calls.clear(); }

    bool isLatched(const QString &combo) const { return m_latched.contains(combo); }

    bool hasCalled(const QString &method) const {
        for (const auto &c : m_calls) {
            if (c.method == method)
                return true;
        }
        return false;
    }

    QString lastArg(const QString &method) const {
        for (int i = m_calls.size() - 1; i >= 0; --i) {
            if (m_calls[i].method == method)
                return m_calls[i].arg;
        }
        return {};
    }

    const QVector<Call> &calls() const { return m_calls; }

private:
    QVector<Call> m_calls;
    QSet<QString> m_latched;
};

} // namespace logitune::test
