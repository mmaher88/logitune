import QtQuick
import QtQuick.Controls
import QtQuick.Window
import Logitune

Item {
    id: root
    required property var editor
    required property int slot
    property int child: -1
    required property var entry
    property real bubbleSize: 48
    readonly property bool selected: editor.selectedSlot === slot && editor.selectedChild === child
    width: bubbleSize; height: bubbleSize
    objectName: "bubble-" + slot + "-" + child
    DropArea {
        anchors.fill: parent
        keys: ["ring-action"]
        onDropped: drop => {
            if (drop.source && drop.source.ringSource)
                drop.accepted = root.editor.drop(drop.source.ringSource, root.slot, root.child)
        }
        RingButton {
            id: button
            centre: Qt.point(root.width / 2, root.height / 2)
            size: root.bubbleSize
            iconSize: size * 0.42
            icon: root.entry.icon || "+"
            hasSubmenu: root.entry.children !== undefined
            border.color: root.selected || parent.containsDrag ? Theme.accent : Theme.border
            border.width: root.selected || parent.containsDrag ? 3 : 1
            theme: ({surface: Theme.cardBg, accent: Theme.accent, text: Theme.text,
                onAccent: Theme.activeTabText, border: Theme.border})
            property var ringSource: ({slot: root.slot, child: root.child})
            Drag.active: drag.active
            Drag.source: button
            Drag.keys: ["ring-action"]
            Drag.hotSpot: Qt.point(width / 2, height / 2)
            TapHandler { onTapped: root.editor.select(root.slot, root.child) }
            DragHandler {
                id: drag
                enabled: root.entry.action !== undefined || root.entry.children !== undefined
                onActiveChanged: {
                    if (active) {
                        let layer = Overlay.overlay || root.Window.window.contentItem
                        let point = button.mapToItem(layer, 0, 0)
                        button.parent = layer; button.x = point.x; button.y = point.y
                    } else {
                        button.Drag.drop(); button.parent = root.children[0]
                        button.x = 0; button.y = 0
                    }
                }
            }
        }
    }
}
