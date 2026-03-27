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

    // ── Mouse image ──────────────────────────────────────────────────────────
    Image {
        id: mouseImage
        anchors.centerIn: parent
        width: parent.implicitWidth
        height: parent.implicitHeight
        source: root.imageSource
        fillMode: Image.PreserveAspectFit
        smooth: true
        mipmap: true
    }

    // ── Button zone overlays with hotspot circles ────────────────────────────
    // Positions are percentage-based (from Options+ core_metadata.json),
    // scaled to our render size. Only configurable buttons (2–7).
    //
    // Button IDs:
    //   2 = Middle / scroll wheel click
    //   3 = Back
    //   4 = Forward
    //   5 = Gesture button
    //   6 = Top / ModeShift
    //   7 = ThumbWheel (horizontal scroll)

    readonly property var hotspotPositions: [
        // 2: Middle / scroll wheel — metadata: (71%, 15%)
        { buttonId: 2, dotXPct: 0.71, dotYPct: 0.15,
          zoneX: 0.59, zoneY: 0.05, zoneW: 0.22, zoneH: 0.15 },
        // 3: Back — metadata: (45%, 60%)
        { buttonId: 3, dotXPct: 0.45, dotYPct: 0.60,
          zoneX: 0.32, zoneY: 0.52, zoneW: 0.22, zoneH: 0.14 },
        // 4: Forward — metadata: (35%, 43%)
        { buttonId: 4, dotXPct: 0.35, dotYPct: 0.43,
          zoneX: 0.22, zoneY: 0.36, zoneW: 0.22, zoneH: 0.14 },
        // 5: Gesture button — metadata: (8%, 58%)
        { buttonId: 5, dotXPct: 0.08, dotYPct: 0.58,
          zoneX: 0.00, zoneY: 0.50, zoneW: 0.16, zoneH: 0.15 },
        // 6: Top / ModeShift — metadata: (81%, 34%)
        { buttonId: 6, dotXPct: 0.81, dotYPct: 0.34,
          zoneX: 0.70, zoneY: 0.26, zoneW: 0.18, zoneH: 0.14 },
        // 7: ThumbWheel — metadata: (55%, 51.5%)
        { buttonId: 7, dotXPct: 0.55, dotYPct: 0.515,
          zoneX: 0.44, zoneY: 0.44, zoneW: 0.22, zoneH: 0.14 },
    ]

    Repeater {
        model: root.showHotspots ? root.hotspotPositions.length : 0

        Item {
            required property int modelData

            readonly property var hp: root.hotspotPositions[modelData]

            // Invisible hit area (percentage-based)
            MouseArea {
                x: hp.zoneX * root.implicitWidth
                y: hp.zoneY * root.implicitHeight
                width:  hp.zoneW * root.implicitWidth
                height: hp.zoneH * root.implicitHeight
                cursorShape: Qt.PointingHandCursor
                onClicked: root.buttonClicked(hp.buttonId)
            }

            // 18x18 hotspot circle — white for dark background
            Rectangle {
                x: hp.dotXPct * root.implicitWidth  - 9
                y: hp.dotYPct * root.implicitHeight - 9
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
