import QtQuick
import QtQuick.Window
import Logitune

Window {
    id: overlay
    width: RadialMenu.overlaySize
    height: RadialMenu.overlaySize
    visible: RadialMenu.visible
    transientParent: null
    title: "Logitune radial menu"
    Component.onCompleted: RadialMenu.configureWindow(overlay)
    onClosing: RadialMenu.cancel()
    color: "transparent"
    flags: Qt.Tool | Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint
           | Qt.WindowDoesNotAcceptFocus
    x: RadialMenu.overlayPosition.x
    y: RadialMenu.overlayPosition.y

    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        onClicked: RadialMenu.cancel()
    }

    RadialMenuRing {
        anchors.fill: parent
        anchors.margins: overlay.width * 10 / 380
        items: RadialMenu.items
        interactive: true
        inFolder: RadialMenu.inFolder
        title: RadialMenu.inFolder ? RadialMenu.folderName : qsTr("Action Ring")
        adjustmentIndex: RadialMenu.adjustmentIndex
        adjustmentSteps: RadialMenu.adjustmentSteps
        onAdjusted: function(index, steps) { RadialMenu.adjust(index, steps) }
        editing: false
        onHoveredChanged: RadialMenu.select(hovered)
        onActionClicked: function(index) { RadialMenu.activate(index) }
        onCloseRequested: RadialMenu.back()
    }
}
