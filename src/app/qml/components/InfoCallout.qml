import QtQuick
import QtQuick.Layouts
import Logitune

// Purple rounded callout card — summarises a group of settings.
// Click to open the corresponding DetailPanel.
Rectangle {
    id: root

    // ── Public API ──────────────────────────────────────────────────────────
    property string title: ""
    property var    settings: []     // list of strings shown as lines
    property string calloutType: ""  // "scrollwheel" | "thumbwheel" | "pointerspeed"

    signal calloutClicked(string type)

    // ── Geometry / Appearance ───────────────────────────────────────────────
    implicitWidth:  Math.min(Math.max(contentCol.implicitWidth + 32, 160), 220)
    implicitHeight: contentCol.implicitHeight + 24

    // Gradient from top to bottom — colors follow theme
    gradient: Gradient {
        orientation: Gradient.Vertical
        GradientStop { position: 0.0; color: Theme.calloutGradientTop }
        GradientStop { position: 1.0; color: Theme.calloutGradientBottom }
    }
    radius: 4

    // Slight brightness boost on hover
    Rectangle {
        anchors.fill: parent
        radius: parent.radius
        color: hoverHandler.hovered ? Qt.rgba(1, 1, 1, 0.08) : "transparent"
        Behavior on color { ColorAnimation { duration: 120 } }
    }

    // ── Content ─────────────────────────────────────────────────────────────
    Column {
        id: contentCol
        anchors {
            left:   parent.left
            right:  parent.right
            top:    parent.top
            margins: 12
        }
        spacing: 1

        // Title
        Text {
            text: root.title
            font.pixelSize: 14
            font.bold: true
            color: "#FFFFFF"
            width: parent.width
            elide: Text.ElideRight
        }

        // Settings lines
        Repeater {
            model: root.settings
            delegate: Text {
                text: modelData
                font.pixelSize: 13
                lineHeight: 1.1
                lineHeightMode: Text.ProportionalHeight
                color: "#FFFFFF"
                opacity: 0.85
                width: parent.width
                elide: Text.ElideRight
            }
        }
    }

    // ── Interaction ─────────────────────────────────────────────────────────
    HoverHandler { id: hoverHandler }

    MouseArea {
        anchors.fill: parent
        cursorShape: Qt.PointingHandCursor
        onClicked: root.calloutClicked(root.calloutType)
    }
}
