import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Logitune

Frame {
    id: root
    required property var editor
    property string payload: ""
    property string appName: ""
    property string appIcon: ""
    property var apps: []
    readonly property string kind: editor.pending ? editor.pending.action.kind : ""
    function reset() {
        payload = ""; appName = ""; appIcon = ""
        value.text = ""; appSearch.text = ""; capture.keystroke = ""
        commandText.text = ""; commandName.text = ""
    }
    Connections { target: root.editor; function onPendingChanged() { root.reset() } }
    Component.onCompleted: {
        let result = []
        if (typeof InstalledApps !== "undefined")
            for (let i = 0; i < InstalledApps.rowCount(); ++i) result.push(InstalledApps.get(i))
        apps = result
    }
    background: Rectangle { color: Theme.cardBg; radius: 8; border.color: Theme.border }
    ColumnLayout {
        anchors.fill: parent; spacing: 12
        Label {
            text: root.editor.pending ? root.editor.pending.action.name : ""
            color: Theme.text; font { bold: true; pixelSize: 18 }
            Layout.fillWidth: true; wrapMode: Text.WordWrap
        }
        RingTextField {
            id: appSearch
            visible: root.kind === "app"; Layout.fillWidth: true
            placeholderText: "Search installed applications"
        }
        ListView {
            id: appList
            visible: root.kind === "app"
            Layout.fillWidth: true; Layout.preferredHeight: 230
            clip: true
            model: root.apps.filter(a => a.name.toLowerCase().indexOf(appSearch.text.toLowerCase()) >= 0)
            delegate: Rectangle {
                id: appRow
                required property var modelData
                readonly property bool chosen: root.payload === modelData.command
                width: appList.width
                height: 36
                radius: 6
                color: chosen ? Theme.accent : appHover.hovered ? Theme.hoverBg : "transparent"

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 8
                    anchors.rightMargin: 8
                    spacing: 10
                    Item {
                        Layout.preferredWidth: 22
                        Layout.preferredHeight: 22
                        Glyph { icon: appRow.modelData.icon; pixelSize: 20; color: Theme.text }
                    }
                    Text {
                        text: appRow.modelData.name
                        color: appRow.chosen ? Theme.activeTabText : Theme.text
                        font.pixelSize: 13
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                    }
                }
                HoverHandler { id: appHover }
                TapHandler {
                    onTapped: {
                        root.payload = appRow.modelData.command
                        root.appName = appRow.modelData.name
                        root.appIcon = appRow.modelData.icon
                    }
                }
            }
            ScrollBar.vertical: ScrollBar {}
        }
        Label {
            visible: root.kind === "app"; text: root.appName || "Choose an application"
            color: Theme.textSecondary; Layout.fillWidth: true; elide: Text.ElideRight
        }
        KeystrokeCapture {
            id: capture
            visible: root.kind === "keystroke"; Layout.fillWidth: true
            onKeystrokeCaptured: ks => root.payload = ks
        }
        RingTextField {
            id: value
            objectName: "parameterValue"
            Layout.fillWidth: true
            visible: ["url", "file", "folder"].indexOf(root.kind) >= 0
            placeholderText: root.kind === "url" ? "https://…" : "Absolute path"
            onTextChanged: root.payload = text
        }
        // Custom command: any shell command, with its own name and icon.
        RingTextField {
            id: commandText
            objectName: "commandText"
            Layout.fillWidth: true
            visible: root.kind === "command"
            placeholderText: "Command, e.g. grim -g \"$(slurp)\" - | wl-copy"
            font.family: "monospace"
            onTextChanged: root.payload = text
        }
        RingTextField {
            id: commandName
            objectName: "commandName"
            Layout.fillWidth: true
            visible: root.kind === "command"
            placeholderText: "Name shown on the ring"
            onTextChanged: root.appName = text
        }
        RowLayout {
            visible: root.kind === "command"
            spacing: 10

            Rectangle {
                width: 40
                height: 40
                radius: 20
                color: Theme.surface
                border.color: Theme.border
                Glyph {
                    icon: root.appIcon || ""
                    pixelSize: 20
                    color: Theme.text
                }
            }
            RingFlatButton {
                text: "Choose icon…"
                onClicked: commandIconPicker.open()
            }
        }
        RingIconPicker {
            id: commandIconPicker
            onPicked: icon => root.appIcon = icon
        }

        RingFlatButton {
            visible: root.kind === "file" || root.kind === "folder"
            text: root.kind === "folder" ? "Choose folder…" : "Choose file…"
            onClicked: {
                fileBrowser.mode = root.kind
                fileBrowser.open()
            }
        }
        RowLayout {
            Layout.fillWidth: true
            RingFlatButton { objectName: "parameterCancel"; text: "Cancel"; onClicked: root.editor.cancelParameter() }
            Item { Layout.fillWidth: true }
            RingFlatButton {
                objectName: "parameterSave"; text: "Save"; primary: true; enabled: root.payload.trim().length > 0
                onClicked: {
                    let name = root.appName
                    if (!name && root.kind === "keystroke") name = root.payload
                    if (!name && root.kind === "url") name = root.payload.replace(/^https?:\/\//, "").replace(/\/$/, "")
                    if (!name && (root.kind === "file" || root.kind === "folder")) name = root.payload.split("/").filter(s => s).pop()
                    // A command with no name falls back to its program name, e.g. "grim".
                    if (!name && root.kind === "command") name = root.payload.trim().split(/\s+/)[0]
                    root.editor.saveParameter(root.payload, name, root.appIcon)
                }
            }
        }
    }
    RingFileBrowser {
        id: fileBrowser
        onChosen: path => value.text = path
    }
}
