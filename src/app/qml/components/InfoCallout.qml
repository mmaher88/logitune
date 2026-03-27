import QtQuick
import QtQuick.Layouts

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
    implicitWidth:  contentCol.implicitWidth  + 20
    implicitHeight: contentCol.implicitHeight + 14

    // Gradient from top (#A04EFA) to bottom (#814EFA)
    gradient: Gradient {
        orientation: Gradient.Vertical
        GradientStop { position: 0.0; color: "#A04EFA" }
        GradientStop { position: 1.0; color: "#814EFA" }
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
            margins: 10
        }
        spacing: 4

        // Title
        Text {
            text: root.title
            font.pixelSize: 12
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
                font.pixelSize: 11
                lineHeight: 1.6
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
