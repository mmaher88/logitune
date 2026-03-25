import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Logitune

Item {
    id: root

    // ── Top bar ──────────────────────────────────────────────────────────────
    RowLayout {
        id: topBar
        anchors {
            top: parent.top
            left: parent.left
            right: parent.right
            topMargin: 24
            leftMargin: 24
            rightMargin: 24
        }
        spacing: 12

        Text {
            id: greeting
            text: {
                var hour = new Date().getHours();
                if (hour >= 5 && hour < 12) return "Good Morning";
                if (hour >= 12 && hour < 17) return "Good Afternoon";
                return "Good Evening";
            }
            font.pixelSize: 28
            font.bold: true
            font.family: "Inter, sans-serif"
            color: "#1A1A1A"
        }

        Item { Layout.fillWidth: true }

        Text {
            text: "+ ADD DEVICE"
            font.pixelSize: 13
            font.letterSpacing: 0.5
            color: "#CCCCCC"

            ToolTip.visible: addDeviceHover.hovered
            ToolTip.text: "Coming soon"
            ToolTip.delay: 400

            HoverHandler { id: addDeviceHover }
        }

        Text {
            text: "|"
            color: "#E0E0E0"
            font.pixelSize: 16
        }

        Text {
            id: settingsGear
            text: "\u2699"
            font.pixelSize: 20
            color: settingsHover.hovered ? "#555555" : "#999999"

            HoverHandler { id: settingsHover }

            Behavior on color { ColorAnimation { duration: 150 } }
        }
    }

    // ── Center content ────────────────────────────────────────────────────────
    Item {
        anchors {
            top: topBar.bottom
            bottom: parent.bottom
            left: parent.left
            right: parent.right
        }

        // Device connected state
        Column {
            anchors.centerIn: parent
            spacing: 16
            visible: DeviceModel.deviceConnected

            // Mouse placeholder rectangle
            Rectangle {
                id: deviceCard
                width: 200
                height: 260
                radius: 20
                anchors.horizontalCenter: parent.horizontalCenter

                gradient: Gradient {
                    GradientStop { position: 0.0; color: "#2C2C2E" }
                    GradientStop { position: 1.0; color: "#1A1A1A" }
                }

                // Simple mouse silhouette lines
                Column {
                    anchors.centerIn: parent
                    spacing: 6
                    opacity: 0.4

                    Rectangle { width: 60; height: 3; radius: 2; color: "white"; anchors.horizontalCenter: parent.horizontalCenter }
                    Rectangle { width: 40; height: 3; radius: 2; color: "white"; anchors.horizontalCenter: parent.horizontalCenter }
                    Rectangle { width: 50; height: 3; radius: 2; color: "white"; anchors.horizontalCenter: parent.horizontalCenter }
                }

                Text {
                    anchors {
                        bottom: parent.bottom
                        bottomMargin: 16
                        horizontalCenter: parent.horizontalCenter
                    }
                    text: DeviceModel.deviceName
                    color: "white"
                    font.pixelSize: 12
                    font.bold: true
                    opacity: 0.7
                }

                // Hover/click effects
                HoverHandler { id: cardHover }

                scale: cardHover.hovered ? 1.03 : 1.0
                Behavior on scale { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }

                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        StackView.view.push(deviceViewComponent)
                    }
                }

                layer.enabled: cardHover.hovered
                layer.effect: null
            }

            // Device name label below the card
            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: DeviceModel.deviceName
                font.pixelSize: 15
                font.bold: true
                color: "#1A1A1A"
            }

            // Battery chip
            BatteryChip {
                anchors.horizontalCenter: parent.horizontalCenter
            }
        }

        // No device connected state
        Column {
            anchors.centerIn: parent
            spacing: 12
            visible: !DeviceModel.deviceConnected

            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: "CONNECT YOUR DEVICE(S)"
                font.pixelSize: 14
                font.letterSpacing: 1.5
                font.bold: true
                color: "#999999"
            }

            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: "Connect a Logitech device to get started"
                font.pixelSize: 13
                color: "#BBBBBB"
            }
        }
    }

    // Expose deviceViewComponent reference so children can push to the stack
    property Component deviceViewComponent: Component { DeviceView {} }
}
