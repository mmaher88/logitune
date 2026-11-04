import QtCore
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Qt.labs.folderlistmodel
import Logitune

// In-app file browser for the ring editor. Qt's own non-native dialog is a light, unthemed list
// with no previews, which is useless for choosing an icon image. This one draws in the app theme
// and shows a thumbnail grid; GIFs animate while hovered.
//
// mode "image": pick an image file; "file": pick any file; "folder": pick the folder being shown.
Popup {
    id: browser

    property string mode: "image"
    property string folderPath: home
    property string selectedPath: ""

    signal chosen(string path)

    readonly property string home: StandardPaths.writableLocation(StandardPaths.HomeLocation).toString()
                                       .replace(/^file:\/\//, "")
    readonly property var imageFilters: ["*.png", "*.svg", "*.jpg", "*.jpeg", "*.webp", "*.gif"]
    readonly property var places: [
        { name: "Home", glyph: "", path: home },
        { name: "Pictures", glyph: "", path: home + "/Pictures" },
        { name: "Downloads", glyph: "", path: home + "/Downloads" },
        { name: "Icons", glyph: "", path: home + "/.local/share/icons" },
        { name: "System icons", glyph: "", path: "/usr/share/icons" },
    ]

    function isImage(name) {
        return /\.(png|svg|jpe?g|webp|gif)$/i.test(name)
    }

    function goUp() {
        if (folderPath === "/")
            return
        let parentPath = folderPath.replace(/\/[^\/]+\/?$/, "")
        folderPath = parentPath === "" ? "/" : parentPath
    }

    function openFolder(path) {
        folderPath = path
        selectedPath = ""
    }

    function confirm() {
        let path = mode === "folder" ? folderPath : selectedPath
        if (path === "")
            return
        chosen(path)
        close()
    }

    width: 720
    height: 520
    padding: 0
    modal: true
    anchors.centerIn: Overlay.overlay
    onOpened: selectedPath = ""

    background: Rectangle {
        radius: 10
        color: Theme.cardBg
        border.color: Theme.border
    }

    FolderListModel {
        id: folder
        folder: "file://" + browser.folderPath
        showDirsFirst: true
        showDotAndDotDot: false
        showHidden: false
        nameFilters: browser.mode === "image" ? browser.imageFilters : ["*"]
        showFiles: browser.mode !== "folder"
        sortCaseSensitive: false
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 14
        spacing: 10

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            RingFlatButton {
                text: ""
                font.family: "JetBrainsMono Nerd Font"
                enabled: browser.folderPath !== "/"
                onClicked: browser.goUp()
                ToolTip.visible: hovered
                ToolTip.text: "Up one folder"
            }
            RingTextField {
                id: pathField
                Layout.fillWidth: true
                text: browser.folderPath
                font.family: "monospace"
                // Typing a path and pressing Enter jumps straight there.
                onAccepted: browser.openFolder(text.replace(/\/$/, "") || "/")
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 12

            // Fixed width: its rows fill their column, and without a maximum the layout handed this
            // column the whole dialog and squeezed the file grid to nothing.
            ColumnLayout {
                Layout.preferredWidth: 150
                Layout.minimumWidth: 150
                Layout.maximumWidth: 150
                Layout.fillWidth: false
                Layout.fillHeight: true
                spacing: 2

                Repeater {
                    model: browser.places
                    Rectangle {
                        id: place
                        required property var modelData
                        readonly property bool current: browser.folderPath === modelData.path
                        Layout.fillWidth: true
                        height: 32
                        radius: 6
                        color: current ? Theme.hoverBg : placeHover.hovered ? Theme.hoverBg : "transparent"

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 8
                            spacing: 8
                            Item {
                                Layout.preferredWidth: 18
                                Layout.preferredHeight: 18
                                Glyph { icon: place.modelData.glyph; pixelSize: 14; color: Theme.textSecondary }
                            }
                            Text {
                                text: place.modelData.name
                                color: place.current ? Theme.accent : Theme.text
                                font.pixelSize: 13
                                Layout.fillWidth: true
                                elide: Text.ElideRight
                            }
                        }
                        HoverHandler { id: placeHover }
                        TapHandler { onTapped: browser.openFolder(place.modelData.path) }
                    }
                }
                Item { Layout.fillHeight: true }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                radius: 8
                color: Theme.surface
                clip: true

                GridView {
                    id: grid
                    objectName: "fileGrid"
                    anchors.fill: parent
                    anchors.margins: 8
                    cellWidth: 108
                    cellHeight: 112
                    model: folder
                    ScrollBar.vertical: ScrollBar {}

                    delegate: Rectangle {
                        id: tile
                        required property string fileName
                        required property string filePath
                        required property bool fileIsDir
                        readonly property bool selected: browser.selectedPath === filePath
                        readonly property bool showsPreview: !fileIsDir && browser.isImage(fileName)
                        readonly property bool isGif: /\.gif$/i.test(fileName)

                        width: grid.cellWidth - 8
                        height: grid.cellHeight - 8
                        radius: 8
                        color: selected ? Theme.accent : tileHover.hovered ? Theme.hoverBg : "transparent"

                        Item {
                            id: preview
                            anchors.horizontalCenter: parent.horizontalCenter
                            y: 8
                            width: 64
                            height: 64

                            Glyph {
                                visible: !tile.showsPreview
                                icon: tile.fileIsDir ? "" : ""
                                pixelSize: 44
                                color: tile.fileIsDir ? Theme.accent : Theme.textSecondary
                            }
                            // Thumbnails load off the UI thread and at a small size, so big photos
                            // don't stall scrolling. GIFs only play while the pointer is over them.
                            Image {
                                anchors.fill: parent
                                visible: tile.showsPreview && !tile.isGif
                                source: visible ? "file://" + tile.filePath : ""
                                asynchronous: true
                                sourceSize: Qt.size(128, 128)
                                fillMode: Image.PreserveAspectFit
                            }
                            AnimatedImage {
                                anchors.fill: parent
                                visible: tile.isGif
                                source: visible ? "file://" + tile.filePath : ""
                                asynchronous: true
                                playing: tileHover.hovered || tile.selected
                                fillMode: Image.PreserveAspectFit
                            }
                        }

                        Text {
                            anchors.top: preview.bottom
                            anchors.topMargin: 6
                            anchors.horizontalCenter: parent.horizontalCenter
                            width: parent.width - 8
                            text: tile.fileName
                            color: tile.selected ? Theme.activeTabText : Theme.text
                            font.pixelSize: 11
                            horizontalAlignment: Text.AlignHCenter
                            elide: Text.ElideMiddle
                        }

                        HoverHandler { id: tileHover }
                        TapHandler {
                            onTapped: {
                                if (!tile.fileIsDir)
                                    browser.selectedPath = tile.filePath
                            }
                            onDoubleTapped: {
                                if (tile.fileIsDir) {
                                    browser.openFolder(tile.filePath)
                                    return
                                }
                                browser.selectedPath = tile.filePath
                                browser.confirm()
                            }
                        }
                    }
                }

                Text {
                    anchors.centerIn: parent
                    visible: folder.status === FolderListModel.Ready && folder.count === 0
                    text: browser.mode === "image" ? "No images in this folder" : "This folder is empty"
                    color: Theme.textSecondary
                    font.pixelSize: 13
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Text {
                Layout.fillWidth: true
                text: {
                    if (browser.mode === "folder")
                        return browser.folderPath
                    return browser.selectedPath || "Nothing selected"
                }
                color: Theme.textSecondary
                font.pixelSize: 12
                elide: Text.ElideMiddle
            }
            RingFlatButton { text: "Cancel"; onClicked: browser.close() }
            RingFlatButton {
                text: browser.mode === "folder" ? "Use this folder" : "Choose"
                primary: true
                enabled: browser.mode === "folder" || browser.selectedPath !== ""
                onClicked: browser.confirm()
            }
        }
    }
}
