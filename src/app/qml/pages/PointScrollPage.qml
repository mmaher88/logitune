import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Logitune

// ── Point & Scroll page ──────────────────────────────────────────────────────
// Layout:
//   • Mouse render centred in the canvas
//   • Three InfoCallouts overlaid at fixed positions around the render
//   • Clicking a callout slides in the DetailPanel from the right
//   • Clicking outside the panel (or the × button) closes it
// ─────────────────────────────────────────────────────────────────────────────
Item {
    id: root

    // ── State ─────────────────────────────────────────────────────────────────
    property string activePanelType: ""   // "" means no panel open

    // ── Background ────────────────────────────────────────────────────────────
    Rectangle {
        anchors.fill: parent
        color: "#FFFFFF"
        radius: 0
    }

    // ── Canvas: device render + overlaid callouts ─────────────────────────────
    Item {
        id: canvas
        anchors {
            fill: parent
        }

        // ── Mouse render (reuses the shared DeviceRender component) ──
        // Shifts left 130px when detail panel opens
        DeviceRender {
            id: mouseRender
            imageSource: "qrc:/Logitune/qml/assets/mx-master-3s-side.png"
            showHotspots: false  // hide button hotspots — we draw our own 3 circles below
            implicitWidth: 340
            implicitHeight: 503
            anchors.centerIn: parent
            anchors.horizontalCenterOffset: root.activePanelType !== "" ? -130 : 0
            Behavior on anchors.horizontalCenterOffset {
                NumberAnimation { duration: 300; easing.type: Easing.InOutCubic }
            }
        }

        // ── Point & Scroll hotspot circles (3 only) ─────────────────────────
        // From Options+ core_metadata.json: device_point_scroll_image markers
        Repeater {
            model: [
                { xPct: 0.73, yPct: 0.16 },  // Scroll wheel
                { xPct: 0.55, yPct: 0.51 },  // Thumb wheel
                { xPct: 0.83, yPct: 0.54 },  // Pointer speed
            ]
            Rectangle {
                x: mouseRender.x + modelData.xPct * mouseRender.implicitWidth - 9
                y: mouseRender.y + modelData.yPct * mouseRender.implicitHeight - 9
                width: 18; height: 18; radius: 9
                color: "transparent"
                border.color: "#814EFA"
                border.width: 2
                opacity: 0.7
            }
        }

        // ── Scroll wheel callout — right of scroll wheel circle (73%, 16%)
        InfoCallout {
            id: scrollCallout
            x: mouseRender.x + mouseRender.implicitWidth * 0.73 + 16
            y: mouseRender.y + mouseRender.implicitHeight * 0.16 - height / 2

            calloutType: "scrollwheel"
            title: "Scroll wheel"
            settings: [
                "Scroll direction: " + (DeviceModel.scrollInvert ? "Natural" : "Standard"),
                "Smooth scrolling: " + (DeviceModel.scrollHiRes ? "On" : "Off"),
                "SmartShift: " + (DeviceModel.smartShiftEnabled ? "On" : "Off")
            ]

            onCalloutClicked: function(type) {
                root.activePanelType = (root.activePanelType === type) ? "" : type
            }
        }

        // ── Thumb wheel callout — left of thumb wheel circle (55%, 51%)
        InfoCallout {
            id: thumbCallout
            x: mouseRender.x + mouseRender.implicitWidth * 0.55 - width - 16
            y: mouseRender.y + mouseRender.implicitHeight * 0.51 - height / 2

            calloutType: "thumbwheel"
            title: "Thumb wheel"
            settings: [
                "Speed: 50%",
                "Direction: Normal"
            ]

            onCalloutClicked: function(type) {
                root.activePanelType = (root.activePanelType === type) ? "" : type
            }
        }

        // ── Pointer speed callout — right of pointer speed circle (83%, 54%)
        InfoCallout {
            id: pointerCallout
            x: mouseRender.x + mouseRender.implicitWidth * 0.83 + 16
            y: mouseRender.y + mouseRender.implicitHeight * 0.54 - height / 2

            calloutType: "pointerspeed"
            title: "Pointer speed"
            settings: [
                "DPI: " + DeviceModel.currentDPI
            ]

            onCalloutClicked: function(type) {
                root.activePanelType = (root.activePanelType === type) ? "" : type
            }
        }

        // ── Click-outside-panel overlay ────────────────────────────────────────
        MouseArea {
            anchors.fill: parent
            enabled: root.activePanelType !== ""
            onClicked: root.activePanelType = ""
            // Transparent — just captures clicks to close the panel
        }
    }

    // ── Detail panel (slides in from the right) ────────────────────────────────
    DetailPanel {
        id: detailPanel
        anchors {
            top:    parent.top
            bottom: parent.bottom
        }

        // Slide in from right: when opened, right edge aligns to parent right edge;
        // when closed, push fully offscreen to the right.
        x: root.activePanelType !== "" ? parent.width - width : parent.width

        panelType: root.activePanelType
        opened:    root.activePanelType !== ""

        onCloseRequested: root.activePanelType = ""
    }
}
