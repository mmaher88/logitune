import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Logitune

ScrollView {
    id: root
    clip: true
    property int editingIndex: -1
    property string error: ""
    function persist(items, click) {
        error = DeviceModel.saveRadialMenu(items, click) ? "" : qsTr("Enter a name and a valid action.")
        return error.length === 0
    }
    function editEntry(index) {
        editingIndex = index
        var entry = index < 0 ? DeviceModel.radialClick : DeviceModel.radialItems[index]
        labelField.text = entry.label
        var action = entry.action
        var found = catalog.indexOfValue(action)
        catalog.currentIndex = Math.max(0, found)
        actionKind.currentIndex = action === "default" ? 3 : found >= 0 ? 0 : action.indexOf("app-launch:") === 0 ? 2 : 1
        payloadField.text = action.substring(action.indexOf(":") + 1)
        error = ""
        editDialog.open()
    }
    ColumnLayout {
        width: root.availableWidth
        spacing: 8
        Label {
            Layout.fillWidth: true
            text: qsTr("Hold the button, move to choose, release to run. Return to the center to cancel. Items are ordered clockwise from the top.")
            wrapMode: Text.Wrap
            color: Theme.textSecondary
        }
        RadialMenuRing {
            Layout.fillWidth: true
            Layout.preferredHeight: Math.min(300, root.availableWidth)
            items: DeviceModel.radialItems
            interactive: true
            onActionClicked: function(index) { root.editEntry(index) }
        }
        Repeater {
            model: DeviceModel.radialItems
            delegate: RowLayout {
                required property int index
                required property var modelData
                Layout.fillWidth: true
                Button {
                    Layout.fillWidth: true
                    text: (index + 1) + ". " + qsTranslate("ActionModel", modelData.label)
                    onClicked: root.editEntry(index)
                }
                ToolButton {
                    text: "↑"
                    enabled: index > 0
                    Accessible.name: qsTr("Move up")
                    onClicked: {
                        var items = DeviceModel.radialItems.slice()
                        var item = items.splice(index, 1)[0]
                        items.splice(index - 1, 0, item)
                        root.persist(items, DeviceModel.radialClick)
                    }
                }
                ToolButton {
                    text: "×"
                    enabled: DeviceModel.radialItems.length > 2
                    Accessible.name: qsTr("Remove item")
                    onClicked: {
                        var items = DeviceModel.radialItems.slice()
                        items.splice(index, 1)
                        root.persist(items, DeviceModel.radialClick)
                    }
                }
            }
        }
        Button {
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
        Button {
            Layout.fillWidth: true
            text: qsTr("Short press: %1").arg(DeviceModel.radialClick.action === "default"
                  ? qsTr("None") : DeviceModel.radialClick.label)
            onClicked: root.editEntry(-1)
        }
    }
    Dialog {
        id: editDialog
        objectName: "editDialog"
        parent: Overlay.overlay
        anchors.centerIn: parent
        width: Math.min(440, parent ? parent.width - 32 : 440)
        modal: true
        title: root.editingIndex < 0 ? qsTr("Short press") : qsTr("Menu item")
        ColumnLayout {
            width: parent.width
            TextField {
                id: labelField
                objectName: "labelField"
                Layout.fillWidth: true
                placeholderText: qsTr("Name")
                maximumLength: 80
            }
            ComboBox {
                id: actionKind
                objectName: "actionKind"
                Layout.fillWidth: true
                model: [qsTr("Existing action"), qsTr("Keyboard shortcut"), qsTr("Launch command"), qsTr("None")]
            }
            ComboBox {
                id: catalog
                objectName: "catalog"
                Layout.fillWidth: true
                visible: actionKind.currentIndex === 0
                model: RadialActionModel
                textRole: "name"
                valueRole: "serializedAction"
                displayText: qsTranslate("ActionModel", currentText)
                delegate: ItemDelegate {
                    required property string name
                    required property int index
                    width: catalog.width
                    text: qsTranslate("ActionModel", name)
                    highlighted: catalog.highlightedIndex === index
                }
                onActivated: {
                    if (!labelField.text.length) labelField.text = qsTranslate("ActionModel", currentText)
                    if (currentValue === "keystroke:") actionKind.currentIndex = 1
                }
            }
            TextField {
                id: payloadField
                objectName: "payloadField"
                Layout.fillWidth: true
                visible: actionKind.currentIndex === 1 || actionKind.currentIndex === 2
                placeholderText: actionKind.currentIndex === 1 ? "Ctrl+Shift+C" : "firefox --new-window"
            }
            KeystrokeCapture {
                Layout.fillWidth: true
                visible: actionKind.currentIndex === 1
                keystroke: payloadField.text
                onKeystrokeCaptured: function(ks) { payloadField.text = ks }
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
                    objectName: "saveEntry"
                    text: qsTr("Save")
                    onClicked: {
                        var action = actionKind.currentIndex === 0 ? catalog.currentValue
                                   : actionKind.currentIndex === 1 ? "keystroke:" + payloadField.text.trim()
                                   : actionKind.currentIndex === 2 ? "app-launch:" + payloadField.text.trim() : "default"
                        var entry = {label: labelField.text.trim(), action: action || ""}
                        var items = DeviceModel.radialItems.slice()
                        var click = DeviceModel.radialClick
                        if (root.editingIndex < 0) click = entry
                        else if (root.editingIndex === items.length) items.push(entry)
                        else items[root.editingIndex] = entry
                        if (root.persist(items, click)) editDialog.close()
                    }
                }
            }
        }
    }
}
