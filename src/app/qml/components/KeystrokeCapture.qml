import QtQuick
import QtQuick.Controls

// Keystroke capture field — click to enter capture mode,
// displays combination as "Ctrl+Shift+A".
Item {
    id: root

    property string keystroke: ""

    signal keystrokeCaptured(string ks)

    implicitWidth:  240
    implicitHeight: 40

    // ── Capture rectangle ──────────────────────────────────────────────────
    Rectangle {
        id: field
        anchors.fill: parent
        radius: 8
        color:   capturing ? "#F0EDFF" : "#FFFFFF"
        border.color: capturing ? "#7B61FF" : "#E0E0E0"
        border.width: capturing ? 2 : 1

        Behavior on border.color { ColorAnimation { duration: 120 } }
        Behavior on color        { ColorAnimation { duration: 120 } }

        property bool capturing: false

        // Placeholder / value text
        Text {
            anchors {
                verticalCenter: parent.verticalCenter
                left:  parent.left
                right: clearBtn.left
                leftMargin:  12
                rightMargin: 8
            }
            text: {
                if (field.capturing) return "Press key combination..."
                if (root.keystroke.length > 0) return root.keystroke
                return "Click to assign"
            }
            color: (field.capturing || root.keystroke.length === 0) ? "#AAAAAA" : "#1A1A1A"
            font.pixelSize: 13
            elide: Text.ElideRight
        }

        // Clear button (X)
        Text {
            id: clearBtn
            anchors {
                verticalCenter: parent.verticalCenter
                right: parent.right
                rightMargin: 10
            }
            text: "\u00D7"
            font.pixelSize: 16
            color: clearHover.hovered ? "#1A1A1A" : "#AAAAAA"
            visible: root.keystroke.length > 0 && !field.capturing

            HoverHandler { id: clearHover }
            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                onClicked: {
                    root.keystroke = ""
                    root.keystrokeCaptured("")
                }
            }
        }

        // Flashing cursor indicator when capturing
        Rectangle {
            anchors {
                verticalCenter: parent.verticalCenter
                right: parent.right
                rightMargin: 10
            }
            width:  8
            height: 16
            radius: 2
            color:  "#7B61FF"
            visible: field.capturing

            SequentialAnimation on opacity {
                running: field.capturing
                loops:   Animation.Infinite
                NumberAnimation { to: 0; duration: 500 }
                NumberAnimation { to: 1; duration: 500 }
            }
        }

        // Click to start capture
        MouseArea {
            anchors.fill: parent
            cursorShape: Qt.IBeamCursor
            onClicked: {
                field.capturing = true
                captureItem.forceActiveFocus()
            }
        }
    }

    // ── Hidden focus item that captures key events ─────────────────────────
    Item {
        id: captureItem
        width: 0; height: 0

        Keys.onPressed: function(event) {
            if (!field.capturing) return

            // Escape cancels capture
            if (event.key === Qt.Key_Escape) {
                field.capturing = false
                event.accepted = true
                return
            }

            // Ignore bare modifier-only presses
            var modifierOnly = [
                Qt.Key_Control, Qt.Key_Shift, Qt.Key_Alt,
                Qt.Key_Meta, Qt.Key_Super_L, Qt.Key_Super_R
            ]
            if (modifierOnly.indexOf(event.key) !== -1) {
                event.accepted = true
                return
            }

            // Build combination string
            var parts = []
            if (event.modifiers & Qt.ControlModifier) parts.push("Ctrl")
            if (event.modifiers & Qt.ShiftModifier)   parts.push("Shift")
            if (event.modifiers & Qt.AltModifier)     parts.push("Alt")
            if (event.modifiers & Qt.MetaModifier)    parts.push("Meta")

            var keyName = Qt.keyName ? Qt.keyName(event.key) : String.fromCharCode(event.key)
            // Prettify common names
            var keyMap = {
                " ": "Space", "\r": "Return", "\n": "Return",
                "\t": "Tab",  "\b": "Backspace"
            }
            keyName = keyMap[keyName] || keyName
            parts.push(keyName.charAt(0).toUpperCase() + keyName.slice(1))

            var combo = parts.join("+")
            root.keystroke = combo
            root.keystrokeCaptured(combo)
            field.capturing = false
            event.accepted = true
        }
    }
}
