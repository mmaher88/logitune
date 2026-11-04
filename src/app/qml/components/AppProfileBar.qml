import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Logitune

Item {
    id: profileBar
    implicitHeight: 48
    property bool compact: false

    property var _allApps: []

    // Whether chips overflow the visible area
    readonly property bool overflowing: chipRow.implicitWidth > chipClip.width

    // Scroll offset (pixels)
    property real scrollOffset: 0

    function scrollLeft() {
        scrollOffset = Math.max(0, scrollOffset - 120)
    }
    function scrollRight() {
        var maxScroll = Math.max(0, chipRow.implicitWidth - chipClip.width)
        scrollOffset = Math.min(maxScroll, scrollOffset + 120)
    }

    RowLayout {
        anchors.fill: parent
        spacing: 6

        // ── Clipped chip area ────────────────────────────────────────────
        Item {
            id: chipClip
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true

            Row {
                id: chipRow
                anchors.verticalCenter: parent.verticalCenter
                x: profileBar.overflowing ? -profileBar.scrollOffset
                   : (chipClip.width - chipRow.implicitWidth) / 2
                spacing: 6

                Behavior on x { NumberAnimation { duration: 200; easing.type: Easing.OutCubic } }

                Repeater {
                    model: ProfileModel

                    delegate: Rectangle {
                        width: profileBar.compact && model.index > 0 ? 36 : chipLabel.implicitWidth + (model.icon ? 46 : 24)
                        height: 32
                        radius: 6
                        color: model.isActive ? Theme.accent
                             : chipHover.hovered ? Theme.hoverBg : Theme.surface
                        border.color: model.isActive ? "transparent" : Theme.border
                        border.width: 1

                        Behavior on color { ColorAnimation { duration: 150 } }

                        HoverHandler { id: chipHover }

                        Row {
                            anchors.centerIn: parent
                            spacing: 6

                            Image {
                                width: 16; height: 16
                                anchors.verticalCenter: parent.verticalCenter
                                source: model.index === 0 && profileBar.compact ? "" : model.icon ? "image://icon/" + model.icon : ""
                                sourceSize: Qt.size(16, 16)
                                visible: status === Image.Ready
                            }

                            Text {
                                id: chipLabel
                                text: profileBar.compact && model.index === 0 ? "All applications" : model.name
                                visible: !profileBar.compact || model.index === 0 || !model.icon
                                font.pixelSize: 12
                                font.bold: model.isActive
                                color: model.isActive ? Theme.activeTabText : Theme.text
                                elide: Text.ElideRight
                                anchors.verticalCenter: parent.verticalCenter
                            }
                        }

                        Rectangle {
                            anchors.horizontalCenter: parent.horizontalCenter
                            anchors.bottom: parent.bottom
                            anchors.bottomMargin: -4
                            width: 6; height: 3; radius: 1.5
                            color: Theme.accent
                            visible: model.isHwActive && !model.isActive
                        }

                        MouseArea {
                            anchors.fill: parent
                            acceptedButtons: Qt.LeftButton | Qt.RightButton
                            cursorShape: Qt.PointingHandCursor

                            onClicked: function(mouse) {
                                if (mouse.button === Qt.RightButton && model.index > 0) {
                                    contextMenu.profileIndex = model.index
                                    contextMenu.profileName = model.name
                                    contextMenu.popup()
                                } else if (mouse.button === Qt.LeftButton) {
                                    ProfileModel.selectTab(model.index)
                                }
                            }
                        }
                        ToolTip.visible: chipHover.hovered && profileBar.compact
                        ToolTip.text: model.name
                    }
                }
            }

            // Horizontal scroll via mouse wheel
            WheelHandler {
                onWheel: function(event) {
                    if (!profileBar.overflowing) return
                    var d = event.angleDelta.x !== 0 ? event.angleDelta.x : event.angleDelta.y
                    if (d > 0) profileBar.scrollLeft()
                    else if (d < 0) profileBar.scrollRight()
                }
            }
        }

        // ── Scroll arrows (right side, only when overflowing) ────────────
        Row {
            spacing: 2
            visible: profileBar.overflowing
            Layout.alignment: Qt.AlignVCenter

            Rectangle {
                width: 24; height: 24; radius: 12
                color: leftHover.hovered ? Theme.hoverBg : Theme.surface
                border.color: Theme.border; border.width: 1
                opacity: profileBar.scrollOffset > 0 ? 1.0 : 0.3

                Text {
                    anchors.centerIn: parent
                    text: "\u2039"; font.pixelSize: 14; color: Theme.text
                }
                HoverHandler { id: leftHover }
                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: profileBar.scrollLeft()
                }
            }

            Rectangle {
                width: 24; height: 24; radius: 12
                color: rightHover.hovered ? Theme.hoverBg : Theme.surface
                border.color: Theme.border; border.width: 1
                opacity: profileBar.scrollOffset < (chipRow.implicitWidth - chipClip.width) ? 1.0 : 0.3

                Text {
                    anchors.centerIn: parent
                    text: "\u203A"; font.pixelSize: 14; color: Theme.text
                }
                HoverHandler { id: rightHover }
                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: profileBar.scrollRight()
                }
            }
        }

        // ── "+ Add application" button (always visible, outside scroll) ──
        Rectangle {
            id: addBtn
            Layout.alignment: Qt.AlignVCenter
            width: addLabel.implicitWidth + 24
            height: 32
            radius: 6
            color: addHover.hovered ? Theme.hoverBg : "transparent"
            border.color: Theme.border
            border.width: 1

            Behavior on color { ColorAnimation { duration: 150 } }

            Text {
                id: addLabel
                anchors.centerIn: parent
                text: profileBar.compact ? "+" : "+ Add application"
                font.pixelSize: 12
                color: Theme.textSecondary
            }

            HoverHandler { id: addHover }

            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                onClicked: {
                    if (dropdown.opened) {
                        dropdown.close()
                    } else {
                        profileBar._allApps = DeviceModel.runningApplications()
                        dropdown.open()
                        searchInput.text = ""
                        searchInput.forceActiveFocus()
                    }
                }
            }
        }
    }

    // ── Right-click context menu ────────────────────────────────────────────
    Menu {
        id: contextMenu
        property int profileIndex: -1
        property string profileName: ""

        MenuItem {
            text: "Remove \"" + contextMenu.profileName + "\""
            onTriggered: {
                if (contextMenu.profileIndex > 0)
                    ProfileModel.removeProfile(contextMenu.profileIndex)
            }
        }
    }

    // ── Dropdown panel ──────────────────────────────────────────────────────
    Popup {
        id: dropdown
        x: addBtn.x + addBtn.width - width
        y: addBtn.y + addBtn.height + 6
        width: 280
        padding: 8
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

        background: Rectangle {
            radius: 8
            color: Theme.cardBg
            border.color: Theme.cardBorder
            border.width: 1
        }

        Column {
            width: parent.width
            spacing: 4

            TextField {
                id: searchInput
                width: parent.width
                height: 32
                placeholderText: "Search applications..."
                font.pixelSize: 13
                color: Theme.text
                placeholderTextColor: Theme.textSecondary
                background: Rectangle {
                    radius: 6
                    color: Theme.inputBg
                    border.color: searchInput.activeFocus ? Theme.accent : Theme.border
                    border.width: 1
                }
            }

            ListView {
                width: parent.width
                height: 300
                clip: true
                spacing: 1

                model: {
                    var query = searchInput.text.toLowerCase()
                    var filtered = []
                    for (var i = 0; i < profileBar._allApps.length; i++) {
                        var app = profileBar._allApps[i]
                        var name = (app.title || "").toLowerCase()
                        var wm = (app.wmClass || "").toLowerCase()
                        if (query.length === 0 || name.indexOf(query) !== -1 || wm.indexOf(query) !== -1)
                            filtered.push(app)
                    }
                    return filtered
                }

                delegate: Rectangle {
                    width: ListView.view.width
                    height: 36
                    radius: 4
                    color: appItemHover.hovered ? Theme.hoverBg : "transparent"

                    Row {
                        anchors {
                            verticalCenter: parent.verticalCenter
                            left: parent.left; leftMargin: 8
                            right: parent.right; rightMargin: 8
                        }
                        spacing: 8

                        Image {
                            width: 22; height: 22
                            anchors.verticalCenter: parent.verticalCenter
                            source: modelData.icon ? "image://icon/" + modelData.icon : ""
                            sourceSize: Qt.size(22, 22)
                            visible: status === Image.Ready
                        }

                        Text {
                            text: modelData.title || modelData.wmClass
                            font.pixelSize: 13
                            color: Theme.text
                            anchors.verticalCenter: parent.verticalCenter
                            elide: Text.ElideRight
                            width: parent.width - wmLabel.width - 40
                        }
                        Text {
                            id: wmLabel
                            text: modelData.wmClass
                            font.pixelSize: 10
                            color: Theme.textSecondary
                            anchors.verticalCenter: parent.verticalCenter
                        }
                    }

                    HoverHandler { id: appItemHover }

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            ProfileModel.addProfile(modelData.wmClass, modelData.title || modelData.wmClass, modelData.icon || "")
                            dropdown.close()
                        }
                    }
                }
            }
        }
    }
}
