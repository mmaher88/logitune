import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Logitune

// Slide-in detail panel from the right edge.
// panelType: "scrollwheel" | "thumbwheel" | "pointerspeed"
Rectangle {
    id: root

    // ── Public API ──────────────────────────────────────────────────────────
    property string panelType: ""

    signal closeRequested

    // ── Geometry / Appearance — percentage-based width ────────────────────
    width: {
        var w = (parent ? parent.width : 960) * 0.33
        return Math.max(360, Math.min(w, 478))
    }
    color:  Theme.surface
    radius: 12
    clip: true

    // Flat right edge (panel sits at window edge)
    Rectangle {
        anchors { right: parent.right; top: parent.top; bottom: parent.bottom }
        width: parent.radius
        color: parent.color
    }

    // Left border
    Rectangle {
        x: 0; y: parent.radius
        width: 1
        height: parent.height - parent.radius * 2
        color: Theme.border
    }

    // ── Slide-in animation ──────────────────────────────────────────────────
    property bool opened: false

    // x is controlled by the parent page — do not set it here
    Behavior on x {
        NumberAnimation { duration: 200; easing.type: Easing.OutQuad }
    }

    // ── Content ─────────────────────────────────────────────────────────────
    ColumnLayout {
        anchors {
            fill: parent
            margins: 32
        }
        spacing: 16

        // Header row: title + X button
        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Text {
                id: panelTitle
                text: {
                    if (root.panelType === "scrollwheel")   return "Scroll wheel"
                    if (root.panelType === "thumbwheel")    return "Thumb wheel"
                    if (root.panelType === "pointerspeed")  return "Pointer speed"
                    return ""
                }
                font.pixelSize: 16
                font.bold: true
                color: Theme.text
                Layout.fillWidth: true
            }

            // Close button
            Rectangle {
                width: 28; height: 28
                radius: 14
                color: closeHover.hovered ? Theme.inputBg : "transparent"
                Behavior on color { ColorAnimation { duration: 100 } }

                Text {
                    anchors.centerIn: parent
                    text: "\u2715"   // ✕
                    font.pixelSize: 13
                    color: "#555555"
                }
                HoverHandler { id: closeHover }
                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.closeRequested()
                }
            }
        }

        // Thin divider
        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: Theme.border
        }

        // ── Per-type content ────────────────────────────────────────────────
        Loader {
            Layout.fillWidth: true
            Layout.fillHeight: true
            sourceComponent: {
                if (root.panelType === "scrollwheel")  return scrollWheelContent
                if (root.panelType === "thumbwheel")   return thumbWheelContent
                if (root.panelType === "pointerspeed") return pointerSpeedContent
                return null
            }
        }
    }

    // ── Scroll Wheel content ────────────────────────────────────────────────
    Component {
        id: scrollWheelContent

        Column {
            width: parent.width
            spacing: 20

            // Guard: don't fire device writes during initial binding
            property bool ready: false
            Component.onCompleted: Qt.callLater(function() { ready = true })

            // Scroll direction radio group
            Column {
                width: parent.width
                spacing: 8

                Text {
                    text: "Scroll direction"
                    font.pixelSize: 16
                    font.bold: true
                    color: Theme.text
                    bottomPadding: 14
                }

                ButtonGroup {
                    id: directionGroup
                }

                RadioButton {
                    id: naturalRadio
                    text: "Natural"
                    checked: DeviceModel.scrollInvert
                    ButtonGroup.group: directionGroup
                    onClicked: DeviceModel.setScrollConfig(DeviceModel.scrollHiRes, true)

                    contentItem: Text {
                        leftPadding: naturalRadio.indicator.width + naturalRadio.spacing
                        text: naturalRadio.text
                        font.pixelSize: 12
                        color: Theme.text
                        verticalAlignment: Text.AlignVCenter
                    }

                    indicator: Rectangle {
                        implicitWidth:  18
                        implicitHeight: 18
                        radius: 9
                        border.color: naturalRadio.checked ? Theme.accent : "#AAAAAA"
                        border.width: 2
                        color: "transparent"
                        anchors.verticalCenter: parent.verticalCenter

                        Rectangle {
                            anchors.centerIn: parent
                            width: 8; height: 8
                            radius: 4
                            color: Theme.accent
                            visible: naturalRadio.checked
                        }
                    }
                }

                RadioButton {
                    id: standardRadio
                    text: "Standard"
                    checked: !DeviceModel.scrollInvert
                    ButtonGroup.group: directionGroup
                    onClicked: DeviceModel.setScrollConfig(DeviceModel.scrollHiRes, false)

                    contentItem: Text {
                        leftPadding: standardRadio.indicator.width + standardRadio.spacing
                        text: standardRadio.text
                        font.pixelSize: 12
                        color: Theme.text
                        verticalAlignment: Text.AlignVCenter
                    }

                    indicator: Rectangle {
                        implicitWidth:  18
                        implicitHeight: 18
                        radius: 9
                        border.color: standardRadio.checked ? Theme.accent : "#AAAAAA"
                        border.width: 2
                        color: "transparent"
                        anchors.verticalCenter: parent.verticalCenter

                        Rectangle {
                            anchors.centerIn: parent
                            width: 8; height: 8
                            radius: 4
                            color: Theme.accent
                            visible: standardRadio.checked
                        }
                    }
                }
            }

            // Smooth scrolling toggle
            Row {
                visible: DeviceModel.smoothScrollSupported
                width: parent.width

                Text {
                    text: "Smooth scrolling"
                    font.pixelSize: 16
                    font.bold: true
                    color: Theme.text
                    anchors.verticalCenter: parent.verticalCenter
                    width: parent.width - smoothToggle.width
                }

                LogituneToggle {
                    id: smoothToggle
                    checked: DeviceModel.scrollHiRes
                    onToggled: DeviceModel.setScrollConfig(checked, DeviceModel.scrollInvert)
                }
            }

            // SmartShift toggle — padding: 12px 32px 24px
            Column {
                width: parent.width
                topPadding: 12
                bottomPadding: 24
                spacing: 12

                Row {
                    width: parent.width

                    Text {
                        text: "SmartShift"
                        font.pixelSize: 16
                        font.bold: true
                        color: Theme.text
                        anchors.verticalCenter: parent.verticalCenter
                        width: parent.width - smartShiftToggle.width
                    }

                    LogituneToggle {
                        id: smartShiftToggle
                        checked: DeviceModel.smartShiftEnabled
                        onToggled: DeviceModel.setSmartShift(checked, smartShiftSlider.value)
                    }
                }

                // SmartShift sensitivity slider — only shown when SmartShift is on
                LogituneSlider {
                    id: smartShiftSlider
                    width: parent.width
                    label: "SmartShift sensitivity"
                    from: 1
                    to: 100
                    value: DeviceModel.smartShiftThreshold
                    visible: smartShiftToggle.checked
                    // Commit on release via the underlying Slider
                    Connections {
                        target: smartShiftSlider
                        function onPressedChanged() {
                            if (!smartShiftSlider.pressed && ready) {
                                DeviceModel.setSmartShift(smartShiftToggle.checked, smartShiftSlider.value)
                            }
                        }
                    }

                    Behavior on opacity { NumberAnimation { duration: 150 } }
                    opacity: smartShiftToggle.checked ? 1.0 : 0.0
                }
            }
        }
    }

    // ── Thumb Wheel content ─────────────────────────────────────────────────
    Component {
        id: thumbWheelContent

        Column {
            width: parent.width
            spacing: 20

            // Direction invert toggle
            Row {
                width: parent.width

                Text {
                    text: "Invert direction"
                    font.pixelSize: 16
                    font.bold: true
                    color: Theme.text
                    anchors.verticalCenter: parent.verticalCenter
                    width: parent.width - invertToggle.width
                }

                LogituneToggle {
                    id: invertToggle
                    checked: DeviceModel.thumbWheelInvert
                    onToggled: DeviceModel.setThumbWheelInvert(checked)
                }
            }
        }
    }

    // ── Pointer Speed / DPI content ─────────────────────────────────────────
    Component {
        id: pointerSpeedContent

        Item {
            width: parent ? parent.width : 340
            implicitHeight: dpiCol.implicitHeight + 64

            Column {
                id: dpiCol
                anchors {
                    left: parent.left; right: parent.right
                    top: parent.top
                    margins: 32
                }
                spacing: 8

                RowLayout {
                    width: parent.width
                    Text {
                        text: "DPI"
                        font.pixelSize: 16
                        font.bold: true
                        color: Theme.text
                    }
                    Item { Layout.fillWidth: true }
                    Text {
                        text: dpiSlider.value.toFixed(0)
                        font.pixelSize: 16
                        font.bold: true
                        color: Theme.accent
                    }
                }

                Slider {
                    id: dpiSlider
                    width: parent.width
                    height: 32
                    from: DeviceModel.minDPI
                    to: DeviceModel.maxDPI
                    stepSize: DeviceModel.dpiStep
                    value: DeviceModel.currentDPI

                    onPressedChanged: {
                        if (!pressed) {
                            // User released the slider — commit the DPI change
                            DeviceModel.setDPI(value)
                        }
                    }

                    background: Rectangle {
                        x: dpiSlider.leftPadding
                        y: dpiSlider.topPadding + dpiSlider.availableHeight / 2 - height / 2
                        width: dpiSlider.availableWidth
                        height: 4
                        radius: 2
                        color: Theme.inputBg

                        Rectangle {
                            width: dpiSlider.visualPosition * parent.width
                            height: parent.height
                            radius: 2
                            color: Theme.accent
                        }
                    }

                    handle: Rectangle {
                        x: dpiSlider.leftPadding + dpiSlider.visualPosition * (dpiSlider.availableWidth - width)
                        y: dpiSlider.topPadding + dpiSlider.availableHeight / 2 - height / 2
                        width: 16; height: 16
                        radius: 8
                        color: Theme.background
                        border.color: Theme.accent
                        border.width: 5
                    }
                }

                // Min/Max labels
                RowLayout {
                    width: parent.width
                    Text {
                        text: DeviceModel.minDPI.toString()
                        font.pixelSize: 11
                        color: "#999999"
                    }
                    Item { Layout.fillWidth: true }
                    Text {
                        text: DeviceModel.maxDPI.toString()
                        font.pixelSize: 11
                        color: "#999999"
                    }
                }
            }
        }
    }
}
