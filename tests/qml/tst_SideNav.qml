import QtQuick
import QtTest
import Logitune

Item {
    width: 200; height: 600

    SideNav {
        id: nav
        anchors.fill: parent
    }

    TestCase {
        name: "SideNav"
        when: windowShown

        Component {
            id: signalSpyComponent
            SignalSpy {}
        }

        function init() {
            nav.currentPage = "buttons"
        }

        function test_defaultPageIsButtons() {
            compare(nav.currentPage, "buttons")
        }

        function test_clickSettingsChangesPage() {
            var spy = createTemporaryObject(signalSpyComponent, this, { target: nav, signalName: "pageSelected" })
            waitForRendering(nav)

            // Layout: 16px spacer + ~20px header + 17px margins + 40px items
            // Header ≈ 53px. Items: 0→53, 1→110, 2→167, 3→224
            // Settings (index 3) center ≈ 244
            mouseClick(nav, 100, 244)

            compare(nav.currentPage, "settings", "clicking settings should update currentPage")
            compare(spy.count, 1, "pageSelected signal should fire")
        }

        function test_clickPointScrollChangesPage() {
            waitForRendering(nav)

            // pointscroll (index 1) center ≈ 130
            mouseClick(nav, 100, 130)

            compare(nav.currentPage, "pointscroll", "clicking point & scroll should update currentPage")
        }

        // Flow section removed — no longer part of the app
    }
}
