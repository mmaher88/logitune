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
            anchors.centerIn: parent
            anchors.horizontalCenterOffset: root.activePanelType !== "" ? -130 : 0
            Behavior on anchors.horizontalCenterOffset {
                NumberAnimation { duration: 300; easing.type: Easing.InOutCubic }
            }
        }

        // ── Scroll wheel callout — top-right ───────────────────────────────────
        InfoCallout {
            id: scrollCallout
            anchors {
                left: mouseRender.right
                leftMargin: 28
                top:  mouseRender.top
                topMargin: 10
            }
            width: 200

            calloutType: "scrollwheel"
            title: "Scroll wheel"
            settings: [
                "SmartShift: " + (DeviceModel.smartShiftEnabled ? "On" : "Off")
            ]

            onCalloutClicked: function(type) {
                root.activePanelType = (root.activePanelType === type) ? "" : type
            }
        }

        // ── Thumb wheel callout — left side ────────────────────────────────────
        InfoCallout {
            id: thumbCallout
            anchors {
                right:          mouseRender.left
                rightMargin:    28
                verticalCenter: mouseRender.verticalCenter
            }
            width: 150

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

        // ── Pointer speed callout — bottom-right ───────────────────────────────
        InfoCallout {
            id: pointerCallout
            anchors {
                left:   mouseRender.right
                leftMargin: 28
                bottom: mouseRender.bottom
                bottomMargin: 10
            }
            width: 180

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
