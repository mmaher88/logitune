import QtQuick
import QtQuick.Controls
import Logitune

Button {
    id: root
    property bool primary: false
    implicitHeight: 40
    implicitWidth: Math.max(40, contentItem.implicitWidth + 28)
    padding: 12
    font.pixelSize: 13
    hoverEnabled: true
    opacity: enabled ? 1 : 0.4
    background: Rectangle {
        radius: 8
        color: root.primary ? (root.hovered ? Theme.accentHover : Theme.accent) : root.hovered ? Theme.hoverBg : Theme.cardBg
        border.width: 1
        border.color: root.activeFocus ? Theme.accent : root.primary ? Theme.accent : Theme.cardBorder
        Behavior on color {
            ColorAnimation {
                duration: 120
            }
        }
    }
    contentItem: Text {
        text: root.text
        font: root.font
        color: root.primary ? Theme.activeTabText : Theme.text
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }
}
