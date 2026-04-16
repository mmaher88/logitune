import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import Logitune

Item {
    id: root

    Rectangle {
        anchors.fill: parent
        color: Theme.background
    }

    // Scale content to fit available height
    readonly property real availH: height
    // Total fixed content: title(~60) + channels(~180) + footer(~30) + spacing(~80) = ~350
    readonly property real imageMaxH: Math.max(120, availH - 380)
    readonly property real imageH: Math.min(380, imageMaxH)
    readonly property real imageW: imageH * 0.676  // back image aspect ratio 692/1024

    Flickable {
        anchors.fill: parent
        contentHeight: contentCol.implicitHeight + 40
        clip: true
        boundsBehavior: Flickable.StopAtBounds

        Column {
            id: contentCol
            width: parent.width
            spacing: 20

            Item { width: 1; height: 10 }  // top padding

            // ── Device image with pulsing LED ───────────────────────────────
            Item {
                id: imageContainer
                width: root.imageW
                height: root.imageH
                anchors.horizontalCenter: parent.horizontalCenter

                Image {
                    id: deviceImage
                    anchors.fill: parent
                    source: DeviceModel.backImage
                    fillMode: Image.PreserveAspectFit
                    smooth: true
                    mipmap: true
                }

                DropArea {
                    id: backImageDrop
                    anchors.fill: deviceImage
                    enabled: typeof EditorModel !== 'undefined' && EditorModel.editing
                    onDropped: function(drop) {
                        if (drop.hasUrls && drop.urls.length > 0) {
                            var url = drop.urls[0].toString()
                            if (url.toLowerCase().endsWith(".png")) {
                                var path = url.replace(/^file:\/\//, "")
                                EditorModel.replaceImage("back", path)
                            }
                        }
                    }
                }

                Rectangle {
                    id: replaceBackButton
                    visible: typeof EditorModel !== 'undefined' && EditorModel.editing
                    anchors {
                        top: deviceImage.top
                        right: deviceImage.right
                        margins: 4
                    }
                    width: 32; height: 28
                    radius: 4
                    color: replaceBackHover.hovered ? Theme.hoverBg : Theme.inputBg
                    Behavior on color { ColorAnimation { duration: 150 } }

                    Text {
                        anchors.centerIn: parent
                        text: "\uD83D\uDDBC"
                        font.pixelSize: 16
                        color: Theme.text
                    }

                    HoverHandler { id: replaceBackHover }
                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: backImageDialog.open()
                        hoverEnabled: true
                        ToolTip.visible: replaceBackHover.hovered
                        ToolTip.text: "Replace image"
                        ToolTip.delay: 500
                    }
                }

                FileDialog {
                    id: backImageDialog
                    nameFilters: ["PNG (*.png)"]
                    onAccepted: {
                        var url = selectedFile.toString()
                        var path = url.replace(/^file:\/\//, "")
                        EditorModel.replaceImage("back", path)
                    }
                }

                readonly property real imgX: (width - deviceImage.paintedWidth) / 2
                readonly property real imgY: (height - deviceImage.paintedHeight) / 2
                readonly property real imgW: deviceImage.paintedWidth
                readonly property real imgH: deviceImage.paintedHeight

                readonly property var slotPositions: DeviceModel.easySwitchSlotPositions

                Repeater {
                    model: imageContainer.slotPositions.length
                    Item {
                        id: slotItem
                        required property int index
                        readonly property bool isActive: (index + 1) === DeviceModel.activeSlot
                        readonly property var pos: index < imageContainer.slotPositions.length
                            ? imageContainer.slotPositions[index] : { xPct: 0.5, yPct: 0.65 }

                        width: 24; height: 24

                        readonly property real targetX: imageContainer.imgX + imageContainer.imgW * pos.xPct
                        readonly property real targetY: imageContainer.imgY + imageContainer.imgH * pos.yPct

                        x: targetX - width / 2
                        y: targetY - height / 2

                        Connections {
                            target: DeviceModel
                            function onSelectedChanged() {
                                if (!drag.active) {
                                    slotItem.x = slotItem.targetX - slotItem.width / 2
                                    slotItem.y = slotItem.targetY - slotItem.height / 2
                                }
                            }
                        }

                        Rectangle {
                            anchors.centerIn: parent
                            width: 16; height: 16; radius: 8
                            color: slotItem.isActive ? Theme.accent : "transparent"
                            border.color: Theme.accent
                            border.width: slotItem.isActive ? 0 : 1.5

                            Text {
                                anchors.centerIn: parent
                                text: (slotItem.index + 1).toString()
                                font.pixelSize: 9
                                font.bold: true
                                color: slotItem.isActive ? Theme.activeTabText : Theme.accent
                            }

                            SequentialAnimation on opacity {
                                running: slotItem.isActive
                                loops: Animation.Infinite
                                NumberAnimation { to: 0.3; duration: 800; easing.type: Easing.InOutSine }
                                NumberAnimation { to: 1.0; duration: 800; easing.type: Easing.InOutSine }
                            }
                        }

                        DragHandler {
                            id: drag
                            enabled: typeof EditorModel !== 'undefined' && EditorModel.editing
                            target: parent
                            onActiveChanged: {
                                if (!active) {
                                    var cx = slotItem.x + slotItem.width / 2
                                    var cy = slotItem.y + slotItem.height / 2
                                    var xPct = (cx - imageContainer.imgX) / imageContainer.imgW
                                    var yPct = (cy - imageContainer.imgY) / imageContainer.imgH
                                    xPct = Math.max(0, Math.min(1, xPct))
                                    yPct = Math.max(0, Math.min(1, yPct))
                                    EditorModel.updateSlotPosition(slotItem.index, xPct, yPct)
                                }
                            }
                        }
                    }
                }
            }

            // ── Title ───────────────────────────────────────────────────────
            Column {
                spacing: 4
                anchors.horizontalCenter: parent.horizontalCenter

                Text {
                    text: "Easy-Switch"
                    font { pixelSize: 22; bold: true }
                    color: Theme.text
                    anchors.horizontalCenter: parent.horizontalCenter
                }
                Text {
                    text: "Connected via " + DeviceModel.connectionType
                    font.pixelSize: 13
                    color: Theme.accent
                    anchors.horizontalCenter: parent.horizontalCenter
                    visible: DeviceModel.deviceConnected
                }
            }

            // ── Channel list ────────────────────────────────────────────────
            Column {
                width: Math.min(380, root.width - 40)
                spacing: 0
                anchors.horizontalCenter: parent.horizontalCenter

                Repeater {
                    model: DeviceModel.easySwitchSlotPositions.length
                    delegate: Rectangle {
                        required property int index
                        readonly property bool isActive: (index + 1) === DeviceModel.activeSlot
                        readonly property var slotData: DeviceModel.easySwitchSlotPositions[index] || ({})
                        readonly property string slotLabel: (slotData.label && slotData.label.length > 0)
                            ? slotData.label
                            : (isActive ? DeviceModel.connectionType : "Available")

                        width: parent.width
                        height: 56
                        color: isActive ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.06)
                                        : "transparent"
                        radius: 8

                        Row {
                            anchors {
                                fill: parent
                                leftMargin: 16
                                rightMargin: 16
                            }
                            spacing: 14

                            Rectangle {
                                width: 32; height: 32; radius: 16
                                anchors.verticalCenter: parent.verticalCenter
                                color: isActive ? Theme.accent : Theme.inputBg

                                Text {
                                    anchors.centerIn: parent
                                    text: (index + 1).toString()
                                    font { pixelSize: 14; bold: true }
                                    color: isActive ? Theme.activeTabText : Theme.textSecondary
                                }
                            }

                            Column {
                                anchors.verticalCenter: parent.verticalCenter
                                spacing: 2

                                EditableText {
                                    width: 260
                                    height: 20
                                    text: slotLabel
                                    pixelSize: 14
                                    textBold: isActive
                                    textColor: isActive ? Theme.text : Theme.textSecondary
                                    onCommit: function(v) { EditorModel.updateText("slotLabel", index, v) }
                                }
                                Text {
                                    text: isActive ? "Connected" : ""
                                    font.pixelSize: 11
                                    color: Theme.accent
                                }
                            }
                        }

                        Rectangle {
                            anchors { bottom: parent.bottom; left: parent.left; right: parent.right }
                            anchors.leftMargin: 62
                            height: 1
                            color: Theme.border
                            visible: index < DeviceModel.easySwitchSlotPositions.length - 1
                        }
                    }
                }
            }

            Text {
                text: "Press the Easy-Switch button on your mouse to change channels"
                font.pixelSize: 11
                color: Theme.textSecondary
                anchors.horizontalCenter: parent.horizontalCenter
            }

            Item { width: 1; height: 10 }  // bottom padding
        }
    }
}
