import QtQuick
import QtQuick.Controls
import Logitune

Item {
    id: root

    Rectangle {
        anchors.fill: parent
        color: Theme.background
    }

    Column {
        anchors.centerIn: parent
        spacing: 24

        // Back view of the mouse showing Easy-Switch buttons
        Image {
            width: 280
            height: 109  // 1872x728 aspect ratio scaled
            anchors.horizontalCenter: parent.horizontalCenter
            source: "qrc:/Logitune/qml/assets/mx-master-3s-back.png"
            fillMode: Image.PreserveAspectFit
            smooth: true
            mipmap: true
        }

        Text {
            text: "Easy-Switch"
            font { pixelSize: 22; bold: true }
            color: Theme.text
            anchors.horizontalCenter: parent.horizontalCenter
        }
        Text {
            text: "Switch between up to 3 devices"
            font.pixelSize: 13
            color: Theme.textSecondary
            anchors.horizontalCenter: parent.horizontalCenter
        }

        Row {
            spacing: 16
            anchors.horizontalCenter: parent.horizontalCenter

            Repeater {
                model: 3
                delegate: Rectangle {
                    width: 160; height: 100
                    radius: 12
                    color: index === 0 ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.06) : Theme.cardBg
                    border.color: index === 0 ? Theme.accent : Theme.border
                    border.width: index === 0 ? 3 : 1

                    Column {
                        anchors.centerIn: parent
                        spacing: 6

                        Text {
                            text: (index + 1).toString()
                            font { pixelSize: 22; bold: true }
                            color: index === 0 ? Theme.accent : Theme.textSecondary
                            anchors.horizontalCenter: parent.horizontalCenter
                        }
                        Text {
                            text: index === 0 ? "Active" : "Available"
                            font.pixelSize: 11
                            color: index === 0 ? Theme.accent : Theme.textSecondary
                            anchors.horizontalCenter: parent.horizontalCenter
                        }
                        Text {
                            text: index === 0 ? "Bolt" : "\u2014"
                            font.pixelSize: 10
                            color: "#AAAAAA"
                            anchors.horizontalCenter: parent.horizontalCenter
                        }
                    }
                }
            }
        }

        Text {
            text: "Press the Easy-Switch button on your mouse to change channels"
            font.pixelSize: 11
            color: Theme.textSecondary
            anchors.horizontalCenter: parent.horizontalCenter
        }
    }
}
