import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Logitune

Item {
    id: root

    RowLayout {
        anchors.fill: parent
        spacing: 0

        // ── Left sidebar ─────────────────────────────────────────────────────
        SideNav {
            id: sideNav
            Layout.preferredWidth: 200
            Layout.fillHeight: true

            onPageSelected: function(page) {
                if (page === "buttons") {
                    contentStack.replace(buttonsPageComponent, {}, StackView.Immediate)
                } else if (page === "pointscroll") {
                    contentStack.replace(pointScrollPageComponent, {}, StackView.Immediate)
                } else if (page === "easyswitch") {
                    contentStack.replace(easySwitchComponent, {}, StackView.Immediate)
                } else if (page === "settings") {
                    contentStack.replace(settingsComponent, {}, StackView.Immediate)
                } else {
                    contentStack.replace(placeholderComponent,
                                         {pageName: page},
                                         StackView.Immediate)
                }
            }
        }

        // ── Main content area ─────────────────────────────────────────────────
        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            // Header: Back + device name
            RowLayout {
                id: header
                anchors {
                    top: parent.top
                    left: parent.left
                    right: parent.right
                    topMargin: 20
                    leftMargin: 20
                    rightMargin: 20
                }
                spacing: 10

                // Back arrow
                Text {
                    text: "\u2190"
                    font.pixelSize: 22
                    color: backHover.containsMouse ? "#7B61FF" : "#444444"
                    Behavior on color { ColorAnimation { duration: 120 } }

                    HoverHandler { id: backHover }
                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: StackView.view.pop()
                    }
                }

                Text {
                    text: DeviceModel.deviceName.length > 0 ? DeviceModel.deviceName : "Device"
                    font.pixelSize: 20
                    font.bold: true
                    color: "#1A1A1A"
                }

                Item { Layout.fillWidth: true }
            }

            // Profile bar placeholder
            Rectangle {
                id: profileBar
                anchors {
                    top: header.bottom
                    left: parent.left
                    right: parent.right
                    topMargin: 12
                    leftMargin: 20
                    rightMargin: 20
                }
                height: 40
                color: "#FFFFFF"
                radius: 8
                border.color: "#E8E8E8"
                border.width: 1

                Text {
                    anchors.centerIn: parent
                    text: "Profiles"
                    font.pixelSize: 13
                    font.letterSpacing: 0.5
                    color: "#666666"
                }
            }

            // Content StackView — default to ButtonsPage
            StackView {
                id: contentStack
                anchors {
                    top: profileBar.bottom
                    bottom: batteryChipArea.top
                    left: parent.left
                    right: parent.right
                    topMargin: 12
                    bottomMargin: 12
                }
                initialItem: buttonsPageComponent
            }

            // Battery chip at bottom-left of content area
            Item {
                id: batteryChipArea
                anchors {
                    bottom: parent.bottom
                    left: parent.left
                    right: parent.right
                    bottomMargin: 20
                    leftMargin: 20
                }
                height: 36
                visible: DeviceModel.deviceConnected

                BatteryChip {
                    anchors.left: parent.left
                }
            }
        }
    }

    // ButtonsPage component (default)
    Component {
        id: buttonsPageComponent
        ButtonsPage {}
    }

    // Point & Scroll page component
    Component {
        id: pointScrollPageComponent
        PointScrollPage {}
    }

    Component {
        id: easySwitchComponent
        EasySwitchPage {}
    }

    Component {
        id: settingsComponent
        SettingsPage {}
    }

    // Placeholder page component
    Component {
        id: placeholderComponent

        Item {
            property string pageName: ""

            Text {
                anchors.centerIn: parent
                text: pageName.length > 0
                      ? pageName.toUpperCase() + "\npage coming soon"
                      : "Select a page"
                horizontalAlignment: Text.AlignHCenter
                font.pixelSize: 15
                color: "#999999"
                lineHeight: 1.5
            }
        }
    }
}
