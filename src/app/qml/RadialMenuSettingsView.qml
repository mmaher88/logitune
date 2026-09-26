import QtQuick
import QtQuick.Controls
import Logitune

Rectangle {
    id: root
    objectName: "actionRingSettings"
    color: Theme.background
    signal buttonsRequested()
    readonly property bool hasDevice: DeviceModel.count > 0 && DeviceModel.selectedIndex >= 0

    RadialMenuEditor {
        objectName: "actionRingEditor"
        anchors.fill: parent
        navigationEnabled: true
        visible: root.hasDevice
        enabled: root.hasDevice
        onBackRequested: if (root.StackView.view) root.StackView.view.pop()
        onButtonsRequested: root.buttonsRequested()
    }
    RingIconButton {
        x: 24; y: 24
        visible: !root.hasDevice
        glyph: "back"; hint: qsTr("Back")
        onClicked: if (root.StackView.view) root.StackView.view.pop()
    }
    Column {
        visible: !root.hasDevice
        anchors.centerIn: parent
        width: Math.min(400, parent.width - 48)
        spacing: 12
        Text {
            width: parent.width
            text: qsTr("Connect your mouse")
            color: Theme.text; font.pixelSize: 22; font.bold: true
            horizontalAlignment: Text.AlignHCenter
        }
        Text {
            width: parent.width
            text: qsTr("Connect a Logitech mouse to configure Action Ring.")
            color: Theme.textSecondary; font.pixelSize: 14
            wrapMode: Text.Wrap; horizontalAlignment: Text.AlignHCenter
        }
    }
}
