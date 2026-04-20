#include <gtest/gtest.h>
#include <QFile>
#include <QString>

namespace {

QString readDesktopEntry(const QString &path) {
    QFile file(path);
    EXPECT_TRUE(file.open(QIODevice::ReadOnly | QIODevice::Text))
        << "Failed to open " << path.toStdString();
    return QString::fromUtf8(file.readAll());
}

} // namespace

TEST(AutostartDesktopEntry, ExecHasMinimizedFlag) {
    const QString content = readDesktopEntry(
        QStringLiteral(SOURCE_ROOT "/data/logitune-autostart.desktop"));
    EXPECT_TRUE(content.contains(QStringLiteral("Exec=logitune --minimized\n")))
        << "Autostart entry must invoke logitune with --minimized so the app "
           "starts hidden to the tray on login";
}

TEST(AutostartDesktopEntry, HasGnomeAutostartEnabled) {
    const QString content = readDesktopEntry(
        QStringLiteral(SOURCE_ROOT "/data/logitune-autostart.desktop"));
    EXPECT_TRUE(content.contains(QStringLiteral("X-GNOME-Autostart-enabled=true")))
        << "GNOME autostart requires the explicit opt-in key";
}

TEST(AutostartDesktopEntry, LauncherEntryDoesNotMinimize) {
    const QString content = readDesktopEntry(
        QStringLiteral(SOURCE_ROOT "/data/logitune.desktop"));
    EXPECT_TRUE(content.contains(QStringLiteral("Exec=logitune\n")))
        << "Manual app-launcher entry must not inherit --minimized; the user "
           "clicking the app launcher expects the window to appear";
    EXPECT_FALSE(content.contains(QStringLiteral("--minimized")))
        << "Launcher .desktop leaked the autostart flag; split files failed";
}
