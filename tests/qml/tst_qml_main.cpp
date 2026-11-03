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
#include "logging/LogManager.h"

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

        m_deviceModel.setDisplayValues(1000, true, 50, true, false, "scroll");
    }

private:
    logitune::DeviceModel  m_deviceModel;
    logitune::ButtonModel  m_buttonModel;
    logitune::ActionModel  m_actionModel;
    logitune::ProfileModel  m_profileModel;
    logitune::SettingsModel m_settingsModel;
};

QUICK_TEST_MAIN_WITH_SETUP(logitune_qml, QmlTestSetup)

#include "tst_qml_main.moc"
