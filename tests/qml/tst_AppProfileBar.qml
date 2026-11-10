import QtQuick
import QtTest
import Logitune

Item {
    width: 800; height: 48

    AppProfileBar {
        id: bar
        anchors.fill: parent
    }

    TestCase {
        name: "AppProfileBar"
        when: windowShown

        function chip(i) { return findChild(bar, "profileChip_" + i) }

        function test_defaultsChipShowsAllAppsMark() {
            waitForRendering(bar)
            var c = chip(0)
            verify(c !== null, "Defaults chip exists")
            verify(findChild(c, "defaultsIcon").visible, "Defaults shows the all-apps mark")

            var img = findChild(c, "appIcon")
            compare(img.source.toString(), "", "Defaults asks the icon theme for nothing")
            verify(!img.visible)

            // Mark (16) + spacing (6) + label + 12px padding on each side.
            // tryCompare: the Row positions its children on the next polish.
            tryCompare(c, "width", 16 + 6 + findChild(c, "chipLabel").implicitWidth + 24)
        }

        function test_appChipWithoutIconHasNoMarkOrGap() {
            ProfileModel.addProfile("tst-appprofilebar", "Test App", "")
            var idx = ProfileModel.rowCount() - 1
            tryVerify(function() { return chip(idx) !== null })
            var c = chip(idx)

            verify(!findChild(c, "defaultsIcon").visible, "only Defaults gets the mark")
            verify(!findChild(c, "appIcon").visible)
            // No space reserved for an icon that isn't shown.
            tryCompare(c, "width", findChild(c, "chipLabel").implicitWidth + 24)

            ProfileModel.removeProfile(idx)
        }
    }
}
