#include <QtQuickTest>
#include <QtQml/qqmlextensionplugin.h>
#include <QObject>
#include <QQmlEngine>
#include <QQmlContext>
#include "models/DeviceModel.h"
#include "models/ButtonModel.h"
#include "models/ActionModel.h"
#include "models/ProfileModel.h"
#include "models/SettingsModel.h"
#include "models/InstalledAppsModel.h"
#include "logging/LogManager.h"
#include "ActionsRing.h"

class RingEditorStub : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList entries READ entries NOTIFY editorChanged)
    Q_PROPERTY(int size MEMBER m_size NOTIFY editorChanged)
public:
    QVariantList entries() const { return m_entries; }
    Q_INVOKABLE void setEntries(const QVariantList &entries) { m_entries = entries; emit editorChanged(); }
    Q_INVOKABLE void setSize(int size) { m_size = size; emit editorChanged(); }
signals:
    void editorChanged();
private:
    int m_size = 100;
    QVariantList m_entries = logitune::ringSlotsToJson(logitune::defaultRingSlots()).toVariantList();
};

Q_IMPORT_QML_PLUGIN(LogitunePlugin)

class QmlTestSetup : public QObject
{
    Q_OBJECT
public:
    QmlTestSetup() = default;

public slots:
    void applicationAvailable()
    {
        logitune::LogManager::instance().init(true);
    }

    void qmlEngineAvailable(QQmlEngine *engine)
    {
        engine->addImportPath("qrc:/");

        engine->rootContext()->setContextProperty("DeviceModel",  &m_deviceModel);
        engine->rootContext()->setContextProperty("ButtonModel",  &m_buttonModel);
        engine->rootContext()->setContextProperty("ActionModel",  &m_actionModel);
        engine->rootContext()->setContextProperty("ProfileModel",  &m_profileModel);
        engine->rootContext()->setContextProperty("SettingsModel", &m_settingsModel);
        engine->rootContext()->setContextProperty("ActionsRing", &m_ringEditor);
        engine->rootContext()->setContextProperty("InstalledApps", &m_installedApps);

        m_deviceModel.setDisplayValues(1000, true, 50, true, false, "scroll");
    }

private:
    logitune::DeviceModel  m_deviceModel;
    logitune::ButtonModel  m_buttonModel;
    logitune::ActionModel  m_actionModel;
    logitune::ProfileModel  m_profileModel;
    logitune::SettingsModel m_settingsModel;
    RingEditorStub m_ringEditor;
    logitune::InstalledAppsModel m_installedApps;
};

QUICK_TEST_MAIN_WITH_SETUP(logitune_qml, QmlTestSetup)

#include "tst_qml_main.moc"
