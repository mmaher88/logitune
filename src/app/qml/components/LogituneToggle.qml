import QtQuick
import Logitune

// Logitune toggle switch — accent ON, gray OFF
// Track: 28×16px, radius 8. Knob: 12×12px, 2px offset from edge.
Item {
    id: root

    property bool   checked: false
    property string label:   ""

    signal toggled(bool checked)

    implicitWidth:  row.implicitWidth
    implicitHeight: row.implicitHeight

    Row {
        id: row
        spacing: 10

        Text {
            anchors.verticalCenter: parent.verticalCenter
            text: root.label
            font.pixelSize: 12
            color: "#444444"
            visible: root.label.length > 0
        }

        Rectangle {
            id: track
            width:  28
            height: 16
            radius: 8
            color:  root.checked ? Theme.accent : Theme.inputBg

            Behavior on color { ColorAnimation { duration: 150 } }

            Rectangle {
                id: knob
                width:  12
                height: 12
                radius: 6
                color:  "#FFFFFF"
                anchors.verticalCenter: parent.verticalCenter
                x: root.checked ? parent.width - width - 2 : 2

                Behavior on x { NumberAnimation { duration: 150; easing.type: Easing.InOutQuad } }
            }

            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                onClicked: {
                    root.checked = !root.checked
                    root.toggled(root.checked)
                }
            }
        }
    }
}
