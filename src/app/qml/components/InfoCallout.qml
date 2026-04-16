import QtQuick
import QtQuick.Layouts
import Logitune

// Callout card for Point & Scroll page — matches ButtonCallout style.
// Shows a title and summary settings lines. Click to open DetailPanel.
Item {
    id: root

    // ── Public API ──────────────────────────────────────────────────────────
    property string title: ""
    property var    settings: []     // list of strings shown as lines
    property string calloutType: ""  // "scrollwheel" | "thumbwheel" | "pointerspeed"

    // Editor-mode drag support (see PointScrollPage wiring)
    property int  hotspotIndex: -1
    property real hsXPct: 0
    property real hsYPct: 0
    property real hsLabelOffsetYPct: 0
    property real pageWidth: 0
    property real pageHeight: 0

    // Target position set by the parent page.
    // No x/y binding — DragHandler severs bindings on first drag.
    // Connections below imperatively sync position when target changes.
    property real targetX: 0
    property real targetY: 0

    Connections {
        target: root
        function onTargetXChanged() {
            if (!cardDrag.active)
                root.x = root.targetX
        }
        function onTargetYChanged() {
            if (!cardDrag.active)
                root.y = root.targetY
        }
    }

    Component.onCompleted: {
        root.x = root.targetX
        root.y = root.targetY
    }

    signal calloutClicked(string type)

    implicitWidth:  card.implicitWidth
    implicitHeight: card.implicitHeight

    // ── Card ────────────────────────────────────────────────────────────────
    Rectangle {
        id: card
        implicitWidth:  Math.min(Math.max(contentCol.implicitWidth + 24, 160), 220)
        implicitHeight: contentCol.implicitHeight + 18
        radius: 8
        color: Theme.cardBg
        border.color: Theme.cardBorder
        border.width: 1

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

            // Title (bold, accent on hover)
            Text {
                text: root.title
                font.pixelSize: 12
                font.weight: Font.DemiBold
                color: hoverHandler.hovered ? Theme.accent : Theme.text
                width: Math.min(implicitWidth, 196)
                elide: Text.ElideRight

                Behavior on color { ColorAnimation { duration: 150 } }
            }

            // Settings lines
            Repeater {
                model: root.settings
                delegate: Text {
                    text: modelData
                    font.pixelSize: 10
                    color: Theme.textSecondary
                    width: Math.min(implicitWidth, 196)
                    elide: Text.ElideRight
                }
            }
        }

        // Hover overlay
        Rectangle {
            anchors.fill: parent
            radius: card.radius
            color: hoverHandler.hovered ? Qt.rgba(0, 0, 0, 0.04) : "transparent"
            Behavior on color { ColorAnimation { duration: 100 } }
        }

        HoverHandler { id: hoverHandler }

        MouseArea {
            anchors.fill: parent
            cursorShape: Qt.PointingHandCursor
            onClicked: root.calloutClicked(root.calloutType)
        }
    }

    // Editor-mode drag: translate root Item freely, on release persist position.
    DragHandler {
        id: cardDrag
        enabled: typeof EditorModel !== 'undefined' && EditorModel.editing
        target: root

        onActiveChanged: {
            if (!active) {
                if (root.pageWidth > 0 && root.pageHeight > 0) {
                    var centroidX = root.x + root.width / 2
                    var newSide = centroidX < root.pageWidth / 2 ? "left" : "right"
                    var newOffsetY = root.hsLabelOffsetYPct
                    EditorModel.updateScrollHotspot(root.hotspotIndex,
                                                     root.hsXPct, root.hsYPct,
                                                     newSide, newOffsetY)
                    Qt.callLater(function() {
                        root.x = root.targetX
                        root.y = root.targetY
                    })
                }
            }
        }
    }
}
