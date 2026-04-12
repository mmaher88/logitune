import QtQuick
import Logitune

Item {
    id: root

    required property int index
    required property string deviceId
    required property string deviceName
    required property string frontImage
    required property int batteryLevel
    required property bool batteryCharging
    required property string connectionType
    required property string status
    required property bool isSelected

    signal clicked()

    width: 180; height: 280

    Image {
        id: deviceImg
        anchors.centerIn: parent
        width: parent.width - 20
        height: parent.height - 60
        source: root.frontImage
        fillMode: Image.PreserveAspectFit
        smooth: true; mipmap: true

        opacity: root.status === "placeholder" ? 0.4 : 1.0
    }

    Rectangle {
        anchors { top: parent.top; right: parent.right; margins: -4 }
        width: 22; height: 22; radius: 11
        z: 2
        border { width: 2; color: Theme.background }

        color: {
            switch (root.status) {
            case "implemented": return "#22c55e";
            case "community-verified": return "#3b82f6";
            case "community-local": return "#f59e0b";
            default: return "#666";
            }
        }

        Text {
            anchors.centerIn: parent
            font.pixelSize: 12; font.bold: true
            color: root.status === "community-local" ? "#222" : "#fff"
            text: {
                switch (root.status) {
                case "implemented": return "\u2713";
                case "community-verified": return "\u2605";
                case "community-local": return "\u270E";
                default: return "?";
                }
            }
        }
    }

    Text {
        anchors { top: deviceImg.bottom; topMargin: 8; horizontalCenter: parent.horizontalCenter }
        text: root.deviceName
        font { pixelSize: root.isSelected ? 13 : 11; bold: root.isSelected }
        color: root.status === "placeholder" ? "#666" : (root.isSelected ? Theme.text : "#888")
    }

    Row {
        anchors { top: deviceImg.bottom; topMargin: 26; horizontalCenter: parent.horizontalCenter }
        spacing: 6
        visible: root.isSelected && root.status !== "placeholder"

        Text {
            text: root.batteryLevel + "%"
            font.pixelSize: 11
            color: root.batteryLevel > 20 ? Theme.batteryGreen : Theme.batteryWarning
        }
        Text {
            text: "\u00B7 " + root.connectionType
            font.pixelSize: 10
            color: Theme.textSecondary
        }
    }

    Text {
        anchors { top: deviceImg.bottom; topMargin: 26; horizontalCenter: parent.horizontalCenter }
        visible: root.status === "placeholder"
        text: "Setup needed"
        font.pixelSize: 9
        color: "#666"
    }

    MouseArea {
        anchors.fill: parent
        cursorShape: Qt.PointingHandCursor
        onClicked: root.clicked()
    }
}
