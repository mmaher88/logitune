import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Logitune

ScrollView {
    id: root
    property var entries: ActionsRing.entries
    property int selectedSlot: 0
    clip: true
    contentWidth: availableWidth

    function save(index, entry) {
        let copy = entries.slice()
        copy[index] = entry
        ActionsRing.setEntries(copy)
    }
    ColumnLayout {
        width: root.availableWidth
        spacing: 10
        Label { text: "Actions Ring"; color: Theme.text; font.bold: true }
        RowLayout {
            Label { text: "Size"; color: Theme.text }
            ComboBox {
                model: ["80%", "100%", "150%"]
                currentIndex: [80, 100, 150].indexOf(ActionsRing.size)
                onActivated: ActionsRing.size = [80, 100, 150][currentIndex]
            }
        }
        ComboBox {
            id: selected
            Layout.fillWidth: true
            model: root.entries.map((entry, index) => (index + 1) + ". " + entry.name)
            currentIndex: root.selectedSlot
            onActivated: root.selectedSlot = currentIndex
        }
        RingSlotEditor {
            Layout.fillWidth: true
            visible: root.selectedSlot >= 0
            entry: root.entries[root.selectedSlot] || ({})
            allowSubmenu: true
            onEdited: (value) => root.save(root.selectedSlot, value)
        }
        Repeater {
            model: (root.entries[root.selectedSlot] || {}).children || []
            ColumnLayout {
                id: childRow
                required property int index
                required property var modelData
                Layout.fillWidth: true
                Label { text: "Child " + (childRow.index + 1); color: Theme.textSecondary }
                RingSlotEditor {
                    Layout.fillWidth: true
                    entry: childRow.modelData
                    onEdited: (value) => {
                        let parent = Object.assign({}, root.entries[root.selectedSlot])
                        parent.children = parent.children.slice()
                        parent.children[childRow.index] = value
                        root.save(root.selectedSlot, parent)
                    }
                }
                Button {
                    text: "Remove child"
                    onClicked: {
                        let parent = Object.assign({}, root.entries[root.selectedSlot])
                        parent.children = parent.children.filter((entry, i) => i !== childRow.index)
                        if (!parent.children.length) delete parent.children
                        root.save(root.selectedSlot, parent)
                    }
                }
            }
        }
        Button {
            text: "Add child"
            visible: ((root.entries[root.selectedSlot] || {}).children || []).length > 0
            enabled: ((root.entries[root.selectedSlot] || {}).children || []).length < 8
            onClicked: {
                let parent = Object.assign({}, root.entries[root.selectedSlot])
                parent.children = parent.children.concat([{icon: "", name: "New action", action: "default"}])
                root.save(root.selectedSlot, parent)
            }
        }
    }
}
