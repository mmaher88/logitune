#pragma once
#include "interfaces/IDesktopIntegration.h"
#include <QHash>
#include <QVariantList>
#include <QVariantMap>
#include <optional>

namespace logitune::test {

class MockDesktop : public logitune::IDesktopIntegration {
    Q_OBJECT
public:
    explicit MockDesktop(QObject *parent = nullptr)
        : IDesktopIntegration(parent)
    {}

    void start() override {}

    bool available() const override { return true; }

    QString desktopName() const override { return QStringLiteral("mock"); }

    QStringList detectedCompositors() const override { return {}; }

    QString variantKey() const override { return m_variantKey; }

    std::optional<logitune::ButtonAction>
    resolveNamedAction(const QString &id) const override {
        auto it = m_scripted.constFind(id);
        if (it == m_scripted.constEnd())
            return std::nullopt;
        return *it;
    }

    void blockGlobalShortcuts(bool /*block*/) override {
        ++m_blockCount;
    }

    QVariantList runningApplications() const override {
        return m_runningApps;
    }

    bool supportsActionRing() const override { return m_supportsActionRing; }
    void requestActionRingPlacement(int requestId, const QSize &windowSize,
                                    const QPoint &anchor) override {
        m_lastPlacementRequest = requestId;
        m_lastWindowSize = windowSize;
        m_lastAnchor = anchor;
        if (m_autoCompletePlacement)
            emit actionRingPlacementReady(requestId, QPoint(100, 120),
                                          QRect(0, 0, 1920, 1080), QPoint(290, 285));
    }
    void cancelActionRingPlacement(int requestId) override {
        m_lastCancelledRequest = requestId;
    }

    // --- Test helpers ---

    void simulateFocus(const QString &wmClass, const QString &title) {
        emit activeWindowChanged(wmClass, title);
    }

    void setRunningApps(const QVariantList &apps) {
        m_runningApps = apps;
    }

    void setVariantKey(const QString &key) { m_variantKey = key; }

    /// Pre-program resolve results by id. Any id not in the map returns nullopt.
    void scriptResolve(const QString &id, logitune::ButtonAction action) {
        m_scripted.insert(id, action);
    }

    void clearScriptedResolves() { m_scripted.clear(); }

    int blockCount() const { return m_blockCount; }
    void setSupportsActionRing(bool supported) { m_supportsActionRing = supported; }
    void setAutoCompletePlacement(bool enabled) { m_autoCompletePlacement = enabled; }
    int lastPlacementRequest() const { return m_lastPlacementRequest; }
    int lastCancelledRequest() const { return m_lastCancelledRequest; }

private:
    QVariantList m_runningApps;
    int m_blockCount = 0;
    QString m_variantKey = QStringLiteral("mock");
    QHash<QString, logitune::ButtonAction> m_scripted;
    bool m_supportsActionRing = false;
    bool m_autoCompletePlacement = true;
    int m_lastPlacementRequest = -1;
    int m_lastCancelledRequest = -1;
    QSize m_lastWindowSize;
    QPoint m_lastAnchor;
};

} // namespace logitune::test
