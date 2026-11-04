#include <gtest/gtest.h>
#include <QFile>
#include <QDir>
#include <QProcess>
#include <QTemporaryDir>
#include "models/InstalledAppsModel.h"

using namespace logitune;

TEST(InstalledAppsModel, ReadsNamesIconsAndUsesDesktopLauncher)
{
    QTemporaryDir dir;
    QFile file(dir.filePath("example.desktop"));
    ASSERT_TRUE(file.open(QIODevice::WriteOnly));
    file.write("[Desktop Entry]\nType=Application\nName=Example\nIcon=example-icon\nExec=example %U\n");
    file.close();
    InstalledAppsModel model;
    model.reload({dir.path()});
    ASSERT_EQ(model.rowCount(), 1);
    auto app = model.get(0);
    EXPECT_EQ(app["name"].toString(), "Example");
    EXPECT_EQ(app["icon"].toString(), "example-icon");
    EXPECT_EQ(app["command"].toString(), "gio launch \"" + file.fileName() + "\"");
}

TEST(InstalledAppsModel, UserOverridesHideSystemEntriesAndFiltersNonApplications)
{
    QTemporaryDir user, system;
    auto write = [](const QString &path, const QByteArray &body) {
        QFile f(path); EXPECT_TRUE(f.open(QIODevice::WriteOnly)); f.write(body);
    };
    write(system.filePath("same.desktop"), "[Desktop Entry]\nType=Application\nName=System\nExec=sys\n");
    write(user.filePath("same.desktop"), "[Desktop Entry]\nHidden=true\n");
    write(user.filePath("hidden.desktop"), "[Desktop Entry]\nType=Application\nName=Hidden\nNoDisplay=true\nExec=x\n");
    write(user.filePath("link.desktop"), "[Desktop Entry]\nType=Link\nName=Link\nURL=https://example.invalid\n");
    InstalledAppsModel model;
    model.reload({user.path(), system.path()});
    EXPECT_EQ(model.rowCount(), 0);
}

TEST(InstalledAppsModel, LocalizedNamesAndRecursiveDesktopIds)
{
    QTemporaryDir dir;
    QDir().mkpath(dir.filePath("nested"));
    QFile file(dir.filePath("nested/app.desktop"));
    ASSERT_TRUE(file.open(QIODevice::WriteOnly));
    file.write("[Desktop Entry]\nType=Application\nName=Default\nName[de]=Anwendung\nExec=app\n");
    file.close();
    InstalledAppsModel model;
    model.reload({dir.path()}, "de_DE");
    ASSERT_EQ(model.rowCount(), 1);
    EXPECT_EQ(model.get(0)["name"].toString(), "Anwendung");
    EXPECT_EQ(model.get(0)["desktopId"].toString(), "nested-app.desktop");
}

TEST(InstalledAppsModel, LaunchCommandPreservesQuotedDesktopPathWithoutShell)
{
    QTemporaryDir dir;
    QFile file(dir.filePath("quoted \"app\\name.desktop"));
    ASSERT_TRUE(file.open(QIODevice::WriteOnly));
    file.write("[Desktop Entry]\nType=Application\nName=Quoted\nExec=app %U\n");
    file.close();
    InstalledAppsModel model;
    model.reload({dir.path()});
    ASSERT_EQ(model.rowCount(), 1);
    EXPECT_EQ(QProcess::splitCommand(model.get(0)["command"].toString()),
              QStringList({"gio", "launch", file.fileName()}));
}
