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

        // Compute the y-center of a navItem by name. The easyswitch tab is
        // conditional on DeviceModel state, so navItems.length can vary —
        // we can't hardcode pixel offsets. Layout: 16px spacer + ~20px
        // header (with 17px top margin) + items, each 40px tall with a
        // 17px top margin between them.
        function navItemCenterY(name) {
            var idx = -1;
            for (var i = 0; i < nav.navItems.length; i++) {
                if (nav.navItems[i].name === name) { idx = i; break; }
            }
            verify(idx >= 0, "no nav item named " + name)
            // 16 spacer + 53 header block + idx * (40 item + 17 margin) + 20 to centre
            return 16 + 53 + idx * 57 + 20;
        }

        function test_clickSettingsChangesPage() {
            var spy = createTemporaryObject(signalSpyComponent, this, { target: nav, signalName: "pageSelected" })
            waitForRendering(nav)

            mouseClick(nav, 100, navItemCenterY("settings"))

            compare(nav.currentPage, "settings", "clicking settings should update currentPage")
            compare(spy.count, 1, "pageSelected signal should fire")
        }

        function test_clickPointScrollChangesPage() {
            waitForRendering(nav)

            mouseClick(nav, 100, navItemCenterY("pointscroll"))

            compare(nav.currentPage, "pointscroll", "clicking point & scroll should update currentPage")
        }

        function findByObjectName(root, name) {
            if (root.objectName === name)
                return root
            for (var i = 0; i < root.children.length; i++) {
                var found = findByObjectName(root.children[i], name)
                if (found !== null)
                    return found
            }
            return null
        }

        function test_amberStripeInvisibleByDefault() {
            var stripe = findByObjectName(nav, "editStripe")
            verify(stripe !== null, "editStripe should exist")
            compare(stripe.visible, false, "editStripe should be hidden when EditorModel is undefined")
        }

        // Flow section removed — no longer part of the app
    }
}
