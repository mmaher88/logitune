import QtQuick

// White card callout showing an action assignment for one mouse button.
// When selected: turns accent purple.
// Positioned absolutely relative to its parent (DeviceRender or ButtonsPage).
Item {
    id: root

    // ── Public API ─────────────────────────────────────────────────────────
    property string actionName: "Left click"
    property string buttonName: "Left click"
    property bool   selected:   false

    // Connector line endpoint (in parent coordinates)
    property real lineToX: 0
    property real lineToY: 0
    property bool showLine: true

    signal clicked()

    implicitWidth:  card.implicitWidth
    implicitHeight: card.implicitHeight

    // ── Connector line (Canvas) ────────────────────────────────────────────
    Canvas {
        id: lineCanvas
        // Cover the space between callout and mouse zone
        anchors.fill: parent
        visible: root.showLine

        onPaint: {
            var ctx = getContext("2d")
            ctx.clearRect(0, 0, width, height)

            // Compute callout center in parent coords
            var fromX = root.x + root.width / 2
            var fromY = root.y + root.height / 2

            // Draw from callout center to lineToX/lineToY (relative to parent)
            // We translate to canvas local coordinates
            var lx = root.lineToX - root.x
            var ly = root.lineToY - root.y

            ctx.beginPath()
            ctx.moveTo(root.width / 2, root.height / 2)
            ctx.lineTo(lx, ly)
            ctx.strokeStyle = root.selected ? "#7B61FF" : "#CCCCCC"
            ctx.lineWidth = 1.5
            ctx.setLineDash([4, 3])
            ctx.stroke()
        }

        // Repaint when selection changes
        Connections {
            target: root
            function onSelectedChanged() { lineCanvas.requestPaint() }
        }
    }

    // ── Card ───────────────────────────────────────────────────────────────
    Rectangle {
        id: card
        implicitWidth:  Math.max(contentCol.implicitWidth + 20, 100)
        implicitHeight: contentCol.implicitHeight + 16
        radius: 8
        color:  root.selected ? "#7B61FF" : "#FFFFFF"
        border.color: root.selected ? "#6B51EF" : "#E8E8E8"
        border.width: 1

        Behavior on color        { ColorAnimation { duration: 150 } }
        Behavior on border.color { ColorAnimation { duration: 150 } }

        // Subtle shadow simulation
        Rectangle {
            anchors {
                fill:         parent
                topMargin:    2
                leftMargin:   1
                rightMargin:  1
                bottomMargin: -2
            }
            radius:  card.radius
            color:   "transparent"
            border.color: Qt.rgba(0, 0, 0, 0.08)
            border.width: 1
            z: -1
        }

        Column {
            id: contentCol
            anchors {
                left:   parent.left
                right:  parent.right
                top:    parent.top
                margins: 10
            }
            spacing: 2

            // Action name (primary, bold)
            Text {
                text: root.actionName
                font.pixelSize: 12
                font.bold: true
                color: root.selected ? "#FFFFFF" : "#1A1A1A"
                width: parent.width
                elide: Text.ElideRight

                Behavior on color { ColorAnimation { duration: 150 } }
            }

            // Physical button name (secondary)
            Text {
                text: root.buttonName
                font.pixelSize: 10
                color: root.selected ? Qt.rgba(1,1,1,0.75) : "#888888"
                width: parent.width
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
