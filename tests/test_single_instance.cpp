#include <gtest/gtest.h>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QUuid>

#include "SingleInstance.h"

TEST(SingleInstanceTest, SecondaryRequestsShowAndExitsSuccessfully)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    const QString serverName = "logitune-test-" + QUuid::createUuid().toString(QUuid::WithoutBraces);
    logitune::SingleInstance primary(dir.filePath("instance.lock"), serverName);
    ASSERT_FALSE(primary.start().has_value());
    QSignalSpy shown(&primary, &logitune::SingleInstance::showRequested);

    logitune::SingleInstance secondary(dir.filePath("instance.lock"), serverName);
    const auto exitCode = secondary.start();
    ASSERT_TRUE(exitCode.has_value());
    EXPECT_EQ(*exitCode, 0);
    ASSERT_TRUE(shown.wait(2000));
    EXPECT_EQ(shown.count(), 1);
}
