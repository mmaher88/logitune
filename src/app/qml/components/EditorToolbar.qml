import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Logitune

Rectangle {
    id: root
    visible: typeof EditorModel !== 'undefined' && EditorModel.editing
    height: visible ? 36 : 0
    color: Theme.background
    border.color: Theme.border
    border.width: visible ? 1 : 0

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 12
        anchors.rightMargin: 12
        spacing: 8

        Text {
            id: pathLabel
            text: root.visible && EditorModel.activeDevicePath
                  ? "\u2026/" + EditorModel.activeDevicePath.split("/").slice(-2).join("/")
                  : ""
            font.pixelSize: 11
            color: Theme.textSecondary
            elide: Text.ElideLeft
            Layout.fillWidth: true
        }

        Rectangle {
            id: dirtyDot
            visible: root.visible && EditorModel.hasUnsavedChanges
            width: 8; height: 8; radius: 4
            color: Theme.accent
        }
        Text {
            visible: root.visible && EditorModel.hasUnsavedChanges
            text: "Unsaved changes"
            font.pixelSize: 11
            color: Theme.text
        }

        Button {
            text: "Undo"
            enabled: root.visible && EditorModel.canUndo
            onClicked: EditorModel.undo()
        }
        Button {
            text: "Redo"
            enabled: root.visible && EditorModel.canRedo
            onClicked: EditorModel.redo()
        }
        Button {
            text: "Reset"
            enabled: root.visible && EditorModel.hasUnsavedChanges
            onClicked: EditorModel.reset()
        }
        Button {
            text: "Save"
            enabled: root.visible && EditorModel.hasUnsavedChanges
            onClicked: EditorModel.save()
        }
    }
}
