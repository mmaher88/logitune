import QtQuick
import QtQuick.Controls
import Logitune

// Unified hotspot control — owns the marker circle, connector line, and callout card
// for a single button hotspot. Used in both production and editor modes.
//
// In production mode: everything positions from model data; DragHandlers disabled.
// In editor mode: marker and card are independently draggable; line tracks live.
Item {
    id: root

    // ── Input properties (set by parent page) ──
    property real imageX: 0
    property real imageY: 0
    property real imageW: 1
    property real imageH: 1

    property real hotspotXPct: 0
    property real hotspotYPct: 0
    property string side: "right"
    property real labelOffsetYPct: 0
    property bool configurable: true

    property string buttonName: ""
    property string actionName: ""
    property bool selected: false
    property int buttonId: -1
    property int hotspotIndex: -1

    property real pageWidth: 0
    property real pageHeight: 0

    signal clicked()

    visible: root.configurable

    // ── Computed marker centre ──
    readonly property real markerCenterX: imageX + hotspotXPct * imageW
    readonly property real markerCenterY: imageY + hotspotYPct * imageH

    // ── Computed card target ──
    readonly property real cardTargetX: side === "left"
        ? markerCenterX - cardItem.width - 24
        : markerCenterX + 24
    readonly property real cardTargetY: markerCenterY - cardItem.height / 2
        + labelOffsetYPct * imageH

    // ══════════════════════════════════════════════════════════════════════
    // MARKER
    // ══════════════════════════════════════════════════════════════════════
    Item {
        id: marker
        width: 24; height: 24
        x: root.markerCenterX - width / 2
        y: root.markerCenterY - height / 2

        Rectangle {
            anchors.centerIn: parent
            width: 18; height: 18; radius: 9
            color: "transparent"
            border.color: Theme.accent
            border.width: 2
            opacity: 0.7

            Behavior on opacity { NumberAnimation { duration: 200 } }

            Rectangle {
                anchors.centerIn: parent
                width: 6; height: 6; radius: 3
                color: Theme.accent
                opacity: 0.6
            }
        }

        MouseArea {
            anchors.centerIn: parent
            width: 0.22 * root.imageW
            height: 0.14 * root.imageH
            cursorShape: Qt.PointingHandCursor
            enabled: !markerDrag.enabled
            onClicked: root.clicked()
        }

        Connections {
            target: root
            function onMarkerCenterXChanged() {
                if (!markerDrag.active)
                    marker.x = root.markerCenterX - marker.width / 2
            }
            function onMarkerCenterYChanged() {
                if (!markerDrag.active)
                    marker.y = root.markerCenterY - marker.height / 2
            }
        }

        DragHandler {
            id: markerDrag
            enabled: typeof EditorModel !== 'undefined' && EditorModel.editing
            target: parent
            onActiveChanged: {
                if (!active) {
                    var cx = marker.x + marker.width / 2
                    var cy = marker.y + marker.height / 2
                    var xPct = (cx - root.imageX) / root.imageW
                    var yPct = (cy - root.imageY) / root.imageH
                    xPct = Math.max(0, Math.min(1, xPct))
                    yPct = Math.max(0, Math.min(1, yPct))
                    EditorModel.updateHotspot(root.hotspotIndex,
                        xPct, yPct, root.side, root.labelOffsetYPct)
                }
            }
        }
    }

    // ══════════════════════════════════════════════════════════════════════
    // CONNECTOR LINE
    // ══════════════════════════════════════════════════════════════════════
    Canvas {
        id: lineCanvas
        anchors.fill: parent

        onPaint: {
            var ctx = getContext("2d")
            ctx.clearRect(0, 0, width, height)

            if (!root.visible) return

            var mx = marker.x + marker.width / 2
            var my = marker.y + marker.height / 2

            var cardEdgeX, cardEdgeY
            if (cardItem.x + cardItem.width / 2 < mx) {
                cardEdgeX = cardItem.x + cardItem.width
            } else {
                cardEdgeX = cardItem.x
            }
            cardEdgeY = cardItem.y + cardItem.height / 2

            ctx.beginPath()
            ctx.moveTo(cardEdgeX, cardEdgeY)
            ctx.lineTo(mx, my)
            ctx.strokeStyle = root.selected ? Theme.accent : "#BBBBBB"
            ctx.lineWidth = 2
            ctx.setLineDash([])
            ctx.stroke()
        }

        Connections {
            target: marker
            function onXChanged() { lineCanvas.requestPaint() }
            function onYChanged() { lineCanvas.requestPaint() }
        }
        Connections {
            target: cardItem
            function onXChanged() { lineCanvas.requestPaint() }
            function onYChanged() { lineCanvas.requestPaint() }
        }
        Connections {
            target: root
            function onSelectedChanged() { lineCanvas.requestPaint() }
            function onVisibleChanged() { lineCanvas.requestPaint() }
        }
    }

    // ══════════════════════════════════════════════════════════════════════
    // CARD
    // ══════════════════════════════════════════════════════════════════════
    Item {
        id: cardItem
        width: cardRect.implicitWidth
        height: cardRect.implicitHeight

        // Initial position. Drags break this binding; Connections below and
        // the Qt.callLater in cardDrag.onActiveChanged restore position
        // imperatively afterwards.
        x: root.cardTargetX
        y: root.cardTargetY

        Connections {
            target: root
            function onCardTargetXChanged() {
                if (!cardDrag.active && !markerDrag.active)
                    cardItem.x = root.cardTargetX
            }
            function onCardTargetYChanged() {
                if (!cardDrag.active && !markerDrag.active)
                    cardItem.y = root.cardTargetY
            }
        }

        Rectangle {
            id: cardRect
            implicitWidth: Math.min(contentCol.implicitWidth + 24, 180)
            implicitHeight: contentCol.implicitHeight + 18
            radius: 8
            color: root.selected ? Theme.accent : Theme.cardBg
            border.color: root.selected ? Theme.accentHover : Theme.cardBorder
            border.width: 1

            Behavior on color { ColorAnimation { duration: 150 } }
            Behavior on border.color { ColorAnimation { duration: 150 } }

            // Drop shadow
            Rectangle {
                x: 4; y: 4
                width: parent.width; height: parent.height
                radius: parent.radius
                color: Qt.rgba(0, 0, 0, 0.1)
                z: -1
            }

            Column {
                id: contentCol
                x: 12; y: 9
                spacing: 2

                // Physical button name (primary, bold)
                EditableText {
                    id: nameLabel
                    width: 156; height: 16
                    text: root.buttonName
                    pixelSize: 12
                    fontWeight: Font.DemiBold
                    textColor: root.selected ? Theme.activeTabText
                        : (hoverHandler.hovered ? Theme.accent : Theme.text)
                    onCommit: function(v) {
                        var ctrls = DeviceModel.controlDescriptors
                        if (!ctrls) return
                        for (var i = 0; i < ctrls.length; ++i) {
                            if (ctrls[i].buttonId === root.buttonId) {
                                EditorModel.updateText("controlDisplayName", i, v)
                                return
                            }
                        }
                    }
                }

                // Action name (secondary)
                Text {
                    text: root.actionName
                    font.pixelSize: 10
                    color: root.selected ? Qt.rgba(1,1,1,0.75) : "#999999"
                    width: Math.min(implicitWidth, 156)
                    elide: Text.ElideRight
                    Behavior on color { ColorAnimation { duration: 150 } }
                }
            }

            // Hover overlay
            Rectangle {
                anchors.fill: parent
                radius: cardRect.radius
                color: hoverHandler.hovered && !root.selected
                    ? Qt.rgba(0, 0, 0, 0.04) : "transparent"
                Behavior on color { ColorAnimation { duration: 100 } }
            }

            HoverHandler { id: hoverHandler }

            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                propagateComposedEvents: true
                onClicked: root.clicked()
                onDoubleClicked: function(mouse) { mouse.accepted = false }
            }
        }

        DragHandler {
            id: cardDrag
            enabled: typeof EditorModel !== 'undefined' && EditorModel.editing
            target: parent
            onActiveChanged: {
                if (!active) {
                    var centroidX = cardItem.x + cardItem.width / 2
                    var newSide = centroidX < root.pageWidth / 2 ? "left" : "right"
                    var my = marker.y + marker.height / 2
                    var baseY = my - cardItem.height / 2
                    var newOffsetY = (cardItem.y - baseY) / (root.imageH > 0 ? root.imageH : 1)
                    EditorModel.updateHotspot(root.hotspotIndex,
                        root.hotspotXPct, root.hotspotYPct,
                        newSide, newOffsetY)
                    Qt.callLater(function() {
                        cardItem.x = root.cardTargetX
                        cardItem.y = root.cardTargetY
                    })
                }
            }
        }
    }
}
