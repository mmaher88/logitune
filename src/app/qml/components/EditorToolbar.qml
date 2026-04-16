import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Logitune

Rectangle {
    id: root
    property bool onDevicePage: false
    visible: typeof EditorModel !== 'undefined' && EditorModel.editing && onDevicePage
    height: visible ? 36 : 0
    color: Theme.cardBg
    border.color: Theme.border
    border.width: visible ? 1 : 0

    component ToolBtn : Rectangle {
        id: btn
        property bool btnEnabled: true
        property bool primary: false
        property string icon: ""
        property string tooltip: ""
        signal pressed()

        implicitWidth: 32
        implicitHeight: 28
        radius: 4
        opacity: btnEnabled ? 1.0 : 0.4
        color: primary && btnEnabled
            ? (btnHover.hovered ? Theme.accentHover : Theme.accent)
            : (btnHover.hovered && btnEnabled ? Theme.hoverBg : Theme.inputBg)
        Behavior on color { ColorAnimation { duration: 150 } }
        Layout.alignment: Qt.AlignVCenter

        Text {
            anchors.centerIn: parent
            text: btn.icon
            font.pixelSize: 16
            color: btn.primary && btn.btnEnabled ? Theme.activeTabText : Theme.text
        }

        HoverHandler { id: btnHover }
        MouseArea {
            anchors.fill: parent
            cursorShape: btn.btnEnabled ? Qt.PointingHandCursor : Qt.ArrowCursor
            onClicked: if (btn.btnEnabled) btn.pressed()
            hoverEnabled: true
            ToolTip.visible: btnHover.hovered && btn.tooltip
            ToolTip.text: btn.tooltip
            ToolTip.delay: 500
        }
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 12
        anchors.rightMargin: 12
        spacing: 4

        ToolBtn {
            icon: "\u2713"
            tooltip: "Save"
            primary: true
            btnEnabled: root.visible && EditorModel.hasUnsavedChanges
            onPressed: EditorModel.save()
        }
        ToolBtn {
            icon: "\u21BA"
            tooltip: "Reset"
            btnEnabled: root.visible && EditorModel.hasUnsavedChanges
            onPressed: EditorModel.reset()
        }
        ToolBtn {
            icon: "\u21B6"
            tooltip: "Undo"
            btnEnabled: root.visible && EditorModel.canUndo
            onPressed: EditorModel.undo()
        }
        ToolBtn {
            icon: "\u21B7"
            tooltip: "Redo"
            btnEnabled: root.visible && EditorModel.canRedo
            onPressed: EditorModel.redo()
        }

        Item { Layout.fillWidth: true }

        Text {
            visible: root.visible && EditorModel.activeDevicePath
            text: root.visible && EditorModel.activeDevicePath
                  ? "\u2026/" + EditorModel.activeDevicePath.split("/").slice(-2).join("/")
                  : ""
            font.pixelSize: 11
            color: Theme.textSecondary
            elide: Text.ElideLeft
        }

        Rectangle {
            visible: root.visible && EditorModel.hasUnsavedChanges
            width: 6; height: 6; radius: 3
            color: Theme.accent
            Layout.leftMargin: 4
        }
        Text {
            visible: root.visible && EditorModel.hasUnsavedChanges
            text: "Unsaved"
            font.pixelSize: 10
            color: Theme.accent
        }
    }
}
