import QtQuick
import QtQuick.Controls

// Logitune-styled horizontal slider
// Purple fill (#814EFA), gray track, circular white thumb with purple border
Item {
    id: root

    property alias value:    slider.value
    property alias from:     slider.from
    property alias to:       slider.to
    property alias stepSize: slider.stepSize
    property alias pressed:  slider.pressed
    property string label:   ""

    implicitHeight: col.implicitHeight
    implicitWidth:  200

    Column {
        id: col
        width: parent.width
        spacing: 4

        // Label + value row — subtitle style: 16px bold, space-between, mb 14px
        Row {
            width: parent.width
            visible: root.label.length > 0
            bottomPadding: 14

            Text {
                text: root.label
                font.pixelSize: 16
                font.bold: true
                color: "#444444"
                width: parent.width - valueLabel.width
            }
            Text {
                id: valueLabel
                text: Math.round(slider.value) + "%"
                font.pixelSize: 16
                color: "#814EFA"
                font.bold: true
            }
        }

        Slider {
            id: slider
            width: parent.width
            height: 32
            from: 0
            to: 100
            value: 50
            stepSize: 1

            background: Rectangle {
                x: slider.leftPadding
                y: slider.topPadding + slider.availableHeight / 2 - height / 2
                width:  slider.availableWidth
                height: 4
                radius: 2
                color:  "#E1E2E3"

                Rectangle {
                    width:  slider.visualPosition * parent.width
                    height: parent.height
                    radius: 2
                    color:  "#814EFA"
                }
            }

            handle: Rectangle {
                x: slider.leftPadding + slider.visualPosition * (slider.availableWidth - width)
                y: slider.topPadding + slider.availableHeight / 2 - height / 2
                width:  16
                height: 16
                radius: 8
                color:  "#FFFFFF"
                border.color: "#814EFA"
                border.width: 5

                // Focus ring
                Rectangle {
                    anchors.centerIn: parent
                    width: parent.width + 6
                    height: parent.height + 6
                    radius: (parent.width + 6) / 2
                    color: "transparent"
                    border.color: Qt.rgba(0.506, 0.306, 0.980, 0.3)
                    border.width: 2
                    visible: slider.activeFocus
                }
            }
        }
    }
}
