import QtQuick
import QtQuick.Controls
import Logitune

Item {
    id: ring
    property var entries: []
    signal activated(int index)
    signal cancelled()

    Rectangle {
        anchors.centerIn: parent
        width: 96; height: 96; radius: 48
        color: Theme.surface
        border.color: Theme.border
        border.width: 1
        Text {
            anchors.centerIn: parent
            text: "Cancel"
            color: Theme.text
            font.pixelSize: 13
        }
        MouseArea {
            anchors.fill: parent
            cursorShape: Qt.PointingHandCursor
            onClicked: ring.cancelled()
        }
    }

    Repeater {
        model: ring.entries
        delegate: Rectangle {
            required property int index
            required property var modelData
            readonly property real angle: -Math.PI / 2 + index * 2 * Math.PI / Math.max(1, ring.entries.length)
            width: 112; height: 48; radius: 24
            x: ring.width / 2 + Math.cos(angle) * 132 - width / 2
            y: ring.height / 2 + Math.sin(angle) * 132 - height / 2
            color: hover.hovered ? Theme.accent : Theme.surface
            border.color: Theme.border
            border.width: 1
            Text {
                anchors.centerIn: parent
                width: parent.width - 18
                horizontalAlignment: Text.AlignHCenter
                elide: Text.ElideRight
                text: modelData.label
                color: hover.hovered ? "white" : Theme.text
                font.pixelSize: 12
            }
            HoverHandler { id: hover }
            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                onClicked: ring.activated(index)
            }
        }
    }
}
