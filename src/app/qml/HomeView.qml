import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Logitune

Item {
    id: root
    signal deviceClicked()
    signal settingsClicked()

    RowLayout {
        id: topBar
        anchors {
            top: parent.top
            left: parent.left
            right: parent.right
        }
        height: Math.min(root.height * 0.185, 144)
        spacing: 12

        Item { width: 24 }

        Text {
            id: greeting
            text: {
                var hour = new Date().getHours();
                if (hour >= 5 && hour < 12) return "Good Morning";
                if (hour >= 12 && hour < 17) return "Good Afternoon";
                return "Good Evening";
            }
            font.pixelSize: Math.max(24, Math.min(36, root.width * 0.01 + root.height * 0.028))
            font.bold: true
            font.family: "Inter, sans-serif"
            color: Theme.text
        }

        Item { Layout.fillWidth: true }

        Text {
            id: settingsGear
            text: "\u2699"
            font.pixelSize: 20
            color: settingsHover.hovered ? Theme.accent : "#999999"

            HoverHandler { id: settingsHover }
            Behavior on color { ColorAnimation { duration: 150 } }

            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                onClicked: root.settingsClicked()
            }
        }

        Item { width: 24 }
    }

    Item {
        anchors {
            top: topBar.bottom
            bottom: parent.bottom
            left: parent.left
            right: parent.right
        }

        // Multi-device carousel
        PathView {
            id: carousel
            anchors {
                fill: parent
                bottomMargin: 40
            }
            model: DeviceModel
            currentIndex: DeviceModel.selectedIndex
            onCurrentIndexChanged: DeviceModel.selectedIndex = currentIndex
            visible: DeviceModel.count > 0

            pathItemCount: Math.min(DeviceModel.count, 5)
            preferredHighlightBegin: 0.5
            preferredHighlightEnd: 0.5
            highlightRangeMode: PathView.StrictlyEnforceRange
            interactive: DeviceModel.count > 1

            path: Path {
                startX: 0; startY: carousel.height / 2
                PathLine { x: carousel.width; y: carousel.height / 2 }
            }

            delegate: DeviceCard {
                required property int index
                required property string deviceId
                required property string deviceName
                required property string frontImage
                required property int batteryLevel
                required property bool batteryCharging
                required property string connectionType
                required property string status
                required property bool isSelected

                scale: PathView.isCurrentItem ? 1.0 : 0.65
                opacity: PathView.isCurrentItem ? 1.0 : 0.5
                z: PathView.isCurrentItem ? 2 : 1

                Behavior on scale { NumberAnimation { duration: 250; easing.type: Easing.OutCubic } }
                Behavior on opacity { NumberAnimation { duration: 250 } }

                onClicked: {
                    if (PathView.isCurrentItem)
                        root.deviceClicked()
                    else
                        carousel.currentIndex = index
                }
            }
        }

        // Dot indicators
        Row {
            anchors {
                horizontalCenter: parent.horizontalCenter
                bottom: parent.bottom
                bottomMargin: 12
            }
            spacing: 8
            visible: DeviceModel.count > 1

            Repeater {
                model: DeviceModel.count
                Rectangle {
                    required property int index
                    width: 8; height: 8; radius: 4
                    color: index === DeviceModel.selectedIndex ? Theme.accent : Theme.border
                    Behavior on color { ColorAnimation { duration: 200 } }
                }
            }
        }

        // No device connected state
        Column {
            anchors.centerIn: parent
            spacing: 12
            visible: DeviceModel.count === 0

            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: "CONNECT YOUR DEVICE(S)"
                font.pixelSize: 14
                font.letterSpacing: 1.5
                font.bold: true
                color: Theme.textSecondary
            }

            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: "Connect a Logitech device to get started"
                font.pixelSize: 13
                color: "#BBBBBB"
            }
        }

        // GNOME AppIndicator notification banner
        Rectangle {
            id: trayBanner
            anchors { bottom: parent.bottom; left: parent.left; right: parent.right; margins: 16 }
            height: bannerCol.implicitHeight + 24
            radius: 8
            color: Theme.surface
            border.color: Theme.border
            border.width: 1
            visible: {
                var status = DeviceModel.gnomeTrayStatus()
                return status === "not-installed" || status === "disabled"
            }

            Column {
                id: bannerCol
                anchors { fill: parent; margins: 12 }
                spacing: 6

                Text {
                    text: {
                        var status = DeviceModel.gnomeTrayStatus()
                        if (status === "not-installed")
                            return "Tray icon requires the AppIndicator GNOME extension"
                        if (status === "disabled")
                            return "AppIndicator extension is installed but not enabled"
                        return ""
                    }
                    font.pixelSize: 12
                    font.bold: true
                    color: Theme.text
                    wrapMode: Text.WordWrap
                    width: parent.width
                }

                Text {
                    text: {
                        var status = DeviceModel.gnomeTrayStatus()
                        if (status === "not-installed")
                            return "Run: sudo pacman -S gnome-shell-extension-appindicator\nThen log out and back in."
                        if (status === "disabled")
                            return "Run: gnome-extensions enable appindicatorsupport@rgcjonas.gmail.com\nThen restart Logitune."
                        return ""
                    }
                    font.pixelSize: 11
                    font.family: "monospace"
                    color: Theme.textSecondary
                    wrapMode: Text.WordWrap
                    width: parent.width
                }
            }

            Text {
                anchors { top: parent.top; right: parent.right; margins: 8 }
                text: "\u2715"
                font.pixelSize: 14
                color: Theme.textSecondary
                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: trayBanner.visible = false
                }
            }
        }
    }
}
