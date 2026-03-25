import QtQuick
import QtQuick.Controls

// Minimal styled slider for Logitune
Item {
    id: root

    property alias value: slider.value
    property alias from: slider.from
    property alias to: slider.to
    property alias stepSize: slider.stepSize
    property string label: ""

    implicitHeight: col.implicitHeight
    implicitWidth: 200

    Column {
        id: col
        width: parent.width
        spacing: 4

        // Label + value row
        Row {
            width: parent.width

            Text {
                text: root.label
                font.pixelSize: 12
                color: "#444444"
                width: parent.width - valueLabel.width
            }
            Text {
                id: valueLabel
                text: Math.round(slider.value) + "%"
                font.pixelSize: 12
                color: "#7B61FF"
                font.bold: true
            }
        }

        Slider {
            id: slider
            width: parent.width
            from: 0
            to: 100
            value: 50
            stepSize: 1

            background: Rectangle {
                x: slider.leftPadding
                y: slider.topPadding + slider.availableHeight / 2 - height / 2
                width: slider.availableWidth
                height: 4
                radius: 2
                color: "#E0E0E0"

                Rectangle {
                    width: slider.visualPosition * parent.width
                    height: parent.height
                    radius: 2
                    color: "#7B61FF"
                }
            }

            handle: Rectangle {
                x: slider.leftPadding + slider.visualPosition * (slider.availableWidth - width)
                y: slider.topPadding + slider.availableHeight / 2 - height / 2
                width: 16
                height: 16
                radius: 8
                color: "#7B61FF"
                border.color: "#FFFFFF"
                border.width: 2

                layer.enabled: true
                layer.effect: null

                Rectangle {
                    anchors.fill: parent
                    radius: parent.radius
                    color: "transparent"
                    border.color: "#7B61FF"
                    border.width: 1
                    visible: slider.pressed
                    scale: 1.3
                }
            }
        }
    }
}
