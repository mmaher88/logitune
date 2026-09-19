import QtQuick
import QtQuick.Window
import Logitune

Window {
    id: overlay
    width: 380
    height: 380
    visible: RadialMenu.visible
    transientParent: null
    title: "Logitune radial menu"
    Component.onCompleted: RadialMenu.configureWindow(overlay)
    onClosing: RadialMenu.cancel()
    color: "transparent"
    flags: Qt.Tool | Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint
           | Qt.WindowDoesNotAcceptFocus | Qt.WindowTransparentForInput
    x: RadialMenu.screenGeometry.x + (RadialMenu.screenGeometry.width - width) / 2
    y: RadialMenu.screenGeometry.y + (RadialMenu.screenGeometry.height - height) / 2

    RadialMenuRing {
        anchors.fill: parent
        anchors.margins: 10
        items: RadialMenu.items
        selected: RadialMenu.selected
    }
}
