import QtQuick
import QtQuick.Controls
import Logitune

Item {
    id: root

    property string text: ""
    property color textColor: Theme.text
    property int pixelSize: 14
    property bool textBold: false
    property int fontWeight: Font.Normal
    property int horizontalAlignment: Text.AlignLeft
    readonly property bool editable: typeof EditorModel !== 'undefined' && EditorModel.editing

    signal commit(string newValue)

    implicitWidth: txt.implicitWidth
    implicitHeight: Math.max(txt.implicitHeight, field.implicitHeight)

    Text {
        id: txt
        anchors.fill: parent
        text: root.text
        color: root.textColor
        font.pixelSize: root.pixelSize
        font.bold: root.textBold
        font.weight: root.fontWeight
        verticalAlignment: Text.AlignVCenter
        horizontalAlignment: root.horizontalAlignment
        elide: Text.ElideRight
        visible: !field.visible
        Behavior on color { ColorAnimation { duration: 150 } }
    }

    TextField {
        id: field
        anchors.fill: parent
        text: root.text
        font.pixelSize: root.pixelSize
        font.bold: root.textBold
        font.weight: root.fontWeight
        visible: false
        padding: 0
        background: Rectangle {
            color: Theme.inputBg
            border.color: Theme.accent
            border.width: 1
            radius: 3
        }
        onAccepted: {
            root.commit(text)
            visible = false
        }
        Keys.onEscapePressed: {
            text = root.text
            visible = false
        }
        onActiveFocusChanged: {
            if (!activeFocus && visible) {
                root.commit(text)
                visible = false
            }
        }
    }

    MouseArea {
        anchors.fill: parent
        enabled: root.editable && !field.visible
        cursorShape: enabled ? Qt.IBeamCursor : Qt.ArrowCursor
        acceptedButtons: Qt.LeftButton
        propagateComposedEvents: true
        onDoubleClicked: {
            field.text = root.text
            field.visible = true
            field.forceActiveFocus()
            field.selectAll()
        }
        onPressed: function(mouse) { mouse.accepted = root.editable }
        onClicked: function(mouse) { mouse.accepted = false }
    }
}
