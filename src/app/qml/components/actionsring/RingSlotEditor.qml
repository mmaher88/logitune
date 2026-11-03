import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Logitune

ColumnLayout {
    id: root
    required property var entry
    property bool allowSubmenu: false
    property var choices: []
    signal edited(var value)

    function change(key, value) {
        let copy = Object.assign({}, entry)
        copy[key] = value
        edited(copy)
    }
    Component.onCompleted: {
        let list = [{name: "Choose an action…", action: "default"}]
        for (let i = 0; i < ActionModel.rowCount(); i++) {
            let index = ActionModel.index(i, 0)
            let type = ActionModel.data(index, 259)
            if (["keystroke", "media", "app-launch", "dbus", "preset"].indexOf(type) < 0)
                continue
            list.push({name: ActionModel.data(index, 257), action: type + ":" + ActionModel.data(index, 260)})
        }
        choices = list
    }
    RowLayout {
        Layout.fillWidth: true
        TextField {
            Layout.preferredWidth: 100
            text: root.entry.icon || ""
            placeholderText: "Icon / glyph"
            color: Theme.text
            onEditingFinished: root.change("icon", text)
        }
        TextField {
            Layout.fillWidth: true
            text: root.entry.name || ""
            placeholderText: "Name"
            color: Theme.text
            onEditingFinished: root.change("name", text)
        }
    }
    CheckBox {
        visible: root.allowSubmenu
        text: "Submenu"
        checked: root.entry.children !== undefined
        onToggled: {
            let copy = Object.assign({}, root.entry)
            if (checked) copy.children = [{icon: "", name: "New action", action: "default"}]
            else delete copy.children
            root.edited(copy)
        }
    }
    ComboBox {
        Layout.fillWidth: true
        visible: root.entry.children === undefined
        model: root.choices
        textRole: "name"
        onActivated: root.change("action", root.choices[currentIndex].action)
    }
    TextField {
        Layout.fillWidth: true
        visible: root.entry.children === undefined
        text: root.entry.action || "default"
        placeholderText: "keystroke:Ctrl+C / app-launch:kitty"
        color: Theme.text
        onEditingFinished: root.change("action", text)
    }
}
