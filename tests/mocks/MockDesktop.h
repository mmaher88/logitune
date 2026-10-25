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

    bool supportsInteractiveWorkspaceSwitch() const override {
        return m_supportsInteractiveWorkspace;
    }

    void beginInteractiveWorkspaceSwitch() override {
        ++m_workspaceBeginCount;
        m_workspaceGestureActive = true;
    }

    void updateInteractiveWorkspaceSwitch(int delta) override {
        ++m_workspaceUpdateCount;
        m_workspaceDeltaSum += delta;
    }

    void endInteractiveWorkspaceSwitch(int snapPercent = 50) override {
        ++m_workspaceEndCount;
        m_lastWorkspaceSnapPercent = snapPercent;
        m_workspaceGestureActive = false;
    }

    // --- Test helpers ---

    void simulateFocus(const QString &wmClass, const QString &title) {
        emit activeWindowChanged(wmClass, title);
    }

    void setRunningApps(const QVariantList &apps) {
        m_runningApps = apps;
    }

    void setVariantKey(const QString &key) { m_variantKey = key; }

    void setSupportsInteractiveWorkspace(bool enabled) {
        m_supportsInteractiveWorkspace = enabled;
    }

    /// Pre-program resolve results by id. Any id not in the map returns nullopt.
    void scriptResolve(const QString &id, logitune::ButtonAction action) {
        m_scripted.insert(id, action);
    }

    void clearScriptedResolves() { m_scripted.clear(); }

    int blockCount() const { return m_blockCount; }
    int workspaceBeginCount() const { return m_workspaceBeginCount; }
    int workspaceUpdateCount() const { return m_workspaceUpdateCount; }
    int workspaceEndCount() const { return m_workspaceEndCount; }
    int workspaceDeltaSum() const { return m_workspaceDeltaSum; }
    int lastWorkspaceSnapPercent() const { return m_lastWorkspaceSnapPercent; }
    bool workspaceGestureActive() const { return m_workspaceGestureActive; }

    void resetWorkspaceCounts() {
        m_workspaceBeginCount = 0;
        m_workspaceUpdateCount = 0;
        m_workspaceEndCount = 0;
        m_workspaceDeltaSum = 0;
        m_lastWorkspaceSnapPercent = 50;
        m_workspaceGestureActive = false;
    }

private:
    QVariantList m_runningApps;
    int m_blockCount = 0;
    int m_workspaceBeginCount = 0;
    int m_workspaceUpdateCount = 0;
    int m_workspaceEndCount = 0;
    int m_workspaceDeltaSum = 0;
    int m_lastWorkspaceSnapPercent = 50;
    bool m_workspaceGestureActive = false;
    bool m_supportsInteractiveWorkspace = false;
    QString m_variantKey = QStringLiteral("mock");
    QHash<QString, logitune::ButtonAction> m_scripted;
};

} // namespace logitune::test
