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
                implicitWidth: 280
                implicitHeight: 414
                anchors.centerIn: parent
            }

            // ── Scroll hotspot data from device descriptor ───────────────────
            readonly property var scrollHotspotsData: DeviceModel.scrollHotspots

            function hotspotByKind(kind, fallbackIdx) {
                for (var i = 0; i < scrollHotspotsData.length; i++) {
                    if (scrollHotspotsData[i].kind === kind)
                        return scrollHotspotsData[i];
                }
                return scrollHotspotsData.length > fallbackIdx
                    ? scrollHotspotsData[fallbackIdx]
                    : null;
            }

            function hotspotIndexByKind(kind, fallbackIdx) {
                for (var i = 0; i < scrollHotspotsData.length; i++) {
                    if (scrollHotspotsData[i].kind === kind)
                        return i;
                }
                return scrollHotspotsData.length > fallbackIdx ? fallbackIdx : -1;
            }

            // ── Point & Scroll hotspot circles — driven by descriptor ────────
            Repeater {
                model: renderGroup.scrollHotspotsData.length
                Item {
                    id: scrollMarker
                    required property int index

                    // Read hotspot data from the live array by index — not stale modelData.
                    readonly property var hsData: DeviceModel.scrollHotspots[scrollMarker.index]
                    readonly property real targetX: mouseRender.x + mouseRender.paintedX + (hsData ? hsData.xPct : 0) * mouseRender.paintedW
                    readonly property real targetY: mouseRender.y + mouseRender.paintedY + (hsData ? hsData.yPct : 0) * mouseRender.paintedH

                    width: 24; height: 24
                    x: targetX - width / 2
                    y: targetY - height / 2

                    Rectangle {
                        anchors.centerIn: parent
                        width: 18; height: 18; radius: 9
                        color: "transparent"
                        border.color: Theme.accent
                        border.width: 2
                        opacity: 0.7
                    }

                    Connections {
                        target: DeviceModel
                        function onSelectedChanged() {
                            if (!scrollMarkerDrag.active) {
                                scrollMarker.x = scrollMarker.targetX - scrollMarker.width / 2
                                scrollMarker.y = scrollMarker.targetY - scrollMarker.height / 2
                            }
                        }
                    }

                    DragHandler {
                        id: scrollMarkerDrag
                        enabled: typeof EditorModel !== 'undefined' && EditorModel.editing
                        target: parent
                        onActiveChanged: {
                            if (!active) {
                                var cx = scrollMarker.x + scrollMarker.width / 2
                                var cy = scrollMarker.y + scrollMarker.height / 2
                                var xPct = (cx - mouseRender.x - mouseRender.paintedX) / mouseRender.paintedW
                                var yPct = (cy - mouseRender.y - mouseRender.paintedY) / mouseRender.paintedH
                                xPct = Math.max(0, Math.min(1, xPct))
                                yPct = Math.max(0, Math.min(1, yPct))
                                EditorModel.updateScrollHotspot(scrollMarker.index,
                                                                 xPct, yPct,
                                                                 scrollMarker.hsData ? scrollMarker.hsData.side : "right",
                                                                 scrollMarker.hsData ? scrollMarker.hsData.labelOffsetYPct : 0)
                            }
                        }
                    }
                }
            }

            // ── Scroll wheel callout — positioned from descriptor hotspot [0]
            InfoCallout {
                id: scrollCallout
                readonly property var hs: {
                    var d = renderGroup.scrollHotspotsData
                    if (!d) return null
                    for (var i = 0; i < d.length; i++)
                        if (d[i].kind === "scrollwheel") return d[i]
                    return d.length > 0 ? d[0] : null
                }
                readonly property int hsIdx: {
                    var d = renderGroup.scrollHotspotsData
                    if (!d) return -1
                    for (var i = 0; i < d.length; i++)
                        if (d[i].kind === "scrollwheel") return i
                    return d.length > 0 ? 0 : -1
                }
                readonly property real hsX: hs ? hs.xPct : 0.73
                readonly property real hsY: hs ? hs.yPct : 0.16
                readonly property real hsOffY: hs ? hs.labelOffsetYPct : 0
                targetX: mouseRender.x + mouseRender.paintedX + mouseRender.paintedW * scrollCallout.hsX + 16
                targetY: mouseRender.y + mouseRender.paintedY + mouseRender.paintedH * scrollCallout.hsY - scrollCallout.height / 2

                hotspotIndex: hsIdx
                hsXPct: hsX
                hsYPct: hsY
                hsLabelOffsetYPct: hsOffY
                pageWidth: renderGroup.width
                pageHeight: renderGroup.height

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
                readonly property var hs: {
                    var d = renderGroup.scrollHotspotsData
                    if (!d) return null
                    for (var i = 0; i < d.length; i++)
                        if (d[i].kind === "thumbwheel") return d[i]
                    return d.length > 1 ? d[1] : null
                }
                readonly property int hsIdx: {
                    var d = renderGroup.scrollHotspotsData
                    if (!d) return -1
                    for (var i = 0; i < d.length; i++)
                        if (d[i].kind === "thumbwheel") return i
                    return d.length > 1 ? 1 : -1
                }
                readonly property real hsX: hs ? hs.xPct : 0.55
                readonly property real hsY: hs ? hs.yPct : 0.51
                readonly property real hsOffY: hs ? hs.labelOffsetYPct : 0
                targetX: mouseRender.x + mouseRender.paintedX + mouseRender.paintedW * thumbCallout.hsX - thumbCallout.width - 16
                targetY: mouseRender.y + mouseRender.paintedY + mouseRender.paintedH * thumbCallout.hsY - thumbCallout.height / 2

                hotspotIndex: hsIdx
                hsXPct: hsX
                hsYPct: hsY
                hsLabelOffsetYPct: hsOffY
                pageWidth: renderGroup.width
                pageHeight: renderGroup.height

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
                readonly property var hs: {
                    var d = renderGroup.scrollHotspotsData
                    if (!d) return null
                    for (var i = 0; i < d.length; i++)
                        if (d[i].kind === "pointer") return d[i]
                    return d.length > 2 ? d[2] : null
                }
                readonly property int hsIdx: {
                    var d = renderGroup.scrollHotspotsData
                    if (!d) return -1
                    for (var i = 0; i < d.length; i++)
                        if (d[i].kind === "pointer") return i
                    return d.length > 2 ? 2 : -1
                }
                readonly property real hsX: hs ? hs.xPct : 0.83
                readonly property real hsY: hs ? hs.yPct : 0.54
                readonly property real hsOffY: hs ? hs.labelOffsetYPct : 0
                targetX: mouseRender.x + mouseRender.paintedX + mouseRender.paintedW * pointerCallout.hsX + 16
                targetY: mouseRender.y + mouseRender.paintedY + mouseRender.paintedH * pointerCallout.hsY - pointerCallout.height / 2

                hotspotIndex: hsIdx
                hsXPct: hsX
                hsYPct: hsY
                hsLabelOffsetYPct: hsOffY
                pageWidth: renderGroup.width
                pageHeight: renderGroup.height

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
