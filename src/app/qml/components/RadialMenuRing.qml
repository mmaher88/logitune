pragma ComponentBehavior: Bound
import QtQuick
import Logitune

Item {
    id: ring
    property var entries: []
    property bool editing: false
    signal activated(int index)
    signal cancelled()
    signal actionClicked(int index)

    Rectangle {
        anchors.centerIn: parent
        width: 96; height: 96; radius: 48
        color: Theme.surface
        border.color: Theme.border
        border.width: 1
        Text {
            anchors.centerIn: parent
            text: ring.editing ? qsTr("Action Ring") : qsTr("Cancel")
            color: Theme.text
            font.pixelSize: 13
        }
        MouseArea {
            anchors.fill: parent
            enabled: !ring.editing
            cursorShape: Qt.PointingHandCursor
            onClicked: ring.cancelled()
        }
    }

    Repeater {
        model: ring.entries
        delegate: Rectangle {
            id: entryDelegate
            required property int index
            required property var modelData
            readonly property real angle: -Math.PI / 2 + entryDelegate.index * 2 * Math.PI
                / Math.max(1, ring.entries.length)
            width: 112; height: 54; radius: 27
            x: ring.width / 2 + Math.cos(angle) * 132 - width / 2
            y: ring.height / 2 + Math.sin(angle) * 132 - height / 2
            color: hover.hovered ? Theme.accent : Theme.surface
            border.color: Theme.border
            border.width: 1
            RadialActionIcon {
                anchors { left: parent.left; leftMargin: 12; verticalCenter: parent.verticalCenter }
                width: 20; height: 20
                action: entryDelegate.modelData.action
                color: hover.hovered ? "white" : Theme.text
            }
            Text {
                anchors { left: parent.left; leftMargin: 38; right: parent.right; rightMargin: 10; verticalCenter: parent.verticalCenter }
                horizontalAlignment: Text.AlignHCenter
                elide: Text.ElideRight
                text: entryDelegate.modelData.label
                color: hover.hovered ? "white" : Theme.text
                font.pixelSize: 12
            }
            HoverHandler { id: hover }
            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                onClicked: {
                    if (ring.editing) ring.actionClicked(entryDelegate.index)
                    else ring.activated(entryDelegate.index)
                }
            }
        }
    }
}
