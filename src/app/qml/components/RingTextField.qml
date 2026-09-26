import QtQuick
import QtQuick.Controls
import Logitune

TextField {
    id: root
    implicitHeight: 40
    leftPadding: 12
    rightPadding: 12
    font.pixelSize: 13
    color: Theme.text
    placeholderTextColor: Theme.textSecondary
    selectionColor: Theme.accent
    selectedTextColor: Theme.activeTabText
    background: Rectangle {
        radius: 8
        color: Theme.cardBg
        border.color: root.activeFocus ? Theme.accent : Theme.cardBorder
    }
}
