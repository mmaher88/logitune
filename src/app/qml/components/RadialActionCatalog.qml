import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Logitune

ColumnLayout {
    id: root
    spacing: 12
    property bool showSearch: false
    property var specialEntries: []
    property bool inFolder: false
    property Item dragLayer: root
    property string expandedCategory: "launch"
    readonly property string query: search.text.trim().toLowerCase()
    readonly property var categories: [
        {key: "launch", title: qsTr("Launch")},
        {key: "keyboard", title: qsTr("Keyboard and editing")},
        {key: "media", title: qsTr("Media and volume")},
        {key: "navigation", title: qsTr("Navigation")},
        {key: "system", title: qsTr("System")},
        {key: "windows", title: qsTr("Windows and desktops")},
        {key: "device", title: qsTr("Device")},
        {key: "folders", title: qsTr("Folders")}
    ]
    readonly property var rows: buildRows()
    signal chosen(var entry)

    function reset() { search.clear(); showSearch = false; expandedCategory = "launch" }
    function categoryFor(entry) {
        if (entry.kind === "folder") return "folders"
        if (entry.action === "application-picker:" || entry.action === "app-launch:") return "launch"
        if (entry.action === "keystroke:") return "keyboard"
        if (entry.parameter === "volume") return "media"
        return "system"
    }
    function buildRows() {
        var buckets = {}
        var result = []
        for (var c of categories) buckets[c.key] = []
        for (var i = 0; i < specialEntries.length; ++i) {
            var entry = specialEntries[i]
            if (inFolder && entry.kind === "folder") continue
            var row = {entry: entry, objectName: "catalogSpecial-" + i, searchText: entry.label, description: ""}
            var key = categoryFor(entry)
            if (entry.action === "application-picker:") buckets[key].unshift(row)
            else buckets[key].push(row)
        }
        var mapping = {Edit: "keyboard", Media: "media", Navigation: "navigation",
            System: "system", Window: "windows", Workspace: "windows", Device: "device", Other: "keyboard"}
        var actions = RadialActionModel.entries
        for (var j = 0; j < actions.length; ++j) {
            var action = actions[j]
            if (action.action === "keystroke:" || action.action === "app-launch:") continue
            var label = qsTranslate("ActionModel", action.name)
            var description = qsTranslate("ActionModel", action.description)
            buckets[mapping[action.category] || "system"].push({
                entry: {label: label, action: action.action}, objectName: "catalogAction-" + j,
                description: description, searchText: label + " " + description + " " + action.name + " " + action.category
            })
        }
        for (var category of categories) {
            var matching = buckets[category.key].filter(function(row) {
                return (row.searchText + " " + category.title).toLowerCase().indexOf(query) >= 0
            })
            if (!matching.length) continue
            var expanded = query.length > 0 || expandedCategory === category.key
            result.push({header: true, key: category.key, title: category.title, count: matching.length, expanded: expanded})
            if (expanded) {
                for (var match of matching) {
                    match.header = false
                    match.subtitle = match.description || category.title
                    result.push(match)
                }
            }
        }
        return result
    }
    RowLayout {
        Layout.fillWidth: true; spacing: 12
        RadialActionIcon { Layout.preferredWidth: 20; Layout.preferredHeight: 20; customIcon: "grid" }
        Text { Layout.fillWidth: true; text: qsTr("All actions"); color: Theme.text; font.pixelSize: 15; font.bold: true }
        RingIconButton {
            objectName: "toggleRingSearch"
            glyph: "search"; hint: qsTr("Search actions…"); checked: root.showSearch
            onClicked: {
                root.showSearch = !root.showSearch
                if (root.showSearch) search.forceActiveFocus()
                else search.clear()
            }
        }
    }
    Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: Theme.border }
    RowLayout {
        Layout.fillWidth: true; visible: root.showSearch
        RingTextField {
            id: search; objectName: "ringSearch"; Layout.fillWidth: true; placeholderText: qsTr("Search actions…")
            onTextChanged: if (text.length) root.showSearch = true
        }
        RingIconButton {
            objectName: "clearRingSearch"
            glyph: "close"; visible: search.text.length > 0; hint: qsTr("Clear search")
            onClicked: { search.clear(); search.forceActiveFocus() }
        }
    }
    ListView {
        id: list
        objectName: "ringCatalogList"
        Layout.fillWidth: true
        Layout.fillHeight: true
        Layout.preferredHeight: 460
        Layout.minimumHeight: 220
        clip: true; spacing: 6
        contentWidth: width
        boundsBehavior: Flickable.StopAtBounds
        model: root.rows
        onModelChanged: positionViewAtBeginning()
        ScrollBar.vertical: ScrollBar {}
        delegate: Item {
            id: rowItem
            required property var modelData
            width: list.width
            height: 46
            RingButton {
                anchors.fill: parent
                visible: rowItem.modelData.header
                objectName: visible ? "catalogCategory-" + rowItem.modelData.key : ""
                enabled: !root.query.length
                opacity: 1
                background: Rectangle {
                    radius: 6
                    color: rowItem.modelData.expanded ? (Theme.dark ? "#203634" : "#EDE5FF") : parent.hovered ? Theme.hoverBg : Theme.surface
                    border.color: rowItem.modelData.expanded ? "transparent" : Theme.border
                }
                Accessible.name: rowItem.modelData.title || ""
                contentItem: RowLayout {
                    spacing: 8
                    RadialActionIcon {
                        Layout.preferredWidth: 16; Layout.preferredHeight: 16; customIcon: "chevron"
                        rotation: rowItem.modelData.expanded ? 180 : 0
                    }
                    Text {
                        Layout.fillWidth: true; text: rowItem.modelData.title || ""
                        color: Theme.text; font.pixelSize: 12; font.bold: true; elide: Text.ElideRight
                    }

                }
                onClicked: root.expandedCategory = rowItem.modelData.expanded ? "" : rowItem.modelData.key
            }
            RingCatalogItem {
                anchors.fill: parent
                anchors.leftMargin: 8; anchors.rightMargin: 8
                visible: !rowItem.modelData.header
                objectName: visible ? rowItem.modelData.objectName : ""
                entry: rowItem.modelData.entry || ({})
                subtitle: rowItem.modelData.subtitle || ""
                dragLayer: root.dragLayer
                onChosen: function(entry) { root.chosen(entry) }
            }
        }
        Text {
            objectName: "catalogNoResults"
            anchors.centerIn: parent; width: parent.width - 24
            visible: list.count === 0
            text: qsTr("No actions found. Try another search or clear the search field.")
            wrapMode: Text.Wrap; horizontalAlignment: Text.AlignHCenter
            color: Theme.textSecondary; font.pixelSize: 13
        }
    }
}
