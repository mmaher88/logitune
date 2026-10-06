import QtQuick
import QtQuick.Controls
import Logitune

MenuItem {
    id: root
    implicitHeight: 40
    implicitWidth: 240
    opacity: enabled ? 1 : 0.4
    contentItem: Text {
        text: root.text
        color: Theme.text
        font.pixelSize: 13
        verticalAlignment: Text.AlignVCenter
        leftPadding: 12
        rightPadding: 12
        elide: Text.ElideRight
    }
    background: Rectangle { radius: 6; color: root.highlighted ? Theme.hoverBg : "transparent" }
}
