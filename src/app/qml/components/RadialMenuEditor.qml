pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Logitune

ScrollView {
    id: root
    clip: true
    property int editingIndex: -1
    property string error: ""

    function modelValue(index, role) {
        return RadialActionModel.data(RadialActionModel.index(index, 0), role)
    }

    function serializedAt(index) {
        if (index < 0) return ""
        var type = modelValue(index, 0x103)
        var payload = modelValue(index, 0x104)
        if (type === "none" || type === "default") return "default"
        if (type === "keystroke" || type === "preset") return type + ":" + payload
        return type
    }

    function findAction(serialized) {
        for (var i = 0; i < RadialActionModel.rowCount(); ++i) {
            if (serializedAt(i) === serialized) return i
        }
        return -1
    }

    function persist(items) {
        error = DeviceModel.saveRadialMenu(items)
            ? "" : qsTr("Enter a name and choose a valid action.")
        return error.length === 0
    }

    function editEntry(index) {
        editingIndex = index
        var entry = DeviceModel.radialItems[index]
        labelField.text = entry.label
        var found = findAction(entry.action)
        catalog.currentIndex = Math.max(0, found)
        actionKind.currentIndex = found >= 0 ? 0
            : entry.action.indexOf("app-launch:") === 0 ? 2 : 1
        payloadField.text = entry.action.indexOf(":") >= 0
            ? entry.action.substring(entry.action.indexOf(":") + 1) : ""
        error = ""
        editDialog.open()
    }

    ColumnLayout {
        width: root.availableWidth
        spacing: 8

        Label {
            Layout.fillWidth: true
            Layout.leftMargin: 16
            Layout.rightMargin: 16
            Layout.topMargin: 10
            text: qsTr("Choose 2–8 actions. Items run clockwise from the top.")
            wrapMode: Text.Wrap
            color: Theme.textSecondary
        }

        RadialMenuRing {
            Layout.fillWidth: true
            Layout.preferredHeight: Math.min(300, root.availableWidth)
            entries: DeviceModel.radialItems
            editing: true
            onActionClicked: function(index) { root.editEntry(index) }
        }

        Repeater {
            model: DeviceModel.radialItems
            delegate: RowLayout {
                id: entryRow
                required property int index
                required property var modelData
                Layout.fillWidth: true
                Layout.leftMargin: 16
                Layout.rightMargin: 16

                Button {
                    Layout.fillWidth: true
                    text: (entryRow.index + 1) + ". " + entryRow.modelData.label
                    icon.source: "qrc:/Logitune/qml/assets/actions/"
                        + iconProbe.glyph + ".svg"
                    icon.color: Theme.text
                    onClicked: root.editEntry(entryRow.index)
                    RadialActionIcon {
                        id: iconProbe
                        visible: false
                        action: entryRow.modelData.action
                    }
                }
                ToolButton {
                    text: "↑"
                    enabled: entryRow.index > 0
                    Accessible.name: qsTr("Move up")
                    onClicked: {
                        var items = DeviceModel.radialItems.slice()
                        var item = items.splice(entryRow.index, 1)[0]
                        items.splice(entryRow.index - 1, 0, item)
                        root.persist(items)
                    }
                }
                ToolButton {
                    text: "×"
                    enabled: DeviceModel.radialItems.length > 2
                    Accessible.name: qsTr("Remove item")
                    onClicked: {
                        var items = DeviceModel.radialItems.slice()
                        items.splice(entryRow.index, 1)
                        root.persist(items)
                    }
                }
            }
        }

        Button {
            Layout.leftMargin: 16
            Layout.bottomMargin: 12
            text: qsTr("Add item")
            enabled: DeviceModel.radialItems.length < 8
            onClicked: {
                root.editingIndex = DeviceModel.radialItems.length
                labelField.text = ""
                payloadField.text = ""
                actionKind.currentIndex = 0
                catalog.currentIndex = 0
                root.error = ""
                editDialog.open()
            }
        }
    }

    Dialog {
        id: editDialog
        objectName: "radialEditDialog"
        parent: Overlay.overlay
        anchors.centerIn: parent
        width: Math.min(440, parent ? parent.width - 32 : 440)
        modal: true
        title: qsTr("Action Ring item")

        ColumnLayout {
            width: parent.width
            TextField {
                id: labelField
                objectName: "radialLabelField"
                Layout.fillWidth: true
                placeholderText: qsTr("Name")
                maximumLength: 80
            }
            ComboBox {
                id: actionKind
                objectName: "radialActionKind"
                Layout.fillWidth: true
                model: [qsTr("Existing action"), qsTr("Keyboard shortcut"),
                        qsTr("Launch command")]
            }
            ComboBox {
                id: catalog
                objectName: "radialActionCatalog"
                Layout.fillWidth: true
                visible: actionKind.currentIndex === 0
                model: RadialActionModel
                textRole: "name"
                displayText: qsTranslate("ActionModel", currentText)
                delegate: ItemDelegate {
                    required property string name
                    required property int index
                    width: catalog.width
                    text: qsTranslate("ActionModel", name)
                    highlighted: catalog.highlightedIndex === index
                }
                onActivated: {
                    if (!labelField.text.length)
                        labelField.text = qsTranslate("ActionModel", currentText)
                    if (root.serializedAt(currentIndex) === "keystroke:")
                        actionKind.currentIndex = 1
                }
            }
            TextField {
                id: payloadField
                objectName: "radialPayloadField"
                Layout.fillWidth: true
                visible: actionKind.currentIndex === 1 || actionKind.currentIndex === 2
                placeholderText: actionKind.currentIndex === 1
                    ? "Ctrl+Shift+C" : "firefox --new-window"
            }
            KeystrokeCapture {
                Layout.fillWidth: true
                visible: actionKind.currentIndex === 1
                keystroke: payloadField.text
                onKeystrokeCaptured: function(keystroke) {
                    payloadField.text = keystroke
                }
            }
            Label {
                Layout.fillWidth: true
                visible: root.error.length > 0
                text: root.error
                color: Theme.text
                wrapMode: Text.Wrap
            }
            RowLayout {
                Layout.alignment: Qt.AlignRight
                Button { text: qsTr("Cancel"); onClicked: editDialog.close() }
                Button {
                    objectName: "saveRadialEntry"
                    text: qsTr("Save")
                    onClicked: {
                        var action = actionKind.currentIndex === 0
                            ? root.serializedAt(catalog.currentIndex)
                            : actionKind.currentIndex === 1
                                ? "keystroke:" + payloadField.text.trim()
                                : "app-launch:" + payloadField.text.trim()
                        var entry = {label: labelField.text.trim(), action: action}
                        var items = DeviceModel.radialItems.slice()
                        if (root.editingIndex === items.length) items.push(entry)
                        else items[root.editingIndex] = entry
                        if (root.persist(items)) editDialog.close()
                    }
                }
            }
        }
    }
}
