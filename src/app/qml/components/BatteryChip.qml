import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Logitune

Rectangle {
    id: chip
    width: chipRow.implicitWidth + 20
    height: 32
    radius: 16
    color: Theme.cardBg
    visible: chip.level > 0

    layer.enabled: true
    layer.effect: null  // Shadow approximated with border for now

    border.color: Theme.cardBorder
    border.width: 1

    // Drop shadow effect
    Rectangle {
        z: -1
        anchors {
            fill: parent
            topMargin: -1
            leftMargin: -1
            rightMargin: -1
            bottomMargin: -3
        }
        radius: parent.radius + 1
        color: "transparent"
        border.color: "#00000018"
        border.width: 1
    }

    // Battery level helpers
    readonly property int level: DeviceModel.batteryLevel
    readonly property bool charging: DeviceModel.batteryCharging
    readonly property string connType: DeviceModel.connectionType

    // Color: orange when <= 25%, green otherwise
    readonly property color battColor: level <= 25 ? Theme.batteryWarning : Theme.batteryGreen

    // Battery icon based on stepped levels
    readonly property string battIcon: {
        if (level <= 25) return "\uD83E\uDEAB";  // low — fallback to text steps
        if (level <= 50) return "\uD83D\uDD0B";
        if (level <= 75) return "\uD83D\uDD0B";
        return "\uD83D\uDD0B";
    }

    // Simple text-based battery representation
    readonly property string battStep: {
        if (level <= 25) return "[\u2581   ]";
        if (level <= 50) return "[\u2581\u2581  ]";
        if (level <= 75) return "[\u2581\u2581\u2581 ]";
        return "[\u2581\u2581\u2581\u2581]";
    }

    RowLayout {
        id: chipRow
        anchors.centerIn: parent
        spacing: 6

        // Connection type icon
        Text {
            text: chip.connType === "Bolt" ? "\u26A1" : "\uD83D\uDD35"
            font.pixelSize: 13
            color: "#999999"
            visible: chip.connType.length > 0
        }

        // Battery bar (simple colored rectangle representation)
        Row {
            spacing: 1

            Rectangle {
                width: 18; height: 10; radius: 1
                color: chip.level >= 1 ? chip.battColor : Theme.inputBg
            }
            Rectangle {
                width: 18; height: 10; radius: 1
                color: chip.level > 25 ? chip.battColor : Theme.inputBg
            }
            Rectangle {
                width: 18; height: 10; radius: 1
                color: chip.level > 50 ? chip.battColor : Theme.inputBg
            }
            Rectangle {
                width: 18; height: 10; radius: 1
                color: chip.level > 75 ? chip.battColor : Theme.inputBg
            }
            // Battery tip
            Rectangle {
                width: 3; height: 6; radius: 1
                color: "#CCCCCC"
                anchors.verticalCenter: parent.verticalCenter
            }
        }

        // Percentage text
        Text {
            text: chip.level > 0 ? chip.level + "%" : "--%"
            font.pixelSize: 12
            font.bold: true
            color: chip.level > 0 ? chip.battColor : "#999999"
        }

        // Charging indicator
        Text {
            text: "\u26A1"
            font.pixelSize: 11
            color: Theme.batteryWarning
            visible: chip.charging
        }
    }
}
