import QtQuick
import QtTest
import Logitune

TestCase {
    name: "HapticsPage"
    when: windowShown
    width: 600
    height: 600

    Component { id: pageFactory; HapticsPage { anchors.fill: parent } }

    // The test harness has no mouse connected, so nothing is reported: both sections must stay
    // hidden rather than showing sliders that would do nothing.
    function test_hidesControlsWithoutAMouse() {
        let page = createTemporaryObject(pageFactory, this)
        verify(page !== null)
        compare(DeviceModel.hapticsSupported, false)
        compare(DeviceModel.panelForceSupported, false)
        compare(findChild(page, "hapticFeedbackSection").visible, false)
        compare(findChild(page, "panelForceSection").visible, false)
    }

    function test_forcePercentCoversTheRange() {
        let page = createTemporaryObject(pageFactory, this)
        // With no mouse the range is 0..0; forceRange guards against dividing by zero.
        compare(page.forceRange, 1)
        compare(page.forcePercent(0), 0)
    }
}
