import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Logitune

ColumnLayout {
    id: root
    property string query: ""
    property var actions: []
    property var collapsed: ({})
    readonly property var filtered: actions.filter(a => (a.name + " " + a.group).toLowerCase().indexOf(query.toLowerCase()) >= 0)
    signal chosen(var entry)

    function glyph(name, group) {
        let icons = {Copy: "󰆏", Cut: "", Paste: "󰆒", Undo: "", Redo: "", Mute: "󰝟",
            "Volume up": "󰝝", "Volume down": "󰝞", "Play/Pause": "󰐎", "Next track": "󰒭",
            "Previous track": "󰒮", "Back": "", "Forward": "", "Screenshot": "󰹑"}
        return icons[name] || ({Media: "󰎆", System: "󰒓", Window: "", Workspace: "󰍹", Edit: "󰏫"}[group] || "󰌌")
    }
    function populate() {
        let result = []
        for (let i = 0; i < ActionModel.rowCount(); ++i) {
            let row = ActionModel.get(i)
            let type = row.type
            if (["keystroke", "media", "preset", "app-launch", "dbus"].indexOf(type) < 0) continue
            let name = row.name
            let payload = row.payload
            let group = row.category
            result.push({name: name, group: group, icon: glyph(name, group),
                action: type + ":" + payload, kind: type === "keystroke" && !payload ? "keystroke" : ""})
        }
        result = result.concat([
            {name: "Open application", group: "Open", kind: "app", icon: "󰀻"},
            {name: "Open URL", group: "Open", kind: "url", icon: "󰖟"},
            {name: "Open file", group: "Open", kind: "file", icon: "󰈔"},
            {name: "Open folder", group: "Open", kind: "folder", icon: "󰉋"},
            {name: "Custom command", group: "Open", kind: "command", icon: "\uf120"},
            // An adjuster: hover it on the ring and scroll. PipeWire's wpctl; -l 1.0 caps at 100%.
            {name: "Volume", group: "Media", icon: "\uf028", action: "default", adjust: {
                up: "wpctl set-volume -l 1.0 @DEFAULT_AUDIO_SINK@ 5%+",
                down: "wpctl set-volume @DEFAULT_AUDIO_SINK@ 5%-",
                read: "wpctl get-volume @DEFAULT_AUDIO_SINK@",
                press: "wpctl set-mute @DEFAULT_AUDIO_SINK@ toggle"}},
            {name: "Submenu", group: "Organize", children: [], icon: "󰕰"}])
        // The list draws a section header wherever the group changes, so entries of one group
        // must be adjacent. Keep groups in the order they first appear.
        let groupOrder = []
        for (let entry of result) {
            if (groupOrder.indexOf(entry.group) < 0)
                groupOrder.push(entry.group)
        }
        let grouped = []
        for (let group of groupOrder)
            grouped = grouped.concat(result.filter(entry => entry.group === group))
        actions = grouped
    }
    Component.onCompleted: populate()
    Connections {
        target: ActionModel
        function onModelReset() { root.populate() }
        function onRowsInserted() { root.populate() }
        function onRowsRemoved() { root.populate() }
    }
    spacing: 12
    Label { text: "All actions"; color: Theme.text; font { pixelSize: 20; bold: true } }
    RingTextField {
        objectName: "actionSearch"
        Layout.fillWidth: true
        placeholderText: "Search actions"
        text: root.query
        onTextEdited: root.query = text
    }
    Label {
        Layout.fillWidth: true
        text: "Drag onto a bubble, or select a bubble and click an action."
        wrapMode: Text.WordWrap; color: Theme.textSecondary; font.pixelSize: 12
    }
    ListView {
        id: list
        Layout.fillWidth: true; Layout.fillHeight: true
        clip: true; model: root.filtered
        section.property: "group"
        section.criteria: ViewSection.FullString
        // Drawn by hand: a stock ItemDelegate uses Qt's light default style and ignores the app theme.
        section.delegate: Rectangle {
            id: header
            required property string section
            width: list.width
            height: 36
            color: headerHover.hovered ? Theme.hoverBg : "transparent"
            radius: 6

            Text {
                anchors.verticalCenter: parent.verticalCenter
                x: 8
                text: (root.collapsed[header.section] && !root.query ? "▸  " : "▾  ") + header.section
                color: Theme.textSecondary
                font.pixelSize: 12
                font.bold: true
                font.capitalization: Font.AllUppercase
            }
            HoverHandler { id: headerHover }
            TapHandler {
                onTapped: {
                    let value = Object.assign({}, root.collapsed)
                    value[header.section] = !value[header.section]
                    root.collapsed = value
                }
            }
        }
        delegate: Item {
            id: row
            required property var modelData
            width: list.width
            height: root.collapsed[modelData.group] && !root.query ? 0 : 44
            visible: height > 0
            Rectangle {
                id: tile
                width: row.width; height: 42; radius: 6
                color: hover.hovered || drag.active ? Theme.hoverBg : Theme.cardBg
                border.color: Theme.border
                property var ringSource: ({entry: row.modelData})
                Drag.active: drag.active
                Drag.keys: ["ring-action"]
                Drag.hotSpot: Qt.point(width / 2, height / 2)
                Drag.source: tile
                z: drag.active ? 100 : 0
                RowLayout {
                    anchors.fill: parent; anchors.margins: 8; spacing: 12
                    Item { Layout.preferredWidth: 26; Layout.fillHeight: true
                        Glyph { icon: row.modelData.icon; color: Theme.text; pixelSize: 20 }
                    }
                    Label { text: row.modelData.name; color: Theme.text; Layout.fillWidth: true; elide: Text.ElideRight }
                }
                HoverHandler { id: hover }
                TapHandler { onTapped: root.chosen(row.modelData) }
                DragHandler {
                    id: drag
                    onActiveChanged: {
                        if (active) {
                            let point = tile.mapToItem(Overlay.overlay, 0, 0)
                            tile.parent = Overlay.overlay; tile.x = point.x; tile.y = point.y
                        } else {
                            tile.Drag.drop(); tile.parent = row; tile.x = 0; tile.y = 0
                        }
                    }
                }
            }
        }
        ScrollBar.vertical: ScrollBar {}
    }
    Label { visible: root.filtered.length === 0; text: "No matching actions"; color: Theme.textSecondary }
}
