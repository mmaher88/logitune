import QtQuick
import QtQuick.Controls
import Logitune

Item {
    id: root
    implicitWidth: 360
    implicitHeight: 360
    property var items: []
    property int selected: -1
    property bool interactive: false
    property bool editing: true
    property int hovered: -1
    property bool inFolder: false
    property string title: qsTr("Action Ring")
    property Item dragLayer: root
    property int adjustmentIndex: -1
    property int adjustmentSteps: 0
    readonly property int activeIndex: interactive && hovered >= 0 ? hovered : selected
    onVisibleChanged: hovered = -1
    onItemsChanged: hovered = -1
    readonly property string activeLabel: activeIndex >= 0 && activeIndex < items.length
        ? (items[activeIndex].kind === "empty" ? qsTr("Empty slot") : qsTranslate("ActionModel", items[activeIndex].label)) : title
    signal actionClicked(int index)
    signal editRequested(int index)
    signal contextRequested(int index)
    signal closeRequested()
    signal entryDropped(int index, var entry, int sourceIndex)
    signal adjusted(int index, int steps)

    Item {
        id: stage
        width: root.editing ? 680 : 360; height: root.editing ? 560 : 360
        anchors.centerIn: parent
        scale: Math.min(root.width / width, root.height / height)

        Repeater {
            model: root.items
            delegate: Item {
                id: bubble
                objectName: "ringItem-" + index
                required property int index
                required property var modelData
                readonly property bool highlighted: root.activeIndex === index
                readonly property real angle: index * 2 * Math.PI / root.items.length - Math.PI / 2
                x: (root.editing ? 340 : 180) + Math.cos(angle) * (root.editing ? 134 : 100) - width / 2
                y: (root.editing ? 270 : 155) + Math.sin(angle) * (root.editing ? 134 : 100) - height / 2
                width: root.editing ? 64 : 54; height: width
                scale: highlighted && !root.editing ? 1.08 : 1
                Behavior on scale { NumberAnimation { duration: 110; easing.type: Easing.OutCubic } }

                Rectangle {
                    anchors.centerIn: parent
                    width: parent.width + 10; height: width; radius: width / 2
                    color: "transparent"
                    border.width: 2
                    border.color: Theme.accent
                    opacity: bubble.highlighted ? 0.9 : 0
                    Behavior on opacity { NumberAnimation { duration: 100 } }
                }
                Rectangle {
                    anchors.fill: parent
                    radius: width / 2
                    opacity: bubble.modelData.kind === "empty" ? (root.editing ? 1 : 0.22) : 1
                    color: root.editing ? (bubble.modelData.kind === "empty" ? Theme.cardBg : Theme.dark ? "#283735" : "#252334") : bubble.highlighted ? Theme.accent : Theme.surface
                    border.width: 1
                    border.color: bubble.highlighted ? Theme.accent : Theme.cardBorder
                    Behavior on color { ColorAnimation { duration: 100 } }
                    RadialActionIcon {
                        anchors.centerIn: parent
                        width: root.editing ? 28 : 24; height: width
                        action: bubble.modelData.action || "default"
                        applicationIcon: bubble.modelData.appIcon || ""
                        customIcon: root.editing && bubble.modelData.kind === "empty" ? "plus" : bubble.modelData.icon || (bubble.modelData.kind === "folder" ? "folder" : bubble.modelData.kind === "adjustment" ? bubble.modelData.parameter : "")
                        darkGlyph: root.editing ? (bubble.modelData.kind === "empty" && !Theme.dark) : bubble.highlighted ? Theme.dark : !Theme.dark
                    }
                }
                Rectangle {
                    id: labelChip
                    readonly property real direction: Math.cos(bubble.angle)
                    visible: root.editing && bubble.modelData.kind !== "empty"
                    width: Math.min(160, labelText.implicitWidth + 24); height: 38
                    x: direction > 0.3 ? bubble.width + 14 : direction < -0.3 ? -width - 14 : (bubble.width - width) / 2
                    y: Math.abs(direction) > 0.3 ? (bubble.height - height) / 2 : Math.sin(bubble.angle) < 0 ? -height - 14 : bubble.height + 14
                    radius: 4
                    color: Theme.surface
                    border.color: bubble.highlighted ? Theme.accent : Theme.border
                    Text {
                        id: labelText
                        anchors.fill: parent; anchors.leftMargin: 12; anchors.rightMargin: 12
                        text: qsTranslate("ActionModel", bubble.modelData.label)
                        color: Theme.text; font.pixelSize: 13; font.weight: Font.Medium
                        horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                        elide: Text.ElideRight
                    }
                    MouseArea {
                        anchors.fill: parent; cursorShape: Qt.PointingHandCursor; hoverEnabled: true
                        acceptedButtons: Qt.LeftButton | Qt.RightButton
                        onClicked: function(mouse) {
                            if (mouse.button === Qt.RightButton) root.contextRequested(bubble.index)
                            else root.actionClicked(bubble.index)
                        }
                        onDoubleClicked: root.editRequested(bubble.index)
                        ToolTip.visible: containsMouse && labelText.truncated
                        ToolTip.text: labelText.text
                    }
                }
                Rectangle {
                    visible: bubble.modelData.kind === "folder"
                    anchors.right: parent.right; anchors.bottom: parent.bottom
                    width: 20; height: 20; radius: 10
                    color: Theme.surface; border.color: Theme.cardBorder
                    RadialActionIcon { anchors.centerIn: parent; width: 12; height: 12; customIcon: "chevron"; rotation: -90 }
                }
                Accessible.role: Accessible.Button
                Accessible.name: bubble.modelData.kind === "empty" ? qsTr("Empty slot") : qsTranslate("ActionModel", modelData.label)
                Accessible.onPressAction: root.actionClicked(bubble.index)
                DropArea {
                    anchors.fill: parent
                    enabled: root.editing
                    keys: ["ring-action"]
                    onEntered: root.hovered = bubble.index
                    onExited: root.hovered = -1
                    onDropped: function(drop) {
                        if (drop.source) {
                            root.entryDropped(bubble.index, drop.source.ringEntry, drop.source.sourceIndex)
                            drop.acceptProposedAction()
                        }
                    }
                }
                Rectangle {
                    id: dragPreview
                    parent: root.dragLayer
                    width: 44; height: 44; radius: 22
                    color: Theme.accent
                    visible: pointer.drag.active
                    z: 1000
                    property var ringEntry: bubble.modelData
                    property int sourceIndex: bubble.index
                    Drag.active: pointer.drag.active
                    Drag.keys: ["ring-action"]
                    Drag.hotSpot.x: 22; Drag.hotSpot.y: 22
                    Drag.source: dragPreview
                    RadialActionIcon {
                        anchors.centerIn: parent; width: 22; height: 22
                        action: bubble.modelData.action || "default"
                        applicationIcon: bubble.modelData.appIcon || ""
                        customIcon: bubble.modelData.icon || (bubble.modelData.kind === "folder" ? "folder" : "")
                        darkGlyph: Theme.dark
                    }
                }
                MouseArea {
                    id: pointer
                    anchors.fill: parent
                    enabled: root.interactive
                    hoverEnabled: true
                    acceptedButtons: root.editing ? Qt.LeftButton | Qt.RightButton : Qt.LeftButton
                    cursorShape: Qt.PointingHandCursor
                    drag.target: root.editing ? dragPreview : null
                    property real lastAdjustmentX: 0
                    property bool adjustedDuringPress: false
                    property real wheelRemainder: 0
                    onEntered: root.hovered = bubble.index
                    onExited: if (root.hovered === bubble.index) root.hovered = -1
                    onPressed: function(mouse) {
                        lastAdjustmentX = mouse.x
                        adjustedDuringPress = false
                        var p = mapToItem(root.dragLayer, mouse.x, mouse.y)
                        dragPreview.x = p.x - 22; dragPreview.y = p.y - 22
                    }
                    onPositionChanged: function(mouse) {
                        if (!pressed || root.editing || bubble.modelData.kind !== "adjustment") return
                        var steps = Math.trunc((mouse.x - lastAdjustmentX) / 12)
                        if (steps) {
                            adjustedDuringPress = true
                            lastAdjustmentX += steps * 12
                            root.adjusted(bubble.index, steps)
                        }
                    }
                    onReleased: if (drag.active) dragPreview.Drag.drop()
                    onClicked: function(mouse) {
                        if (root.editing && mouse.button === Qt.RightButton) root.contextRequested(bubble.index)
                        else if (!adjustedDuringPress) root.actionClicked(bubble.index)
                    }
                    onDoubleClicked: if (root.editing) root.editRequested(bubble.index)
                    onWheel: function(wheel) {
                        if (!root.editing && bubble.modelData.kind === "adjustment") {
                            wheelRemainder += (wheel.angleDelta.y || wheel.angleDelta.x) / 120
                            var steps = Math.trunc(wheelRemainder)
                            if (steps) { root.adjusted(bubble.index, steps); wheelRemainder -= steps }
                        }
                        wheel.accepted = !root.editing
                    }
                }
            }
        }
        Rectangle {
            x: root.editing ? 323 : 165; y: root.editing ? 253 : 140
            width: root.editing ? 34 : 30; height: width; radius: width / 2
            visible: !root.editing || root.inFolder
            objectName: "ringClose"
            color: Theme.surface
            border.width: 1
            border.color: Theme.cardBorder
            RadialActionIcon {
                anchors.centerIn: parent; width: 16; height: 16
                customIcon: root.inFolder ? "back" : "close"
                opacity: root.editing && !root.inFolder ? 0.35 : 1
            }
            Accessible.role: Accessible.Button
            Accessible.name: root.inFolder ? qsTr("Back") : qsTr("Close menu")
            MouseArea {
                anchors.fill: parent
                enabled: root.interactive && (!root.editing || root.inFolder)
                cursorShape: Qt.PointingHandCursor
                onClicked: root.closeRequested()
            }
        }
        Rectangle {
            anchors.horizontalCenter: parent.horizontalCenter
            visible: !root.editing
            y: 295
            width: Math.min(320, caption.implicitWidth + 32)
            height: 32
            radius: 8
            color: Theme.surface
            Text {
                id: caption
                anchors.fill: parent
                anchors.leftMargin: 16; anchors.rightMargin: 16
                text: root.adjustmentIndex >= 0 && root.adjustmentIndex < root.items.length && !root.editing && (root.activeIndex < 0 || root.activeIndex === root.adjustmentIndex)
                    ? qsTranslate("ActionModel", root.items[root.adjustmentIndex].label) + "  " + (root.adjustmentSteps > 0 ? "+" : "") + root.adjustmentSteps
                    : root.activeLabel
                color: Theme.text
                font.pixelSize: 13
                font.weight: Font.Medium
                verticalAlignment: Text.AlignVCenter
                horizontalAlignment: Text.AlignHCenter
                elide: Text.ElideRight
            }
        }
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            visible: !root.editing && root.activeIndex >= 0 && root.items[root.activeIndex].kind === "adjustment"
            y: 334
            text: root.editing ? qsTr("Select a slot or drag an action here")
                : root.activeIndex >= 0 && root.items[root.activeIndex].kind === "adjustment"
                  ? qsTr("Scroll or drag sideways to adjust") : qsTr("Click an action to run")
            color: Theme.textSecondary
            style: root.editing ? Text.Normal : Text.Outline
            styleColor: Theme.background
            font.pixelSize: 11
        }
    }
}
