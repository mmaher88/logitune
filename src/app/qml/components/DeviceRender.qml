import QtQuick
import Logitune

// Mouse device render — MX Master 3S PNG with invisible clickable button zones overlaid.
// White hotspot circles on configurable buttons (no left/right click zones shown).
Item {
    id: root

    implicitWidth:  280
    implicitHeight: 414

    signal buttonClicked(int buttonId)

    // Allow parent to override the image source per page
    property string imageSource: "qrc:/Logitune/qml/assets/mx-master-3s.png"
    property bool showHotspots: true  // set false on Point & Scroll page

    // Painted-rect properties — actual rendered area after PreserveAspectFit
    readonly property real paintedX: (width - mouseImage.paintedWidth) / 2
    readonly property real paintedY: (height - mouseImage.paintedHeight) / 2
    readonly property real paintedW: mouseImage.paintedWidth
    readonly property real paintedH: mouseImage.paintedHeight

    // ── Half-extents for the invisible hit zone around each hotspot dot ──────
    readonly property real zoneHalfW: 0.11   // fraction of paintedW
    readonly property real zoneHalfH: 0.07   // fraction of paintedH

    Image {
        id: mouseImage
        anchors.centerIn: parent
        width:  parent.implicitWidth
        height: parent.implicitHeight
        source: root.imageSource
        fillMode: Image.PreserveAspectFit
        smooth: true
        mipmap: true
    }

    Repeater {
        model: root.showHotspots ? DeviceModel.buttonHotspots.length : 0

        Item {
            required property int modelData

            readonly property var hp: DeviceModel.buttonHotspots[modelData]

            // Skip non-configurable buttons (left/right click etc.)
            visible: hp.configurable

            // Dot centre in item coordinates
            readonly property real dotCX: root.paintedX + hp.hotspotXPct * root.paintedW
            readonly property real dotCY: root.paintedY + hp.hotspotYPct * root.paintedH

            // Invisible hit zone centred on dot
            MouseArea {
                x: dotCX - root.zoneHalfW * root.paintedW
                y: dotCY - root.zoneHalfH * root.paintedH
                width: root.zoneHalfW * root.paintedW * 2
                height: root.zoneHalfH * root.paintedH * 2
                cursorShape: Qt.PointingHandCursor
                onClicked: root.buttonClicked(hp.buttonId)
            }

            // 18x18 hotspot circle — white for dark background
            Rectangle {
                x: dotCX - 9
                y: dotCY - 9
                width: 18; height: 18
                radius: 9
                color: "transparent"
                border.color: Theme.accent
                border.width: 2
                opacity: 0.7

                Behavior on opacity { NumberAnimation { duration: 200 } }

                // Inner dot
                Rectangle {
                    anchors.centerIn: parent
                    width: 6; height: 6
                    radius: 3
                    color: Theme.accent
                    opacity: 0.6
                }
            }
        }
    }
}
