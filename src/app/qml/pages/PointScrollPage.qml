import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

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
        color: "#F5F5F5"
        radius: 12
    }

    // ── Canvas: device render + overlaid callouts ─────────────────────────────
    Item {
        id: canvas
        anchors {
            fill: parent
            rightMargin: root.activePanelType !== "" ? detailPanel.width : 0
        }
        Behavior on anchors.rightMargin {
            NumberAnimation { duration: 220; easing.type: Easing.OutCubic }
        }

        // ── Mouse render (placeholder rectangle styled as a mouse silhouette) ──
        Rectangle {
            id: mouseRender
            anchors.centerIn: parent
            width:  120
            height: 190
            radius: 55
            color:  "#DCDCDC"
            border.color: "#BBBBBB"
            border.width: 2

            // Scroll wheel indicator on the render
            Rectangle {
                anchors.horizontalCenter: parent.horizontalCenter
                y: 30
                width: 18
                height: 44
                radius: 9
                color: "#AAAAAA"
            }

            // Thumb wheel indicator on the left side of the render
            Rectangle {
                anchors.verticalCenter: parent.verticalCenter
                x: -12
                width: 10
                height: 36
                radius: 5
                color: "#AAAAAA"
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
            width: 160

            calloutType: "scrollwheel"
            title: "Scroll wheel"
            settings: [
                "Speed: 50%",
                "Direction: Natural",
                "Smooth scrolling: Off",
                "SmartShift: On"
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
            width: 160

            calloutType: "pointerspeed"
            title: "Pointer speed"
            settings: [
                "Speed: 50%"
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
            right:  parent.right
        }

        panelType: root.activePanelType
        opened:    root.activePanelType !== ""

        onCloseRequested: root.activePanelType = ""
    }
}
