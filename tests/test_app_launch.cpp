#include <gtest/gtest.h>

#include <QFile>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

#include "input/UinputInjector.h"

using logitune::UinputInjector;

TEST(AppLaunch, SplitsProgramFromQuotedArguments)
{
    const QString touch = QStandardPaths::findExecutable(QStringLiteral("touch"));
    ASSERT_FALSE(touch.isEmpty());

    QTemporaryDir temp;
    ASSERT_TRUE(temp.isValid());
    const QString marker = temp.filePath(QStringLiteral("argument with spaces $literal"));

    UinputInjector injector;
    injector.launchApp(QStringLiteral("%1 \"%2\"").arg(touch, marker));

    for (int i = 0; i < 100 && !QFile::exists(marker); ++i)
        QTest::qWait(10);

    EXPECT_TRUE(QFile::exists(marker));
}
