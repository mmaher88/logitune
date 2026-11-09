import QtQuick
import QtQuick.Window
import Logitune

Window {
    id: overlay
    width: 380
    height: 380
    x: RadialMenu.overlayPosition.x
    y: RadialMenu.overlayPosition.y
    visible: RadialMenu.visible
    color: "transparent"
    flags: Qt.FramelessWindowHint | Qt.Tool | Qt.WindowStaysOnTopHint

    Component.onCompleted: RadialMenu.configureWindow(overlay)

    RadialMenuRing {
        anchors.fill: parent
        entries: RadialMenu.items
        onActivated: function(index) { RadialMenu.activate(index) }
        onCancelled: RadialMenu.cancel()
    }
}
