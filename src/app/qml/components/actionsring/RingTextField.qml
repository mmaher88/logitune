import QtQuick
import QtQuick.Controls
import Logitune

// A text field in Logitune's colours. The default Qt style draws a white box that ignores the theme.
TextField {
    id: field

    implicitHeight: 32
    leftPadding: 10
    rightPadding: 10
    color: Theme.text
    placeholderTextColor: Theme.textSecondary
    selectionColor: Theme.accent
    selectedTextColor: Theme.activeTabText
    font.pixelSize: 13

    background: Rectangle {
        radius: 6
        color: Theme.inputBg
        border.color: field.activeFocus ? Theme.accent : "transparent"
        border.width: 1
    }
}
