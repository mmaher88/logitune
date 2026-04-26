#include <gtest/gtest.h>
#include <QFile>
#include <QString>

TEST(AutostartDesktopEntry, ExecHasMinimizedFlag) {
    QFile file(QStringLiteral(SOURCE_ROOT "/data/logitune-autostart.desktop"));
    ASSERT_TRUE(file.open(QIODevice::ReadOnly | QIODevice::Text))
        << "Failed to open " << file.fileName().toStdString();
    const QString content = QString::fromUtf8(file.readAll());

    EXPECT_TRUE(content.contains(QStringLiteral("Exec=logitune --minimized\n")))
        << "Autostart entry must invoke logitune with --minimized so the app "
           "starts hidden to the tray on login";
}

TEST(AutostartDesktopEntry, HasGnomeAutostartEnabled) {
    QFile file(QStringLiteral(SOURCE_ROOT "/data/logitune-autostart.desktop"));
    ASSERT_TRUE(file.open(QIODevice::ReadOnly | QIODevice::Text))
        << "Failed to open " << file.fileName().toStdString();
    const QString content = QString::fromUtf8(file.readAll());

    EXPECT_TRUE(content.contains(QStringLiteral("X-GNOME-Autostart-enabled=true")))
        << "GNOME autostart requires the explicit opt-in key";
}

TEST(AutostartDesktopEntry, LauncherEntryDoesNotMinimize) {
    QFile file(QStringLiteral(SOURCE_ROOT "/data/logitune.desktop"));
    ASSERT_TRUE(file.open(QIODevice::ReadOnly | QIODevice::Text))
        << "Failed to open " << file.fileName().toStdString();
    const QString content = QString::fromUtf8(file.readAll());

    EXPECT_TRUE(content.contains(QStringLiteral("Exec=logitune\n")))
        << "Manual app-launcher entry must not inherit --minimized; the user "
           "clicking the app launcher expects the window to appear";
    EXPECT_FALSE(content.contains(QStringLiteral("--minimized")))
        << "Launcher .desktop leaked the autostart flag; split files failed";
}
