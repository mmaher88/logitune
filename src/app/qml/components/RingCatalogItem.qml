import QtQuick
import QtQuick.Controls
import Logitune

Rectangle {
    id: root
    property var entry: ({})
    property string subtitle: ""
    property Item dragLayer: root
    signal chosen(var entry)
    implicitHeight: 46
    radius: 5
    color: pointer.containsMouse ? Theme.hoverBg : Theme.surface
    border.color: Theme.cardBorder
    RadialActionIcon {
        x: 12; anchors.verticalCenter: parent.verticalCenter
        width: 22; height: 22
        action: root.entry.action || "default"
        applicationIcon: root.entry.appIcon || ""
        customIcon: root.entry.icon || (root.entry.kind === "folder" ? "folder" : root.entry.kind === "adjustment" ? root.entry.parameter : "")
    }
    Text {
        x: 46; anchors.verticalCenter: parent.verticalCenter
        width: parent.width - 58
        text: root.entry.label || ""; color: Theme.text; font.pixelSize: 13; elide: Text.ElideRight
    }
    ToolTip.visible: pointer.containsMouse && root.subtitle.length > 0 && !pointer.pressed
    ToolTip.delay: 700
    ToolTip.text: root.subtitle
    Rectangle {
        id: preview
        parent: root.dragLayer
        width: 44; height: 44; radius: 22
        z: 1000; color: Theme.accent; visible: pointer.drag.active
        property var ringEntry: root.entry
        property int sourceIndex: -1
        Drag.active: pointer.drag.active
        Drag.source: preview
        Drag.keys: ["ring-action"]
        Drag.hotSpot.x: 22; Drag.hotSpot.y: 22
        RadialActionIcon {
            anchors.centerIn: parent; width: 22; height: 22
            action: root.entry.action || "default"
            applicationIcon: root.entry.appIcon || ""
            customIcon: root.entry.icon || (root.entry.kind === "folder" ? "folder" : "")
            darkGlyph: Theme.dark
        }
    }
    Accessible.role: Accessible.Button
    Accessible.name: root.entry.label || ""
    Accessible.onPressAction: root.chosen(root.entry)
    MouseArea {
        id: pointer
        anchors.fill: parent; hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        drag.target: preview
        onPressed: function(mouse) {
            var p = mapToItem(root.dragLayer, mouse.x, mouse.y)
            preview.x = p.x - 22; preview.y = p.y - 22
        }
        onReleased: if (drag.active) preview.Drag.drop()
        onClicked: root.chosen(root.entry)
    }
}
