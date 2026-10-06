import QtQuick
import QtQuick.Controls
import Logitune

ComboBox {
    id: root
    implicitHeight: 40
    leftPadding: 12
    rightPadding: 30
    font.pixelSize: 13
    contentItem: Text {
        text: root.displayText
        font: root.font
        color: Theme.text
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }
    background: Rectangle {
        radius: 8
        color: Theme.cardBg
        border.color: root.activeFocus ? Theme.accent : Theme.cardBorder
    }
    indicator: Text {
        x: root.width - 23
        anchors.verticalCenter: parent.verticalCenter
        text: "⌄"
        color: Theme.textSecondary
    }
    delegate: ItemDelegate {
        required property int index
        width: root.width
        text: root.textAt(index)
        highlighted: root.highlightedIndex === index
        contentItem: Text {
            text: parent.text
            color: Theme.text
            font.pixelSize: 13
            elide: Text.ElideRight
        }
        background: Rectangle {
            color: parent.highlighted ? Theme.hoverBg : Theme.cardBg
        }
    }
    popup: Popup {
        y: root.height + 4
        width: root.width
        padding: 4
        implicitHeight: Math.min(contentItem.implicitHeight + 8, 280)
        background: Rectangle {
            radius: 8
            color: Theme.cardBg
            border.color: Theme.cardBorder
        }
        contentItem: ListView {
            clip: true
            implicitHeight: contentHeight
            model: root.popup.visible ? root.delegateModel : null
            currentIndex: root.highlightedIndex
            ScrollIndicator.vertical: ScrollIndicator {}
        }
    }
}
