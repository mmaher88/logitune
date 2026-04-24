#include <gtest/gtest.h>

#include "desktop/GenericDesktop.h"

using logitune::GenericDesktop;

TEST(GenericDesktopResolve, VariantKeyIsGeneric) {
    GenericDesktop d;
    EXPECT_EQ(d.variantKey(), "generic");
}

TEST(GenericDesktopResolve, ResolveReturnsNulloptForEverything) {
    GenericDesktop d;
    EXPECT_FALSE(d.resolveNamedAction("show-desktop").has_value());
    EXPECT_FALSE(d.resolveNamedAction("task-switcher").has_value());
    EXPECT_FALSE(d.resolveNamedAction("arbitrary-unknown-id").has_value());
}
