#include <gtest/gtest.h>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QUuid>
#include <QLocalSocket>
#include <QFile>
#include <QProcess>
#include <QProcessEnvironment>

#include "SingleInstance.h"

TEST(SingleInstanceTest, SecondaryRequestsRingWithoutMainWindow)
{
    QTemporaryDir dir;
    const QString serverName = "logitune-test-" + QUuid::createUuid().toString(QUuid::WithoutBraces);
    logitune::SingleInstance primary(dir.filePath("instance.lock"), serverName);
    ASSERT_FALSE(primary.start().has_value());
    QSignalSpy ring(&primary, &logitune::SingleInstance::ringRequested);
    QSignalSpy shown(&primary, &logitune::SingleInstance::showRequested);
    logitune::SingleInstance secondary(dir.filePath("instance.lock"), serverName);
    EXPECT_EQ(secondary.start(logitune::SingleInstance::Command::Ring), std::optional<int>(0));
    ASSERT_TRUE(ring.wait(2000));
    EXPECT_EQ(shown.count(), 0);
}

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

TEST(SingleInstanceTest, SecondaryRequestsQuitAndExitsSuccessfully)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    const QString serverName = "logitune-test-" + QUuid::createUuid().toString(QUuid::WithoutBraces);
    logitune::SingleInstance primary(dir.filePath("instance.lock"), serverName);
    ASSERT_FALSE(primary.start().has_value());
    QSignalSpy quit(&primary, &logitune::SingleInstance::quitRequested);
    QSignalSpy shown(&primary, &logitune::SingleInstance::showRequested);

    logitune::SingleInstance secondary(dir.filePath("instance.lock"), serverName);
    const auto exitCode = secondary.start(logitune::SingleInstance::Command::Quit);
    ASSERT_TRUE(exitCode.has_value());
    EXPECT_EQ(*exitCode, 0);
    ASSERT_TRUE(quit.wait(2000));
    EXPECT_EQ(quit.count(), 1);
    EXPECT_EQ(shown.count(), 0);
}

TEST(SingleInstanceTest, QuitWithoutPrimaryExitsSuccessfullyWithoutStartingServer)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    const QString serverName = "logitune-test-" + QUuid::createUuid().toString(QUuid::WithoutBraces);
    logitune::SingleInstance instance(dir.filePath("instance.lock"), serverName);

    const auto exitCode = instance.start(logitune::SingleInstance::Command::Quit);
    ASSERT_TRUE(exitCode.has_value());
    EXPECT_EQ(*exitCode, 0);
    QLocalSocket socket;
    socket.connectToServer(serverName);
    EXPECT_FALSE(socket.waitForConnected(100));
    EXPECT_FALSE(QFile::exists(dir.filePath("instance.lock")));
}

TEST(SingleInstanceTest, QuitCommandLineWithoutPrimaryDoesNotStartApplication)
{
    QTemporaryDir runtime;
    ASSERT_TRUE(runtime.isValid());
    QProcess process;
    auto environment = QProcessEnvironment::systemEnvironment();
    environment.insert("QT_QPA_PLATFORM", "offscreen");
    environment.insert("XDG_RUNTIME_DIR", runtime.path());
    process.setProcessEnvironment(environment);
    process.start(QStringLiteral(LOGITUNE_BINARY), {"--quit"});
    ASSERT_TRUE(process.waitForFinished(5000));
    EXPECT_EQ(process.exitStatus(), QProcess::NormalExit);
    EXPECT_EQ(process.exitCode(), 0) << process.readAllStandardError().constData();
    EXPECT_FALSE(QFile::exists(runtime.filePath("logitune.lock")));
    EXPECT_FALSE(QFile::exists(runtime.filePath("logitune")));
}
