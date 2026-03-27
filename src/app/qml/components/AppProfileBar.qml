import QtQuick
import Logitune

Row {
    spacing: 8
    height: 48

    Repeater {
        model: ProfileModel
        delegate: Item {
            width: 48
            height: 48

            // Icon container — 48x48 outer, 24x24 content centered
            Rectangle {
                anchors.fill: parent
                radius: 8
                color: model.isActive ? Qt.rgba(0.506, 0.306, 0.980, 0.06) : "transparent"

                // Icon content — 24x24 centered
                Text {
                    anchors.centerIn: parent
                    text: model.icon || "\u229E"
                    font.pixelSize: 20
                    width: 24; height: 24
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
            }

            // Selected indicator — 32px wide, 2px tall purple line below
            Rectangle {
                anchors.bottom: parent.bottom
                anchors.horizontalCenter: parent.horizontalCenter
                width: 32
                height: 2
                radius: 1
                color: Theme.accent
                visible: model.isActive
            }

            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                onClicked: ProfileModel.setActiveByIndex(model.index)
            }
        }
    }

    // Add button — 48x48 container
    Item {
        width: 48
        height: 48

        Rectangle {
            anchors.fill: parent
            radius: 8
            color: "transparent"

            Text {
                anchors.centerIn: parent
                text: "+"
                font.pixelSize: 22
                color: "#CCCCCC"
            }
        }

        MouseArea {
            anchors.fill: parent
            cursorShape: Qt.PointingHandCursor
            onClicked: {
                // TODO: open add-app dialog
            }
        }
    }
}
