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
                }
                height: 80
                spacing: 10

                Item { width: 20 }  // left margin

                // Back arrow — 48x48 container
                Rectangle {
                    width: 48; height: 48
                    color: "transparent"

                    Text {
                        anchors.centerIn: parent
                        text: "\u2190"
                        font.pixelSize: 22
                        color: backHover.hovered ? Theme.accent : "#444444"
                        Behavior on color { ColorAnimation { duration: 120 } }
                    }

                    HoverHandler { id: backHover }
                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.StackView.view.pop()
                    }
                }

                Text {
                    text: DeviceModel.deviceName || "MX Master 3S"
                    font.pixelSize: 22
                    font.bold: true
                    color: Theme.text
                }

                Item { Layout.fillWidth: true }

                Item { width: 20 }  // right margin
            }

            // Per-app profile bar — visible on buttons/pointscroll only
            AppProfileBar {
                id: profileBar
                visible: sideNav.currentPage === "buttons" || sideNav.currentPage === "pointscroll"
                anchors {
                    top: header.bottom
                    left: parent.left
                    right: parent.right
                    topMargin: 4
                    leftMargin: 20
                    rightMargin: 20
                }
                height: visible ? 48 : 0
            }

            // Content StackView — default to ButtonsPage
            StackView {
                id: contentStack
                anchors {
                    top: profileBar.bottom
                    bottom: parent.bottom
                    left: parent.left
                    right: parent.right
                    topMargin: 12
                    bottomMargin: 12
                }
                initialItem: buttonsPageComponent
            }

            // Battery chip moved to SideNav bottom
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
                color: Theme.textSecondary
                lineHeight: 1.5
            }
        }
    }

    // Disconnect handling — pop back to home immediately. The old
    // 3-second delay meant quick transport switches (Bolt <-> BT) could
    // reconnect before the timer fired but still kick the user out of
    // the device page. Going back on the first disconnect edge is
    // snappier and also correct: if the device reconnects, the user
    // can re-enter from the home carousel.
    Connections {
        target: DeviceModel
        function onDeviceConnectedChanged() {
            if (!DeviceModel.deviceConnected)
                root.StackView.view.pop()
        }
    }
}
