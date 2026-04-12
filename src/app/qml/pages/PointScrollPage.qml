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
        color: Theme.background
    }

    // ── Dismiss panel by clicking background ─────────────────────────────────
    MouseArea {
        anchors.fill: parent
        enabled: root.activePanelType !== ""
        onClicked: root.activePanelType = ""
    }

    // ── Render area (shrinks when panel opens, same as ButtonsPage) ──────────
    Item {
        id: canvas
        anchors {
            left:   parent.left
            top:    parent.top
            bottom: parent.bottom
            right:  detailPanel.left
        }

        // ── Mouse + callouts group ──
        Item {
            id: renderGroup
            width: mouseRender.implicitWidth + 460  // match ButtonsPage: room for callouts
            height: 414
            anchors.verticalCenter: parent.verticalCenter

            // Scale down when available space is tight
            readonly property real fitScale: Math.min(1.0, Math.max(0.55, canvas.width / width))
            scale: fitScale
            transformOrigin: Item.Center

            Behavior on scale {
                NumberAnimation { duration: 300; easing.type: Easing.InOutCubic }
            }

            // Centre horizontally
            x: (parent.width - width) / 2

            Behavior on x {
                NumberAnimation { duration: 300; easing.type: Easing.InOutCubic }
            }

            DeviceRender {
                id: mouseRender
                imageSource: DeviceModel.sideImage
                showHotspots: false
                implicitWidth: 280
                implicitHeight: 414
                anchors.centerIn: parent
            }

            // ── Scroll hotspot data from device descriptor ───────────────────
            readonly property var scrollHotspotsData: DeviceModel.scrollHotspots

            // ── Point & Scroll hotspot circles — driven by descriptor ────────
            Repeater {
                model: renderGroup.scrollHotspotsData
                Rectangle {
                    x: mouseRender.x + mouseRender.paintedX + modelData.xPct * mouseRender.paintedW - 9
                    y: mouseRender.y + mouseRender.paintedY + modelData.yPct * mouseRender.paintedH - 9
                    width: 18; height: 18; radius: 9
                    color: "transparent"
                    border.color: Theme.accent
                    border.width: 2
                    opacity: 0.7
                }
            }

            // ── Scroll wheel callout — positioned from descriptor hotspot [0]
            InfoCallout {
                id: scrollCallout
                readonly property var hs: renderGroup.scrollHotspotsData.length > 0 ? renderGroup.scrollHotspotsData[0] : null
                readonly property real hsX: hs ? hs.xPct : 0.73
                readonly property real hsY: hs ? hs.yPct : 0.16
                x: mouseRender.x + mouseRender.paintedX + mouseRender.paintedW * hsX + 16
                y: mouseRender.y + mouseRender.paintedY + mouseRender.paintedH * hsY - height / 2

                calloutType: "scrollwheel"
                title: "Scroll wheel"
                settings: {
                    var s = [
                        "Scroll direction: " + (DeviceModel.scrollInvert ? "Natural" : "Standard")
                    ];
                    if (DeviceModel.smoothScrollSupported)
                        s.push("Smooth scrolling: " + (DeviceModel.scrollHiRes ? "On" : "Off"));
                    s.push("SmartShift: " + (DeviceModel.smartShiftEnabled ? "On" : "Off"));
                    return s;
                }

                onCalloutClicked: function(type) {
                    root.activePanelType = (root.activePanelType === type) ? "" : type
                }
            }

            // ── Thumb wheel callout — positioned from descriptor hotspot [1]
            InfoCallout {
                id: thumbCallout
                readonly property var hs: renderGroup.scrollHotspotsData.length > 1 ? renderGroup.scrollHotspotsData[1] : null
                readonly property real hsX: hs ? hs.xPct : 0.55
                readonly property real hsY: hs ? hs.yPct : 0.51
                x: mouseRender.x + mouseRender.paintedX + mouseRender.paintedW * hsX - width - 16
                y: mouseRender.y + mouseRender.paintedY + mouseRender.paintedH * hsY - height / 2

                calloutType: "thumbwheel"
                title: "Thumb wheel"
                settings: [
                    "Speed: 50%",
                    "Direction: " + (DeviceModel.thumbWheelInvert ? "Inverted" : "Normal")
                ]

                onCalloutClicked: function(type) {
                    root.activePanelType = (root.activePanelType === type) ? "" : type
                }
            }

            // ── Pointer speed callout — positioned from descriptor hotspot [2]
            InfoCallout {
                id: pointerCallout
                readonly property var hs: renderGroup.scrollHotspotsData.length > 2 ? renderGroup.scrollHotspotsData[2] : null
                readonly property real hsX: hs ? hs.xPct : 0.83
                readonly property real hsY: hs ? hs.yPct : 0.54
                x: mouseRender.x + mouseRender.paintedX + mouseRender.paintedW * hsX + 16
                y: mouseRender.y + mouseRender.paintedY + mouseRender.paintedH * hsY - height / 2

                calloutType: "pointerspeed"
                title: "Pointer speed"
                settings: [
                    "DPI: " + DeviceModel.currentDPI
                ]

                onCalloutClicked: function(type) {
                    root.activePanelType = (root.activePanelType === type) ? "" : type
                }
            }
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
