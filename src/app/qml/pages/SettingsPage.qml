import QtQuick
import QtQuick.Controls
import Logitune

Item {
    Column {
        anchors { left: parent.left; top: parent.top; margins: 32 }
        spacing: 24
        width: 400

        Text {
            text: "Settings"
            font { pixelSize: 22; bold: true }
            color: "#1A1A1A"
        }

        // Device info section
        Column {
            spacing: 12
            width: parent.width

            // Info rows
            Row {
                width: parent.width
                Text { text: "Device name"; width: 160; color: "#888888"; font.pixelSize: 13 }
                Text { text: DeviceModel.deviceName || "MX Master 3S"; color: "#1A1A1A"; font.pixelSize: 13 }
            }
            Row {
                width: parent.width
                Text { text: "Firmware version"; width: 160; color: "#888888"; font.pixelSize: 13 }
                Text { text: "12.00.11"; color: "#1A1A1A"; font.pixelSize: 13 }
            }
            Row {
                width: parent.width
                Text { text: "Serial number"; width: 160; color: "#888888"; font.pixelSize: 13 }
                Text { text: "XXXX-XXXX"; color: "#1A1A1A"; font.pixelSize: 13 }
            }
            Row {
                width: parent.width
                Text { text: "Per-app profiles"; width: 160; color: "#888888"; font.pixelSize: 13 }
                Text { text: "Active"; color: "#4CAF50"; font.pixelSize: 13 }
            }

            // Separator
            Rectangle { width: parent.width; height: 1; color: "#E0E0E0" }

            // Reset button
            Rectangle {
                width: 180; height: 40
                radius: 8; color: "transparent"
                border.color: "#E0E0E0"; border.width: 1

                Text {
                    anchors.centerIn: parent
                    text: "Reset to defaults"
                    font.pixelSize: 13; color: "#666666"
                }

                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: resetDialog.open()
                }
            }
        }
    }

    // Confirmation dialog
    Dialog {
        id: resetDialog
        title: "Reset to defaults?"
        modal: true
        anchors.centerIn: parent
        standardButtons: Dialog.Ok | Dialog.Cancel
    }
}
