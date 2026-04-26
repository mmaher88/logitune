import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Logitune

Rectangle {
    id: sideNav
    color: Theme.background

    signal pageSelected(string pageName)
    property string currentPage: "buttons"

    // Nav items model — easy-switch is hidden for devices that don't
    // expose any slot positions or don't have a back image to render
    // them on. Both come from the loaded descriptor, so the binding
    // re-evaluates when DeviceModel switches devices.
    readonly property bool easySwitchSupported:
        DeviceModel.easySwitchSlotPositions.length > 0
        && DeviceModel.backImage.length > 0

    readonly property var navItems: {
        var items = [
            { name: "buttons",     label: "BUTTONS",         icon: "\uD83D\uDDB1", enabled: true  },
            { name: "pointscroll", label: "POINT & SCROLL",  icon: "\u25CE",       enabled: true  }
        ];
        if (easySwitchSupported)
            items.push({ name: "easyswitch", label: "EASY-SWITCH", icon: "\u21C4", enabled: true });
        items.push({ name: "settings", label: "SETTINGS", icon: "\u2261", enabled: true });
        return items;
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // Top spacer
        Item { height: 16 }

        // Device name area at the top of the sidebar
        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: 35
            Layout.rightMargin: 16
            spacing: 8

            EditableText {
                text: DeviceModel.deviceName || "MX Master 3S"
                pixelSize: 13
                textBold: true
                textColor: Theme.text
                Layout.fillWidth: true
                Layout.preferredHeight: 18
                onCommit: function(v) { EditorModel.updateText("deviceName", -1, v) }
            }
        }

        // Divider
        Rectangle {
            Layout.fillWidth: true
            Layout.topMargin: 12
            Layout.bottomMargin: 4
            height: 1
            color: Theme.border
        }

        // Nav items
        Repeater {
            model: sideNav.navItems

            delegate: Item {
                Layout.fillWidth: true
                Layout.topMargin: index > 0 ? 17 : 0    // 17px vertical margin between tabs
                height: 40
                opacity: modelData.enabled ? 1.0 : 0.4

                Rectangle {
                    id: pill
                    anchors {
                        fill: parent
                        leftMargin: 12
                        rightMargin: 8
                    }
                    radius: 4
                    readonly property bool isActive: sideNav.currentPage === modelData.name
                    readonly property bool isHovered: itemHover.hovered && modelData.enabled && !isActive

                    color: isActive ? Theme.activeTabBg
                         : isHovered ? Theme.hoverBg
                         : Qt.rgba(Theme.hoverBg.r, Theme.hoverBg.g, Theme.hoverBg.b, 0)
                    border.color: "transparent"
                    border.width: 0

                    Behavior on color { ColorAnimation { duration: 150 } }

                    RowLayout {
                        anchors {
                            fill: parent
                            leftMargin: 7
                            rightMargin: 12
                            topMargin: 0
                            bottomMargin: 0
                        }
                        spacing: 10

                        // 32x32 icon container
                        Item {
                            width: 32; height: 32
                            Text {
                                anchors.centerIn: parent
                                text: modelData.icon
                                font.pixelSize: 18
                                color: pill.isActive ? Theme.activeTabText
                                     : pill.isHovered ? Theme.text
                                     : Theme.dark ? "#888888" : "#555555"
                                Behavior on color { ColorAnimation { duration: 200 } }
                            }
                        }

                        Text {
                            text: modelData.label
                            font.pixelSize: 13
                            font.letterSpacing: 0.6
                            font.bold: true
                            color: pill.isActive ? Theme.activeTabText
                                 : pill.isHovered ? Theme.text
                                 : Theme.dark ? "#888888" : "#555555"
                            // Strikethrough when disabled
                            font.strikeout: !modelData.enabled
                            Layout.fillWidth: true

                            Behavior on color { ColorAnimation { duration: 200 } }
                        }
                    }
                }

                HoverHandler { id: itemHover }

                MouseArea {
                    anchors.fill: parent
                    enabled: modelData.enabled
                    cursorShape: modelData.enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
                    onClicked: {
                        sideNav.currentPage = modelData.name
                        sideNav.pageSelected(modelData.name)
                    }
                }
            }
        }

        // Bottom fill
        Item { Layout.fillHeight: true }

        // Battery chip at bottom of sidebar (Options+ style)
        BatteryChip {
            Layout.alignment: Qt.AlignHCenter
            Layout.bottomMargin: 16
            visible: DeviceModel.deviceConnected && DeviceModel.batteryLevel > 0
        }
    }

    // Edit-mode indicator stripe along the left edge, gated on EditorModel.editing
    Rectangle {
        id: editStripe
        objectName: "editStripe"
        anchors {
            left: parent.left
            top: parent.top
            bottom: parent.bottom
        }
        width: 4
        color: "#F5A623"
        visible: typeof EditorModel !== 'undefined' && EditorModel.editing
    }
}
