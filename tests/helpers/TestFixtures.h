#pragma once
#include <gtest/gtest.h>
#include <QCoreApplication>
#include <QDir>
#include <QStandardPaths>
#include <QTemporaryDir>
#include "ProfileEngine.h"

namespace logitune::test {

/// Returns a static QCoreApplication, creating it on first call.
/// Required for QSignalSpy and other Qt event-loop facilities in tests.
/// Also enables QStandardPaths test mode so production code paths that
/// write to AppConfigLocation (e.g. setupProfileForDevice) don't pollute
/// the real user config.
inline QCoreApplication *ensureApp() {
    static int argc = 0;
    static QCoreApplication *app = nullptr;
    if (!app) {
        QStandardPaths::setTestModeEnabled(true);
        app = new QCoreApplication(argc, nullptr);
    }
    return app;
}

/// Wipes the AppConfigLocation subtree so tests that drive through
/// setupProfileForDevice (which writes to AppConfigLocation) start from
/// a clean slate. Must be called AFTER ensureApp() so test mode is on.
inline void clearTestAppConfig() {
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    if (!dir.isEmpty())
        QDir(dir).removeRecursively();
}

/// GTest fixture providing a temporary directory and a default Profile helper.
class ProfileFixture : public ::testing::Test {
protected:
    void SetUp() override {
        ensureApp();
        ASSERT_TRUE(m_tmpDir.isValid());
    }

    QString tmpPath() const { return m_tmpDir.path(); }

    /// Returns a Profile pre-populated with sensible defaults for testing.
    logitune::Profile makeDefaultProfile() {
        logitune::Profile p;
        p.name                = QStringLiteral("default");
        p.dpi                 = 1000;
        p.smartShiftEnabled   = true;
        p.smartShiftThreshold = 128;
        p.smoothScrolling     = false;
        p.scrollDirection     = QStringLiteral("standard");
        p.hiResScroll         = true;
        return p;
    }

private:
    QTemporaryDir m_tmpDir;
};

} // namespace logitune::test
