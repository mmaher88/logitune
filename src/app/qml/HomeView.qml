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
            flickDeceleration: 800
            maximumFlickVelocity: 4000
            snapMode: PathView.SnapToItem
            highlightMoveDuration: 300

            path: Path {
                startX: 0; startY: carousel.height / 2
                PathLine { x: carousel.width; y: carousel.height / 2 }
            }

            delegate: DeviceCard {
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

            // Debounce wheel events: step once per event, ignore further
            // events until the card-swap animation settles. Without this a
            // fast spin dispatches many events in rapid succession and the
            // carousel jitters or wraps through the list.
            property double wheelLastTick: 0

            WheelHandler {
                acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
                onWheel: function(event) {
                    if (DeviceModel.count <= 1)
                        return
                    var now = Date.now()
                    if (now - carousel.wheelLastTick < carousel.highlightMoveDuration)
                        return
                    var delta = event.angleDelta.y + event.angleDelta.x
                    if (Math.abs(delta) < 20)
                        return
                    if (delta > 0)
                        carousel.decrementCurrentIndex()
                    else
                        carousel.incrementCurrentIndex()
                    carousel.wheelLastTick = now
                }
            }
        }

        // Counter text
        Text {
            anchors {
                horizontalCenter: parent.horizontalCenter
                bottom: scrollTrack.top
                bottomMargin: 6
            }
            text: (DeviceModel.selectedIndex + 1) + " / " + DeviceModel.count
            font.pixelSize: 11
            color: Theme.textSecondary
            visible: DeviceModel.count > 1
        }

        // Scroll indicator — thin track with highlighted segment
        Rectangle {
            id: scrollTrack
            anchors {
                horizontalCenter: parent.horizontalCenter
                bottom: parent.bottom
                bottomMargin: 12
            }
            width: Math.min(200, parent.width * 0.3)
            height: 3
            radius: 1.5
            color: Theme.border
            visible: DeviceModel.count > 1

            Rectangle {
                height: parent.height
                radius: parent.radius
                color: Theme.accent
                width: Math.max(12, parent.width / Math.max(1, DeviceModel.count))
                x: DeviceModel.count > 1
                    ? (DeviceModel.selectedIndex / (DeviceModel.count - 1)) * (parent.width - width)
                    : 0
                Behavior on x { NumberAnimation { duration: 200; easing.type: Easing.OutCubic } }
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
            readonly property string trayStatus: DeviceModel.gnomeTrayStatus()
            anchors { bottom: parent.bottom; left: parent.left; right: parent.right; margins: 16 }
            height: bannerCol.implicitHeight + 24
            radius: 8
            color: Theme.surface
            border.color: Theme.border
            border.width: 1
            visible: trayStatus === "not-installed" || trayStatus === "disabled"

            Column {
                id: bannerCol
                anchors { fill: parent; margins: 12 }
                spacing: 6

                Text {
                    text: {
                        if (trayBanner.trayStatus === "not-installed")
                            return "Tray icon requires the AppIndicator GNOME extension"
                        if (trayBanner.trayStatus === "disabled")
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
                        if (trayBanner.trayStatus === "not-installed")
                            return "Run: " + DeviceModel.appIndicatorInstallCommand() + "\nThen log out and back in."
                        if (trayBanner.trayStatus === "disabled")
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
