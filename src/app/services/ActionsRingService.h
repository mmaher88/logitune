#pragma once
#include "ActionsRing.h"
#include <QElapsedTimer>
#include <QObject>
#include <QPointer>
#include <QVariantList>
#include <QQmlEngine>

class QQmlEngine;
class QQuickView;

namespace logitune {
class ProfileEngine;
class DeviceManager;
class ActionExecutor;
class ActiveDeviceResolver;
class IDesktopIntegration;
class ProfileOrchestrator;

class ActionsRingService : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList entries READ entries NOTIFY editorChanged)
    Q_PROPERTY(int size READ size WRITE setSize NOTIFY editorChanged)
public:
    ActionsRingService(ProfileEngine *profiles, DeviceManager *devices, ActionExecutor *executor,
                       ActiveDeviceResolver *selection, IDesktopIntegration *desktop,
                       ProfileOrchestrator *orchestrator, QObject *parent = nullptr);
    ~ActionsRingService() override;
    void setEngine(QQmlEngine *engine) { m_engine = engine; }
    void trigger(const QString &serial, bool pressed);
    Q_INVOKABLE void toggle();
    Q_INVOKABLE void dismiss();
    Q_INVOKABLE void haptic(const QString &kind);
    Q_INVOKABLE void run(const QString &action);
    Q_INVOKABLE void placed(QObject *ring);
    Q_INVOKABLE void finished();
    void shutdown();
    QVariantList entries() const;
    int size() const;
    void setSize(int size);
    Q_INVOKABLE void setEntries(const QVariantList &entries);
signals:
    void editorChanged();
private:
    void open(const QString &serial);
    void destroyOverlays();
    ProfileEngine *m_profiles;
    DeviceManager *m_devices;
    ActionExecutor *m_executor;
    ActiveDeviceResolver *m_selection;
    IDesktopIntegration *m_desktop;
    ProfileOrchestrator *m_orchestrator;
    QPointer<QQmlEngine> m_engine;
    QList<QQuickView *> m_overlays;
    QPointer<QObject> m_activeRing;
    QString m_serial;
    ActionsRingTrigger m_trigger;
    QElapsedTimer m_clock;
    ButtonAction m_pendingAction;
};
}
