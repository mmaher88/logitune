#include <gtest/gtest.h>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
#include "desktop/ApplicationCatalog.h"
#include "input/UinputInjector.h"
#include "helpers/TestFixtures.h"
using namespace logitune;
namespace {
void desktopFile(const QString &path, const QByteArray &body)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    ASSERT_TRUE(file.open(QIODevice::WriteOnly));
    file.write("[Desktop Entry]\nType=Application\nName=Example\nExec=/bin/true\n");
    file.write(body);
}
}
TEST(ApplicationCatalog, UserOverridesAndHiddenEntriesMaskSystemLaunchers)
{
    QTemporaryDir temp;
    const auto user = temp.filePath("user");
    const auto system = temp.filePath("system");
    desktopFile(user + "/hidden.desktop", "Hidden=true\n");
    desktopFile(system + "/hidden.desktop", "Name=Must not appear\n");
    desktopFile(user + "/custom.desktop", "Name=My application\n");
    desktopFile(system + "/custom.desktop", "Name=System application\n");
    desktopFile(system + "/helper.desktop", "NoDisplay=true\n");
    const auto apps = ApplicationCatalog::applications({user, system}, "en_US", {"KDE"});
    ASSERT_EQ(apps.size(), 1);
    EXPECT_EQ(apps[0].toMap().value("title"), "My application");
    EXPECT_EQ(apps[0].toMap().value("desktopFile"), user + "/custom.desktop");
}
TEST(ApplicationCatalog, LocalizationDesktopVisibilityAndMissingExecutable)
{
    QTemporaryDir temp;
    desktopFile(temp.filePath("nested/app.desktop"), "Name[ru]=Программа\nName[ru_RU]=Моя программа\nOnlyShowIn=KDE;\nIcon=example\n");
    desktopFile(temp.filePath("gnome.desktop"), "OnlyShowIn=GNOME;\n");
    desktopFile(temp.filePath("excluded.desktop"), "NotShowIn=KDE;\n");
    desktopFile(temp.filePath("missing.desktop"), "TryExec=logitune-no-such-executable-38210\n");
    const auto apps = ApplicationCatalog::applications({temp.path()}, "ru_RU.UTF-8", {"KDE"});
    ASSERT_EQ(apps.size(), 1);
    EXPECT_EQ(apps[0].toMap().value("title"), QString::fromUtf8("Моя программа"));
    EXPECT_EQ(apps[0].toMap().value("desktopId"), "nested-app.desktop");
    EXPECT_EQ(apps[0].toMap().value("icon"), "example");
}
TEST(ApplicationCatalog, LaunchCommandKeepsSpacesQuotesAndShellCharactersLiteral)
{
    const QString file = "/tmp/app space \"quoted\" $literal `literal`.desktop";
    EXPECT_EQ(QProcess::splitCommand(ApplicationCatalog::launchCommand(file)), (QStringList{"gio", "launch", file}));
    EXPECT_TRUE(ApplicationCatalog::launchCommand("relative.desktop").isEmpty());
}
TEST(ApplicationCatalog, DesktopLaunchExpandsFileCodesWithoutPassingThemToApplication)
{
    logitune::test::ensureApp();
    if (QStandardPaths::findExecutable("gio").isEmpty()) GTEST_SKIP() << "gio is not installed";
    const auto touch = QStandardPaths::findExecutable("touch");
    ASSERT_FALSE(touch.isEmpty());
    QTemporaryDir temp;
    const auto marker = temp.filePath("launch succeeded");
    const auto file = temp.filePath("test app.desktop");
    desktopFile(file, ("Exec=" + touch + " \"" + marker + "\" %U\n").toUtf8());
    UinputInjector injector;
    injector.launchApp(ApplicationCatalog::launchCommand(file));
    for (int i = 0; i < 100 && !QFile::exists(marker); ++i) QTest::qWait(10);
    EXPECT_TRUE(QFile::exists(marker));
}
