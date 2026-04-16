import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Logitune

Rectangle {
    id: root
    visible: typeof EditorModel !== 'undefined' && EditorModel.editing
    height: visible ? 36 : 0
    color: Theme.cardBg
    border.color: Theme.border
    border.width: visible ? 1 : 0

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 12
        anchors.rightMargin: 12
        spacing: 6

        // Path breadcrumb
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

        // Unsaved-changes indicator
        Rectangle {
            visible: root.visible && EditorModel.hasUnsavedChanges
            width: 6; height: 6; radius: 3
            color: Theme.accent
        }
        Text {
            visible: root.visible && EditorModel.hasUnsavedChanges
            text: "Unsaved changes"
            font.pixelSize: 10
            color: Theme.accent
        }

        // --- Action buttons ---

        // Undo
        Rectangle {
            id: undoBtn
            property bool btnEnabled: root.visible && EditorModel.canUndo
            implicitWidth: undoRow.implicitWidth + 16
            implicitHeight: 28
            radius: 4
            opacity: btnEnabled ? 1.0 : 0.4
            color: undoHover.hovered && btnEnabled ? Theme.hoverBg : Theme.inputBg
            Behavior on color { ColorAnimation { duration: 150 } }
            Layout.alignment: Qt.AlignVCenter

            Row {
                id: undoRow
                anchors.centerIn: parent
                spacing: 4
                Text { text: "\u21B6"; font.pixelSize: 14; color: Theme.text }
                Text { text: "Undo"; font.pixelSize: 11; font.bold: true; color: Theme.text }
            }
            HoverHandler { id: undoHover }
            MouseArea {
                anchors.fill: parent
                cursorShape: parent.btnEnabled ? Qt.PointingHandCursor : Qt.ArrowCursor
                onClicked: if (parent.btnEnabled) EditorModel.undo()
            }
        }

        // Redo
        Rectangle {
            id: redoBtn
            property bool btnEnabled: root.visible && EditorModel.canRedo
            implicitWidth: redoRow.implicitWidth + 16
            implicitHeight: 28
            radius: 4
            opacity: btnEnabled ? 1.0 : 0.4
            color: redoHover.hovered && btnEnabled ? Theme.hoverBg : Theme.inputBg
            Behavior on color { ColorAnimation { duration: 150 } }
            Layout.alignment: Qt.AlignVCenter

            Row {
                id: redoRow
                anchors.centerIn: parent
                spacing: 4
                Text { text: "\u21B7"; font.pixelSize: 14; color: Theme.text }
                Text { text: "Redo"; font.pixelSize: 11; font.bold: true; color: Theme.text }
            }
            HoverHandler { id: redoHover }
            MouseArea {
                anchors.fill: parent
                cursorShape: parent.btnEnabled ? Qt.PointingHandCursor : Qt.ArrowCursor
                onClicked: if (parent.btnEnabled) EditorModel.redo()
            }
        }

        // Reset
        Rectangle {
            id: resetBtn
            property bool btnEnabled: root.visible && EditorModel.hasUnsavedChanges
            implicitWidth: resetRow.implicitWidth + 16
            implicitHeight: 28
            radius: 4
            opacity: btnEnabled ? 1.0 : 0.4
            color: resetHover.hovered && btnEnabled ? Theme.hoverBg : Theme.inputBg
            Behavior on color { ColorAnimation { duration: 150 } }
            Layout.alignment: Qt.AlignVCenter

            Row {
                id: resetRow
                anchors.centerIn: parent
                spacing: 4
                Text { text: "\u21BA"; font.pixelSize: 14; color: Theme.text }
                Text { text: "Reset"; font.pixelSize: 11; font.bold: true; color: Theme.text }
            }
            HoverHandler { id: resetHover }
            MouseArea {
                anchors.fill: parent
                cursorShape: parent.btnEnabled ? Qt.PointingHandCursor : Qt.ArrowCursor
                onClicked: if (parent.btnEnabled) EditorModel.reset()
            }
        }

        // Save (primary action — accent color)
        Rectangle {
            id: saveBtn
            property bool btnEnabled: root.visible && EditorModel.hasUnsavedChanges
            implicitWidth: saveRow.implicitWidth + 16
            implicitHeight: 28
            radius: 4
            opacity: btnEnabled ? 1.0 : 0.4
            color: btnEnabled
                ? (saveHover.hovered ? Theme.accentHover : Theme.accent)
                : Theme.inputBg
            Behavior on color { ColorAnimation { duration: 150 } }
            Layout.alignment: Qt.AlignVCenter

            Row {
                id: saveRow
                anchors.centerIn: parent
                spacing: 4
                Text {
                    text: "\u2713"
                    font.pixelSize: 14
                    color: saveBtn.btnEnabled ? Theme.activeTabText : Theme.text
                }
                Text {
                    text: "Save"
                    font.pixelSize: 11
                    font.bold: true
                    color: saveBtn.btnEnabled ? Theme.activeTabText : Theme.text
                }
            }
            HoverHandler { id: saveHover }
            MouseArea {
                anchors.fill: parent
                cursorShape: parent.btnEnabled ? Qt.PointingHandCursor : Qt.ArrowCursor
                onClicked: if (parent.btnEnabled) EditorModel.save()
            }
        }
    }
}
