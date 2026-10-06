import QtQuick
import QtQuick.Controls
import Logitune

Button {
    id: root
    property string glyph: "more"
    property string hint: ""
    implicitWidth: 40
    implicitHeight: 40
    padding: 10
    hoverEnabled: true
    opacity: enabled ? 1 : 0.3
    Accessible.name: hint
    ToolTip.visible: hovered && hint.length > 0
    ToolTip.delay: 600
    ToolTip.text: hint
    background: Rectangle {
        radius: 8
        color: root.checked || root.down ? Theme.hoverBg : root.hovered ? Theme.cardBg : "transparent"
        border.width: root.activeFocus ? 1 : 0
        border.color: Theme.accent
    }
    contentItem: RadialActionIcon { customIcon: root.glyph }
}
