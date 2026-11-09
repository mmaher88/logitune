/// Tests for SystemColorScheme: portal value parsing and live change filtering.
#include <gtest/gtest.h>
#include <QDBusVariant>
#include <QSignalSpy>
#include "services/SystemColorScheme.h"
#include "helpers/TestFixtures.h"

namespace logitune::test {

TEST(SystemColorScheme, PreferDarkIsDark) {
    EXPECT_EQ(SystemColorScheme::darkFromPortalValue(QVariant(1u)), true);
}

TEST(SystemColorScheme, NoPreferenceAndPreferLightAreLight) {
    EXPECT_EQ(SystemColorScheme::darkFromPortalValue(QVariant(0u)), false);
    EXPECT_EQ(SystemColorScheme::darkFromPortalValue(QVariant(2u)), false);
}

TEST(SystemColorScheme, UnwrapsNestedDBusVariants) {
    // Read() replies wrap the value twice; SettingChanged wraps it once.
    QVariant once = QVariant::fromValue(QDBusVariant(QVariant(1u)));
    QVariant twice = QVariant::fromValue(QDBusVariant(once));
    EXPECT_EQ(SystemColorScheme::darkFromPortalValue(once), true);
    EXPECT_EQ(SystemColorScheme::darkFromPortalValue(twice), true);
}

TEST(SystemColorScheme, NonNumericValueIsIgnored) {
    EXPECT_FALSE(SystemColorScheme::darkFromPortalValue(QVariant(QStringLiteral("dark"))).has_value());
}

TEST(SystemColorScheme, EmitsOnColorSchemeChange) {
    ensureApp();
    SystemColorScheme scheme;
    QSignalSpy spy(&scheme, &SystemColorScheme::darkChanged);

    scheme.onSettingChanged(QStringLiteral("org.freedesktop.appearance"),
                            QStringLiteral("color-scheme"), QDBusVariant(QVariant(1u)));
    scheme.onSettingChanged(QStringLiteral("org.freedesktop.appearance"),
                            QStringLiteral("color-scheme"), QDBusVariant(QVariant(0u)));

    ASSERT_EQ(spy.count(), 2);
    EXPECT_TRUE(spy.at(0).at(0).toBool());
    EXPECT_FALSE(spy.at(1).at(0).toBool());
}

TEST(SystemColorScheme, RepeatedValueEmitsOnce) {
    ensureApp();
    SystemColorScheme scheme;
    QSignalSpy spy(&scheme, &SystemColorScheme::darkChanged);

    for (int i = 0; i < 2; ++i)
        scheme.onSettingChanged(QStringLiteral("org.freedesktop.appearance"),
                                QStringLiteral("color-scheme"), QDBusVariant(QVariant(1u)));

    EXPECT_EQ(spy.count(), 1);
}

TEST(SystemColorScheme, IgnoresOtherSettings) {
    ensureApp();
    SystemColorScheme scheme;
    QSignalSpy spy(&scheme, &SystemColorScheme::darkChanged);

    scheme.onSettingChanged(QStringLiteral("org.freedesktop.appearance"),
                            QStringLiteral("accent-color"), QDBusVariant(QVariant(1u)));
    scheme.onSettingChanged(QStringLiteral("org.gnome.desktop.interface"),
                            QStringLiteral("color-scheme"), QDBusVariant(QVariant(1u)));

    EXPECT_EQ(spy.count(), 0);
}

} // namespace logitune::test
