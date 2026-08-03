import QtQuick
import QtQuick.Controls
import Logitune

Item {
    Column {
        anchors { left: parent.left; top: parent.top; margins: 32 }
        spacing: 24
        width: 400

        Text {
            text: "Device"
            font { pixelSize: 22; bold: true }
            color: Theme.text
        }

        Column {
            spacing: 12
            width: parent.width

            Row {
                width: parent.width
                Text { text: "Device name"; width: 160; color: Theme.textSecondary; font.pixelSize: 13 }
                Text { text: DeviceModel.deviceName || "MX Master 3S"; color: Theme.text; font.pixelSize: 13 }
            }
            Row {
                width: parent.width
                Text { text: "Firmware version"; width: 160; color: Theme.textSecondary; font.pixelSize: 13 }
                Text { text: DeviceModel.firmwareVersion; color: Theme.text; font.pixelSize: 13 }
            }
            Row {
                width: parent.width
                Text { text: "Serial number"; width: 160; color: Theme.textSecondary; font.pixelSize: 13 }
                Text { text: DeviceModel.deviceSerial; color: Theme.text; font.pixelSize: 13 }
            }
            Row {
                width: parent.width
                Text { text: "Per-app profiles"; width: 160; color: Theme.textSecondary; font.pixelSize: 13 }
                Text { text: "Active"; color: Theme.accent; font.pixelSize: 13 }
            }
        }
    }
}
