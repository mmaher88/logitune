import QtQuick
import Logitune

Item {
    id: root
    implicitWidth: 360
    implicitHeight: 360
    property var items: []
    property int selected: -1
    property bool interactive: false
    property int hovered: -1
    readonly property int activeIndex: interactive ? hovered : selected
    readonly property string activeLabel: activeIndex >= 0 && activeIndex < items.length
        ? qsTranslate("ActionModel", items[activeIndex].label) : qsTr("Radial menu")
    signal actionClicked(int index)

    Item {
        id: stage
        width: 360; height: 360
        anchors.centerIn: parent
        scale: Math.min(root.width / width, root.height / height)

        Repeater {
            model: root.items
            delegate: Item {
                id: bubble
                required property int index
                required property var modelData
                readonly property bool highlighted: root.activeIndex === index
                readonly property real angle: index * 2 * Math.PI / root.items.length - Math.PI / 2
                x: 180 + Math.cos(angle) * 100 - width / 2
                y: 155 + Math.sin(angle) * 100 - height / 2
                width: 54; height: 54
                scale: highlighted ? 1.12 : 1
                Behavior on scale { NumberAnimation { duration: 110; easing.type: Easing.OutCubic } }

                Rectangle {
                    anchors.centerIn: parent
                    width: 64; height: 64; radius: 32
                    color: "transparent"
                    border.width: 2
                    border.color: "#bfa0ff"
                    opacity: bubble.highlighted ? 0.9 : 0
                    Behavior on opacity { NumberAnimation { duration: 100 } }
                }
                Rectangle {
                    anchors.fill: parent
                    radius: width / 2
                    color: bubble.highlighted ? "#7951c6" : "#f018181b"
                    border.width: 1
                    border.color: bubble.highlighted ? "#ceb5ff" : "#66525259"
                    Behavior on color { ColorAnimation { duration: 100 } }
                    RadialActionIcon {
                        anchors.centerIn: parent
                        width: 24; height: 24
                        action: bubble.modelData.action
                    }
                }
                Accessible.role: Accessible.Button
                Accessible.name: qsTranslate("ActionModel", modelData.label)
                MouseArea {
                    anchors.fill: parent
                    enabled: root.interactive
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onEntered: root.hovered = bubble.index
                    onExited: if (root.hovered === bubble.index) root.hovered = -1
                    onClicked: root.actionClicked(bubble.index)
                }
            }
        }
        Rectangle {
            x: 165; y: 140
            width: 30; height: 30; radius: 15
            color: "#db29292e"
            border.width: 1
            border.color: "#55555c"
            Text {
                anchors.centerIn: parent
                text: "×"
                color: "#dedee5"
                font.pixelSize: 19
            }
        }
        Rectangle {
            anchors.horizontalCenter: parent.horizontalCenter
            y: 295
            width: Math.min(320, caption.implicitWidth + 32)
            height: 32
            radius: 8
            color: "#f018181b"
            Text {
                id: caption
                anchors.fill: parent
                anchors.leftMargin: 16; anchors.rightMargin: 16
                text: root.activeLabel
                color: "#ffffff"
                font.pixelSize: 13
                font.weight: Font.Medium
                verticalAlignment: Text.AlignVCenter
                horizontalAlignment: Text.AlignHCenter
                elide: Text.ElideRight
            }
        }
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            y: 334
            text: root.interactive ? qsTr("Click an icon to edit")
                  : root.activeIndex < 0 ? qsTr("Release to cancel") : qsTr("Release to run")
            color: root.interactive ? Theme.textSecondary : "#eeeeef"
            style: root.interactive ? Text.Normal : Text.Outline
            styleColor: "#b018181b"
            font.pixelSize: 11
        }
    }
}
