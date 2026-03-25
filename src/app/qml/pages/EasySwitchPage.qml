import QtQuick
import QtQuick.Controls

Item {
    Column {
        anchors.centerIn: parent
        spacing: 24

        Text {
            text: "Easy-Switch"
            font { pixelSize: 22; bold: true }
            color: "#1A1A1A"
        }
        Text {
            text: "Switch between up to 3 devices"
            font.pixelSize: 13
            color: "#666666"
        }

        Row {
            spacing: 16

            // 3 channel cards
            Repeater {
                model: 3
                delegate: Rectangle {
                    width: 160; height: 120
                    radius: 12
                    color: "#FFFFFF"
                    border.color: index === 0 ? "#7B61FF" : "#E0E0E0"
                    border.width: index === 0 ? 2 : 1

                    Column {
                        anchors.centerIn: parent
                        spacing: 8

                        Text {
                            text: (index + 1).toString()
                            font { pixelSize: 24; bold: true }
                            color: index === 0 ? "#7B61FF" : "#666666"
                            anchors.horizontalCenter: parent.horizontalCenter
                        }
                        Text {
                            text: index === 0 ? "Active" : "Available"
                            font.pixelSize: 11
                            color: index === 0 ? "#7B61FF" : "#999999"
                            anchors.horizontalCenter: parent.horizontalCenter
                        }
                        Text {
                            text: index === 0 ? "Bluetooth" : "—"
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
            color: "#999999"
        }
    }
}
