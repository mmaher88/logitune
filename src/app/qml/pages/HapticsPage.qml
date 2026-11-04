import QtQuick
import QtQuick.Controls
import Logitune

// Haptic feedback strength and the thumb panel's press force. Both are settings of the mouse itself,
// so they don't change with the per-application profile. Sliders write to the mouse only when
// released: a drag would otherwise send dozens of HID++ writes.
Item {
    id: root

    readonly property int forceRange: Math.max(1, DeviceModel.panelForceMax - DeviceModel.panelForceMin)

    function forcePercent(force) {
        return Math.round((force - DeviceModel.panelForceMin) * 100 / forceRange)
    }

    Flickable {
        anchors.fill: parent
        contentHeight: content.implicitHeight + 64
        clip: true

        Column {
            id: content
            anchors { left: parent.left; top: parent.top; margins: 32 }
            width: Math.min(460, root.width - 64)
            spacing: 28

            Text {
                text: "Haptics"
                font { pixelSize: 22; bold: true }
                color: Theme.text
            }

            Column {
                objectName: "hapticFeedbackSection"
                visible: DeviceModel.hapticsSupported
                width: parent.width
                spacing: 12

                Text {
                    text: "Haptic feedback"
                    font { pixelSize: 15; bold: true }
                    color: Theme.text
                }
                Text {
                    width: parent.width
                    text: "The small buzzes you feel from the mouse, for example when you move across the Actions Ring."
                    wrapMode: Text.WordWrap
                    font.pixelSize: 12
                    color: Theme.textSecondary
                }

                LogituneToggle {
                    id: feedbackToggle
                    objectName: "hapticToggle"
                    label: "Feedback"
                    checked: DeviceModel.hapticsEnabled
                    onToggled: (checked) => DeviceModel.setHaptics(checked, DeviceModel.hapticsLevel)
                }

                LogituneSlider {
                    id: strength
                    objectName: "hapticStrength"
                    width: parent.width
                    label: "Strength  " + Math.round(value) + "%"
                    enabled: DeviceModel.hapticsEnabled
                    opacity: enabled ? 1 : 0.4
                    from: DeviceModel.hapticsFourLevelsOnly ? 25 : 0
                    to: 100
                    stepSize: DeviceModel.hapticsFourLevelsOnly ? 25 : 1
                    value: DeviceModel.hapticsLevel
                    onPressedChanged: {
                        if (!pressed)
                            DeviceModel.setHaptics(true, Math.round(value))
                    }
                }
                Row {
                    width: parent.width
                    Text { text: "Subtle"; width: parent.width / 2; font.pixelSize: 11; color: Theme.textSecondary }
                    Text {
                        text: "Strong"
                        width: parent.width / 2
                        horizontalAlignment: Text.AlignRight
                        font.pixelSize: 11
                        color: Theme.textSecondary
                    }
                }
            }

            Column {
                objectName: "panelForceSection"
                visible: DeviceModel.panelForceSupported
                width: parent.width
                spacing: 12

                Text {
                    text: "Thumb panel press force"
                    font { pixelSize: 15; bold: true }
                    color: Theme.text
                }
                Text {
                    width: parent.width
                    text: "How hard you have to press the thumb panel before it counts as a click."
                    wrapMode: Text.WordWrap
                    font.pixelSize: 12
                    color: Theme.textSecondary
                }

                LogituneSlider {
                    id: force
                    objectName: "panelForce"
                    width: parent.width
                    label: "Press force  " + root.forcePercent(value) + "%"
                    from: DeviceModel.panelForceMin
                    to: DeviceModel.panelForceMax
                    stepSize: Math.max(1, Math.round(root.forceRange / 100))
                    value: DeviceModel.panelForce
                    onPressedChanged: {
                        if (!pressed)
                            DeviceModel.setPanelForce(Math.round(value))
                    }
                }
                Row {
                    width: parent.width
                    Text { text: "Light"; width: parent.width / 2; font.pixelSize: 11; color: Theme.textSecondary }
                    Text {
                        text: "Firm"
                        width: parent.width / 2
                        horizontalAlignment: Text.AlignRight
                        font.pixelSize: 11
                        color: Theme.textSecondary
                    }
                }

                Rectangle {
                    id: resetForce
                    objectName: "panelForceReset"
                    readonly property bool atDefault: DeviceModel.panelForce === DeviceModel.panelForceDefault
                    width: resetText.implicitWidth + 24
                    height: 32
                    radius: 6
                    color: resetHover.hovered && !atDefault ? Theme.hoverBg : "transparent"
                    border.color: Theme.border
                    opacity: atDefault ? 0.4 : 1

                    Text {
                        id: resetText
                        anchors.centerIn: parent
                        text: "Reset to default (" + root.forcePercent(DeviceModel.panelForceDefault) + "%)"
                        font.pixelSize: 12
                        color: Theme.text
                    }
                    HoverHandler { id: resetHover }
                    TapHandler {
                        enabled: !resetForce.atDefault
                        onTapped: DeviceModel.setPanelForce(DeviceModel.panelForceDefault)
                    }
                }
            }

            Text {
                visible: !DeviceModel.hapticsSupported && !DeviceModel.panelForceSupported
                text: "This mouse hasn't reported any haptic settings. It may be asleep: move it and they'll appear."
                width: parent.width
                wrapMode: Text.WordWrap
                font.pixelSize: 13
                color: Theme.textSecondary
            }
        }
    }
}
