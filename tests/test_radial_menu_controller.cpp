#include <gtest/gtest.h>

#include "desktop/KDeDesktop.h"
#include "mocks/MockDesktop.h"
#include "services/RadialMenuController.h"

using namespace logitune;
using namespace logitune::test;

TEST(RadialMenuController, UnsupportedDesktopNeverOpens)
{
    MockDesktop desktop;
    RadialMenuController controller(&desktop);

    controller.begin(RadialMenuConfig::defaults());

    EXPECT_FALSE(controller.isActive());
    EXPECT_FALSE(controller.visible());
    EXPECT_EQ(desktop.lastPlacementRequest(), -1);
}

TEST(RadialMenuController, OpensOnlyAfterDesktopPlacementCompletes)
{
    MockDesktop desktop;
    desktop.setSupportsActionRing(true);
    desktop.setAutoCompletePlacement(false);
    RadialMenuController controller(&desktop);

    controller.begin(RadialMenuConfig::defaults());
    ASSERT_TRUE(controller.isActive());
    EXPECT_FALSE(controller.visible());
    const int request = desktop.lastPlacementRequest();

    emit desktop.actionRingPlacementReady(request, QPoint(10, 20),
                                          QRect(0, 0, 800, 600), QPoint(200, 185));
    EXPECT_TRUE(controller.visible());
    EXPECT_EQ(controller.overlayPosition(), QPoint(10, 20));
    EXPECT_EQ(controller.cursorPosition(), QPoint(200, 185));
}

TEST(RadialMenuController, ActivationClosesBeforeEmittingAction)
{
    MockDesktop desktop;
    desktop.setSupportsActionRing(true);
    RadialMenuController controller(&desktop);
    ButtonAction fired;
    bool visibleWhileEmitting = true;
    QObject::connect(&controller, &RadialMenuController::triggered,
            [&controller, &fired, &visibleWhileEmitting](const ButtonAction &action) {
        fired = action;
        visibleWhileEmitting = controller.visible();
    });

    controller.begin(RadialMenuConfig::defaults());
    ASSERT_TRUE(controller.visible());
    controller.activate(0);

    EXPECT_FALSE(visibleWhileEmitting);
    EXPECT_EQ(fired.type, ButtonAction::Keystroke);
    EXPECT_EQ(fired.payload, QStringLiteral("Ctrl+C"));
}

TEST(KDeDesktop, ActionRingPlacementClampsToAvailableGeometry)
{
    const QRect available(100, 50, 1000, 700);
    const QSize size(380, 380);
    const QPoint anchor(190, 165);

    EXPECT_EQ(KDeDesktop::actionRingPlacement(QPoint(150, 80), available, size, anchor),
              QPoint(100, 50));
    EXPECT_EQ(KDeDesktop::actionRingPlacement(QPoint(1090, 740), available, size, anchor),
              QPoint(720, 370));
    EXPECT_EQ(KDeDesktop::actionRingPlacement(QPoint(600, 400), available, size, anchor),
              QPoint(410, 235));
}
