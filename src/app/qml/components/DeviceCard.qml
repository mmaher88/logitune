import QtQuick
import QtQuick.Controls
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

        opacity: root.status === "beta" ? 0.8 : 1.0
    }

    Rectangle {
        id: statusBadge
        anchors { top: parent.top; right: parent.right; margins: -4 }
        width: 22; height: 22; radius: 11
        z: 2
        border { width: 2; color: Theme.background }

        color: {
            switch (root.status) {
            case "verified": return "#22c55e";
            default:         return "#f59e0b";
            }
        }

        Text {
            anchors.centerIn: parent
            font.pixelSize: 12; font.bold: true
            color: "#fff"
            text: {
                switch (root.status) {
                case "verified": return "\u2713";
                default:         return "\u03B2";
                }
            }
        }

        HoverHandler { id: badgeHover }

        ToolTip.visible: badgeHover.hovered
        ToolTip.delay: 500
        ToolTip.text: root.status === "verified"
            ? qsTr("Verified on real hardware by the Logitune team. Full feature support.")
            : qsTr("Beta descriptor — added but not yet verified on physical hardware. Core features should work; some may be untested.")
    }

    Text {
        anchors { top: deviceImg.bottom; topMargin: 8; horizontalCenter: parent.horizontalCenter }
        text: root.deviceName
        font { pixelSize: root.isSelected ? 13 : 11; bold: root.isSelected }
        color: root.isSelected ? Theme.text : "#888"
    }

    Row {
        anchors { top: deviceImg.bottom; topMargin: 26; horizontalCenter: parent.horizontalCenter }
        spacing: 6
        visible: root.isSelected

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

    MouseArea {
        anchors.fill: parent
        cursorShape: Qt.PointingHandCursor
        onClicked: root.clicked()
    }
}
