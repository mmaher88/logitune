import QtQuick
import Logitune

// Dark callout card showing an action assignment for one mouse button (Options+ style).
// Default: dark gray card with white text.
// Selected: accent purple card with white text.
// Connector line runs from the card edge to the hotspot circle.
Item {
    id: root

    // ── Public API ─────────────────────────────────────────────────────────
    property string actionName: "Middle click"
    property string buttonName: "Middle button"
    property bool   selected:   false

    // Connector line endpoint (in parent coordinates)
    property real lineToX: 0
    property real lineToY: 0
    property bool showLine: true

    // "left" = label is left of hotspot, "right" = label is right of hotspot
    property string lineSide: "right"

    signal clicked()

    implicitWidth:  card.implicitWidth
    implicitHeight: card.implicitHeight

    // ── Connector line (Canvas) ────────────────────────────────────────────
    Canvas {
        id: lineCanvas

        // The canvas must cover the area between the card and the hotspot.
        // We extend well beyond the card so the line can reach the hotspot.
        x: -300
        y: -300
        width:  600 + root.width
        height: 600 + root.height

        visible: root.showLine

        onPaint: {
            var ctx = getContext("2d")
            ctx.clearRect(0, 0, width, height)

            // Card edge point (in canvas-local coords)
            // Canvas origin is at (root.x - 300, root.y - 300) in parent space
            var cardEdgeX, cardEdgeY
            if (root.lineSide === "left") {
                // Line from right edge of card to hotspot (which is to the right)
                cardEdgeX = 300 + root.width
                cardEdgeY = 300 + root.height / 2
            } else {
                // Line from left edge of card to hotspot (which is to the left)
                cardEdgeX = 300
                cardEdgeY = 300 + root.height / 2
            }

            // Hotspot in canvas-local coords
            var hx = root.lineToX - root.x + 300
            var hy = root.lineToY - root.y + 300

            ctx.beginPath()
            ctx.moveTo(cardEdgeX, cardEdgeY)
            ctx.lineTo(hx, hy)
            ctx.strokeStyle = root.selected ? Theme.accent : "#BBBBBB"
            ctx.lineWidth = 2
            ctx.setLineDash([])
            ctx.stroke()
        }

        Connections {
            target: root
            function onSelectedChanged() { lineCanvas.requestPaint() }
            function onXChanged()        { lineCanvas.requestPaint() }
            function onYChanged()        { lineCanvas.requestPaint() }
        }
    }

    // ── Card ───────────────────────────────────────────────────────────────
    Rectangle {
        id: card
        implicitWidth:  Math.min(contentCol.implicitWidth + 24, 180)
        implicitHeight: contentCol.implicitHeight + 18
        radius: 8
        color:  root.selected ? Theme.accent : Theme.cardBg
        border.color: root.selected ? Theme.accentHover : Theme.cardBorder
        border.width: 1

        Behavior on color        { ColorAnimation { duration: 150 } }
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
            x: 12
            y: 9
            spacing: 2

            // Action name (primary, bold)
            Text {
                text: root.actionName
                font.pixelSize: 12
                font.weight: Font.DemiBold
                color: root.selected ? Theme.activeTabText : (hoverHandler.hovered ? Theme.accent : Theme.text)
                width: Math.min(implicitWidth, 156)
                elide: Text.ElideRight

                Behavior on color { ColorAnimation { duration: 150 } }
            }

            // Physical button name (secondary)
            Text {
                text: root.buttonName
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
            radius: card.radius
            color: hoverHandler.hovered && !root.selected
                   ? Qt.rgba(0, 0, 0, 0.04) : "transparent"
            Behavior on color { ColorAnimation { duration: 100 } }
        }

        HoverHandler { id: hoverHandler }

        MouseArea {
            anchors.fill: parent
            cursorShape: Qt.PointingHandCursor
            onClicked: root.clicked()
        }
    }
}
