import QtQuick
import QtQuick.Controls
import Logitune

// A button in Logitune's colours. "primary" fills it with the accent, for the main action in a card.
Button {
    id: control

    property bool primary: false

    implicitHeight: 32
    leftPadding: 12
    rightPadding: 12
    font.pixelSize: 13

    contentItem: Text {
        text: control.text
        font: control.font
        color: control.primary ? Theme.activeTabText : Theme.text
        opacity: control.enabled ? 1 : 0.4
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    background: Rectangle {
        radius: 6
        color: {
            if (control.primary)
                return control.hovered ? Theme.accentHover : Theme.accent
            return control.hovered ? Theme.hoverBg : Theme.cardBg
        }
        border.color: control.primary ? "transparent" : Theme.border
        opacity: control.enabled ? 1 : 0.5
    }
}
