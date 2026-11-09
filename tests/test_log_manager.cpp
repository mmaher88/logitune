/// Tests for LogManager: what reaches stderr and which categories the
/// debug preset turns on.
#include <gtest/gtest.h>
#include "logging/LogManager.h"
#include "helpers/TestFixtures.h"

namespace logitune::test {

TEST(LogManagerStderr, WarningsAndAboveAlwaysEcho) {
    for (bool terminal : {false, true}) {
        EXPECT_TRUE(LogManager::echoesToStderr(QtWarningMsg, terminal));
        EXPECT_TRUE(LogManager::echoesToStderr(QtCriticalMsg, terminal));
        EXPECT_TRUE(LogManager::echoesToStderr(QtFatalMsg, terminal));
    }
}

TEST(LogManagerStderr, DebugAndInfoOnlyOnTerminal) {
    EXPECT_FALSE(LogManager::echoesToStderr(QtDebugMsg, false));
    EXPECT_FALSE(LogManager::echoesToStderr(QtInfoMsg, false));
    EXPECT_TRUE(LogManager::echoesToStderr(QtDebugMsg, true));
    EXPECT_TRUE(LogManager::echoesToStderr(QtInfoMsg, true));
}

TEST(LogManagerCategories, DebugPresetLeavesRawHidReportsOff) {
    ensureApp();
    auto &mgr = LogManager::instance();
    const bool initial = mgr.isLoggingEnabled();

    mgr.setLoggingEnabled(true);
    EXPECT_TRUE(lcHidpp().isDebugEnabled());
    EXPECT_FALSE(lcHidppRaw().isDebugEnabled());

    mgr.setLoggingEnabled(initial);
}

} // namespace logitune::test
