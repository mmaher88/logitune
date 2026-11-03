#include <gtest/gtest.h>
#include <QCoreApplication>
#include <QPointer>
#include <QQmlContext>
#include <QSignalSpy>
#include <QQmlExpression>
#include <QtQml/qqmlextensionplugin.h>

#include "MainWindowController.h"
#include "models/ActionModel.h"
#include "models/ButtonModel.h"
#include "models/DeviceModel.h"
#include "models/ProfileModel.h"
#include "models/SettingsModel.h"
#include "DeviceRegistry.h"
#include "DeviceSession.h"
#include "PhysicalDevice.h"
#include "hidpp/HidrawDevice.h"

Q_IMPORT_QML_PLUGIN(LogitunePlugin)

class ObservedDeviceModel : public logitune::DeviceModel
{
public:
    int selectionReceivers() const { return receivers(SIGNAL(selectedChanged())); }
};

class MainWindowTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        engine.addImportPath("qrc:/");
        engine.rootContext()->setContextProperty("DeviceModel", &deviceModel);
        engine.rootContext()->setContextProperty("ButtonModel", &buttonModel);
        engine.rootContext()->setContextProperty("ActionModel", &actionModel);
        engine.rootContext()->setContextProperty("ProfileModel", &profileModel);
        engine.rootContext()->setContextProperty("SettingsModel", &settingsModel);
    }

    QQuickWindow *window()
    {
        if (engine.rootObjects().isEmpty())
            return nullptr;
        return qobject_cast<QQuickWindow*>(engine.rootObjects().last());
    }

    void addDevice(const QString &id)
    {
        auto session = std::make_unique<logitune::DeviceSession>(
            std::make_unique<logitune::hidpp::HidrawDevice>("/dev/null"),
            0xFF, QStringLiteral("Bluetooth"), &registry);
        session->setConnectedForTest(true);
        session->setDeviceNameForTest(id);
        auto device = std::make_unique<logitune::PhysicalDevice>(id);
        device->attachTransport(session.get());
        deviceModel.addPhysicalDevice(device.get());
        sessions.push_back(std::move(session));
        devices.push_back(std::move(device));
    }

    logitune::DeviceRegistry registry;
    std::vector<std::unique_ptr<logitune::DeviceSession>> sessions;
    std::vector<std::unique_ptr<logitune::PhysicalDevice>> devices;
    ObservedDeviceModel deviceModel;
    logitune::ButtonModel buttonModel;
    logitune::ActionModel actionModel;
    logitune::ProfileModel profileModel;
    logitune::SettingsModel settingsModel;
    QQmlApplicationEngine engine;
    logitune::MainWindowController windows{engine};
};

TEST_F(MainWindowTest, StartsWithoutWindowAndLoadsOnlyOnShow)
{
    EXPECT_TRUE(engine.rootObjects().isEmpty());
    QCoreApplication::processEvents();
    ASSERT_TRUE(windows.showMainWindow());
    ASSERT_NE(window(), nullptr);
    EXPECT_TRUE(window()->isVisible());
    ASSERT_TRUE(windows.showMainWindow());
    EXPECT_EQ(engine.rootObjects().size(), 1);
}

TEST_F(MainWindowTest, SelectionAndThemeSurviveReloadAndBackgroundDeviceEvents)
{
    addDevice("first");
    addDevice("second");
    deviceModel.setSelectedIndex(1);
    const int backgroundReceivers = deviceModel.selectionReceivers();
    ASSERT_TRUE(windows.showMainWindow());
    EXPECT_EQ(deviceModel.selectedIndex(), 1);
    EXPECT_GT(deviceModel.selectionReceivers(), backgroundReceivers);

    QQmlExpression theme(QQmlEngine::contextForObject(window()), window(), "Theme.dark = true");
    theme.evaluate();
    ASSERT_FALSE(theme.hasError());

    ASSERT_TRUE(window()->close());
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QCoreApplication::processEvents();
    EXPECT_EQ(deviceModel.selectionReceivers(), backgroundReceivers);
    EXPECT_EQ(deviceModel.selectedIndex(), 1);
    deviceModel.setSelectedIndex(0);
    deviceModel.setSelectedIndex(1);
    deviceModel.setDisplayValues(3200, true, 50, true, false, "scroll");

    ASSERT_TRUE(windows.showMainWindow());
    EXPECT_EQ(deviceModel.selectedIndex(), 1);
    EXPECT_EQ(deviceModel.currentDPI(), 3200);
    QQmlExpression restoredTheme(QQmlEngine::contextForObject(window()), window(), "Theme.dark");
    EXPECT_TRUE(restoredTheme.evaluate().toBool());
    EXPECT_FALSE(restoredTheme.hasError());
}

TEST_F(MainWindowTest, CloseDestroysRootAndShowRecreatesIt)
{
    ASSERT_TRUE(windows.showMainWindow());
    QPointer<QQuickWindow> oldWindow = window();
    QSignalSpy destroyed(oldWindow, &QObject::destroyed);
    ASSERT_TRUE(oldWindow->close());
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    ASSERT_TRUE(oldWindow.isNull());
    EXPECT_EQ(destroyed.count(), 1);
    EXPECT_TRUE(engine.rootObjects().isEmpty());

    deviceModel.setDisplayValues(2400, true, 50, true, false, "scroll");
    ASSERT_TRUE(windows.showMainWindow());
    ASSERT_NE(window(), nullptr);
    EXPECT_TRUE(window()->isVisible());
    EXPECT_EQ(deviceModel.currentDPI(), 2400);
}

TEST_F(MainWindowTest, ShowDuringDeferredCloseKeepsReplacementAlive)
{
    ASSERT_TRUE(windows.showMainWindow());
    QPointer<QQuickWindow> oldWindow = window();
    ASSERT_TRUE(oldWindow->close());
    ASSERT_TRUE(windows.showMainWindow());
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    EXPECT_TRUE(oldWindow.isNull());
    ASSERT_NE(window(), nullptr);
    EXPECT_TRUE(window()->isVisible());
    EXPECT_EQ(engine.rootObjects().size(), 1);
}
