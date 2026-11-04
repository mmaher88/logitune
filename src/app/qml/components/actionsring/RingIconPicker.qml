import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Logitune

// Icon chooser for ring slots: a searchable grid of built-in Nerd Font glyphs, plus a free field
// that takes any glyph, a theme icon name ("firefox") or an image file.
Popup {
    id: picker

    signal picked(string icon)

    property string query: ""
    readonly property var icons: [
        ["copy clipboard", "󰆏"], ["paste clipboard", "󰆒"], ["cut scissors", "\uf0c4"],
        ["undo back", "\uf0e2"], ["redo forward", "\uf01e"], ["keyboard shortcut", "󰌌"],
        ["terminal command", "\uf120"], ["application grid", "󰀻"], ["browser web globe", "󰖟"],
        ["folder directory", "󰉋"], ["file document", "󰈔"], ["menu submenu", "󰕰"],
        ["play pause", "󰐎"], ["stop", "󰓛"], ["next skip", "󰒭"], ["previous", "󰒮"],
        ["volume sound up", "󰝝"], ["volume sound down", "󰝞"], ["mute", "󰝟"],
        ["music media", "󰎆"], ["headphones audio", "󰋋"], ["microphone", "󰍬"],
        ["settings gear", "󰒓"], ["home", "\uf015"], ["search", "\uf002"], ["star favorite", "\uf005"],
        ["heart", "\uf004"], ["bookmark", "\uf02e"], ["bell notification", "\uf0f3"], ["calendar", "\uf073"],
        ["clock time", "\uf017"], ["camera screenshot", "󰹑"], ["display desktop", "󰍹"],
        ["window", "\uf2d0"], ["close cross", "\uf00d"], ["lock security", "\uf023"], ["unlock", "\uf09c"],
        ["sun brightness", "\uf185"], ["moon dark", "\uf186"], ["power", "\uf011"], ["refresh", "\uf021"],
        ["left arrow", "\uf060"], ["right arrow", "\uf061"], ["up arrow", "\uf062"], ["down arrow", "\uf063"],
        ["mail email", "\uf0e0"], ["chat message", "\uf075"], ["code", "\uf121"], ["save disk", "\uf0c7"],
        ["download", "\uf019"], ["upload", "\uf093"], ["trash delete", "\uf1f8"], ["edit pencil", "󰏫"],
        ["colour picker eyedropper", "󰈊"], ["wifi network", "󰖩"], ["bluetooth", "󰂯"],
        ["git branch", "\ue725"], ["docker container", "\ue7b0"], ["game controller", "󰊗"]]

    function choose(icon) {
        picked(icon)
        close()
    }

    width: 320
    height: 400
    padding: 12
    modal: true
    onOpened: {
        query = ""
        search.text = ""
        custom.text = ""
    }

    background: Rectangle {
        radius: 8
        color: Theme.cardBg
        border.color: Theme.border
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 8

        RingTextField {
            id: search
            Layout.fillWidth: true
            placeholderText: "Search icons"
            onTextChanged: picker.query = text.toLowerCase()
        }

        GridView {
            id: grid
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            cellWidth: 46
            cellHeight: 46
            model: picker.icons.filter(entry => entry[0].indexOf(picker.query) >= 0)
            delegate: ItemDelegate {
                required property var modelData
                width: 42
                height: 42
                background: Rectangle {
                    radius: 6
                    color: parent.hovered ? Theme.hoverBg : "transparent"
                }
                Glyph { icon: parent.modelData[1]; color: Theme.text; pixelSize: 22 }
                ToolTip.visible: hovered
                ToolTip.text: modelData[0]
                onClicked: picker.choose(modelData[1])
            }
            ScrollBar.vertical: ScrollBar {}
        }

        Label {
            text: "Or your own: paste a glyph, a theme icon name, or pick an image"
            color: Theme.textSecondary
            font.pixelSize: 11
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 6

            RingTextField {
                id: custom
                Layout.fillWidth: true
                placeholderText: "e.g. 󰊠  or  firefox"
                onAccepted: if (text.trim()) picker.choose(text.trim())
            }
            RingFlatButton {
                text: "Image…"
                onClicked: imageDialog.open()
            }
            RingFlatButton {
                text: "Use"
                primary: true
                enabled: custom.text.trim().length > 0
                onClicked: picker.choose(custom.text.trim())
            }
        }
    }

    RingFileBrowser {
        id: imageDialog
        mode: "image"
        onChosen: path => picker.choose(path)
    }
}
