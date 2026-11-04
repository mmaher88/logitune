import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Logitune

ColumnLayout {
    id: root
    required property var editor
    spacing: 10

    Label { text: "Selected action"; color: Theme.text; font.bold: true }

    RingTextField {
        objectName: "slotName"
        Layout.fillWidth: true
        text: root.editor.selectedEntry.name || ""
        placeholderText: "Action name"
        onEditingFinished: root.editor.rename(text)
    }

    RowLayout {
        RingFlatButton { text: "Choose icon…"; onClicked: picker.open() }
        RingFlatButton { text: "Clear"; onClicked: root.editor.clear() }
    }

    RingIconPicker {
        id: picker
        onPicked: icon => root.editor.setIcon(icon)
    }
}
