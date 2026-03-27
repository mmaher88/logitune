import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Logitune

// Slide-in Actions panel from the right edge.
// Width: 33% of window, clamped 360–478px.
Rectangle {
    id: root

    // ── Public API ─────────────────────────────────────────────────────────
    property int    buttonId:     -1
    property string buttonName:   ""
    property string currentAction: ""
    property string currentActionType: ""
    property bool   isWheel:      false  // true for thumb wheel — shows wheel-specific modes

    signal closed()
    signal actionSelected(string actionName, string actionType)
    signal wheelModeSelected(string mode)  // "scroll", "zoom", "volume"

    // Counter to force gesture action name re-evaluation
    property int _gestureRefresh: 0
    Connections {
        target: DeviceModel
        function onGestureChanged() { root._gestureRefresh++ }
    }

    // Wheel mode options
    readonly property var wheelModes: [
        { name: "Horizontal scroll", mode: "scroll",  desc: "Native horizontal scrolling" },
        { name: "Zoom in/out",       mode: "zoom",    desc: "Ctrl + scroll for zoom" },
        { name: "Volume control",    mode: "volume",  desc: "Adjust system volume" },
        { name: "No action",         mode: "none",    desc: "Disable thumb wheel" },
    ]

    // ── Geometry — percentage-based width ──────────────────────────────────
    width: {
        var w = (parent ? parent.width : 960) * 0.33
        return Math.max(360, Math.min(w, 478))
    }
    color:  Theme.surface
    radius: 0

    // Left border
    Rectangle {
        anchors { left: parent.left; top: parent.top; bottom: parent.bottom }
        width: 1
        color: Theme.border
    }

    // ── Slide animation ────────────────────────────────────────────────────
    // Controlled by parent via x property. No internal animation here;
    // parent drives x so it can coordinate with layout.

    // ── Content ────────────────────────────────────────────────────────────
    ColumnLayout {
        anchors {
            fill: parent
            topMargin:    16
            bottomMargin: 16
            leftMargin:   0
            rightMargin:  0
        }
        spacing: 0

        // Header row
        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin:  20
            Layout.rightMargin: 16

            Column {
                spacing: 2

                Text {
                    text: "Actions"
                    font.pixelSize: 18
                    font.bold: true
                    color: Theme.text
                }

                Text {
                    text: root.buttonName.length > 0 ? root.buttonName : "Button"
                    font.pixelSize: 12
                    color: Theme.textSecondary
                }
            }

            Item { Layout.fillWidth: true }

            // Close button
            Rectangle {
                width: 28; height: 28
                radius: 14
                color: closeHover.hovered ? Theme.inputBg : "transparent"
                Behavior on color { ColorAnimation { duration: 100 } }

                Text {
                    anchors.centerIn: parent
                    text: "\u00D7"
                    font.pixelSize: 18
                    color: "#999999"
                }

                HoverHandler { id: closeHover }
                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.closed()
                }
            }
        }

        // Divider
        Rectangle {
            Layout.fillWidth: true
            Layout.topMargin: 12
            height: 1
            color: Theme.border
        }

        // ── Wheel mode list (shown when isWheel) ─────────────────────────────
        ListView {
            id: wheelList
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.topMargin: 12
            visible: root.isWheel
            clip: true
            model: root.wheelModes
            spacing: 5

            delegate: Item {
                width: wheelList.width
                height: wheelRow.height

                readonly property bool isCurrent: DeviceModel.thumbWheelMode === modelData.mode
                        || (modelData.mode === "scroll" && DeviceModel.thumbWheelMode === undefined)

                Rectangle {
                    id: wheelRow
                    anchors {
                        left: parent.left; right: parent.right
                        leftMargin: wheelList.width * 0.04
                        rightMargin: wheelList.width * 0.04
                    }
                    height: isCurrent ? 48 : 32
                    Behavior on height { NumberAnimation { duration: 200 } }
                    radius: 4
                    color: isCurrent ? Theme.accent : (wheelHover.hovered ? Theme.hoverBg : "transparent")
                    border.color: isCurrent ? "transparent" : (wheelHover.hovered ? "#D4C5FF" : "transparent")
                    border.width: 1

                    Rectangle {
                        anchors { left: parent.left; leftMargin: 15; verticalCenter: parent.verticalCenter }
                        width: 18; height: 18; radius: 9
                        color: isCurrent ? Theme.activeTabText : (wheelHover.hovered ? "#EAE6F5" : Theme.inputBg)
                        border.color: isCurrent ? Theme.accent : "transparent"
                        border.width: isCurrent ? 6 : 0
                    }

                    Text {
                        anchors {
                            left: parent.left; leftMargin: 48
                            right: parent.right; rightMargin: 8
                            verticalCenter: parent.verticalCenter
                        }
                        text: modelData.name
                        font.pixelSize: 14
                        font.bold: isCurrent
                        color: isCurrent ? Theme.activeTabText : (wheelHover.hovered ? Theme.accent : Theme.text)
                        elide: Text.ElideRight
                    }

                    HoverHandler { id: wheelHover }

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            root.wheelModeSelected(modelData.mode)
                        }
                    }
                }
            }
        }

        // ── Search bar (hidden for wheel) ────────────────────────────────────
        Item {
            Layout.fillWidth: true
            Layout.leftMargin: 16
            Layout.rightMargin: 16
            visible: !root.isWheel
            height: 80

            Rectangle {
                anchors {
                    fill: parent
                    topMargin: 16
                    bottomMargin: 16
                }
                radius: 4
                color: Theme.cardBg
                border.color: searchInput.activeFocus ? Theme.accent : Theme.inputBg
                border.width: 1

                Behavior on border.color { ColorAnimation { duration: 200 } }

                Text {
                    anchors {
                        fill: parent
                        leftMargin: 12
                        rightMargin: 12
                    }
                    verticalAlignment: Text.AlignVCenter
                    text: "Search actions..."
                    font.pixelSize: 14
                    color: "#AAAAAA"
                    visible: !searchInput.text && !searchInput.activeFocus
                }

                TextInput {
                    id: searchInput
                    anchors {
                        fill: parent
                        leftMargin: 12
                        rightMargin: 12
                    }
                    verticalAlignment: TextInput.AlignVCenter
                    font.pixelSize: 14
                    color: Theme.text
                    clip: true
                }
            }
        }

        // ── SMART ACTIONS section (hidden for wheel) ─────────────────────────
        Item {
            Layout.fillWidth: true
            visible: !root.isWheel
            Layout.leftMargin:  33
            Layout.rightMargin: 16
            Layout.topMargin:   12
            implicitHeight: 50 + (smartExpanded ? smartBody.implicitHeight + 8 : 0)
            clip: true

            property bool smartExpanded: false

            RowLayout {
                id: smartHeader
                width: parent.width
                height: 50
                spacing: 6

                Text {
                    text: "SMART ACTIONS"
                    font.pixelSize: 14
                    font.letterSpacing: 0.8
                    font.bold: true
                    font.capitalization: Font.AllUppercase
                    color: "#888888"
                    Layout.fillWidth: true
                }

                Text {
                    text: parent.parent.smartExpanded ? "\u25B2" : "\u25BC"
                    font.pixelSize: 9
                    color: "#AAAAAA"
                }
            }

            Column {
                id: smartBody
                anchors { top: smartHeader.bottom; topMargin: 8; left: parent.left; right: parent.right }
                visible: parent.smartExpanded

                Text {
                    width: parent.width
                    text: "Create Smart Actions to assign\ncontext-aware behaviors to buttons."
                    font.pixelSize: 12
                    color: "#AAAAAA"
                    wrapMode: Text.Wrap
                    lineHeight: 1.5
                }
            }

            MouseArea {
                anchors { left: parent.left; right: parent.right; top: parent.top }
                height: smartHeader.height + 4
                cursorShape: Qt.PointingHandCursor
                onClicked: parent.smartExpanded = !parent.smartExpanded
            }
        }

        // Divider
        Rectangle {
            Layout.fillWidth: true
            Layout.topMargin: 8
            height: 1
            color: Theme.border
            visible: !root.isWheel
        }

        // ── OTHER ACTIONS section header (hidden for wheel) ──────────────
        Item {
            visible: !root.isWheel
            Layout.fillWidth: true
            Layout.leftMargin:  33
            Layout.rightMargin: 16
            Layout.topMargin:   10
            implicitHeight: 50

            RowLayout {
                width: parent.width
                height: 50

                Text {
                    text: "OTHER ACTIONS"
                    font.pixelSize: 14
                    font.letterSpacing: 0.8
                    font.bold: true
                    font.capitalization: Font.AllUppercase
                    color: "#888888"
                    Layout.fillWidth: true
                }
            }
        }

        // ── Action list (hidden for wheel) ────────────────────────────────
        ListView {
            visible: !root.isWheel
            id: actionList
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.topMargin: 4
            clip: true
            model: ActionModel
            spacing: 5

            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

            delegate: Item {
                // Row height animates between 32px (unselected) and 48px (selected)
                width: actionList.width
                height: rowRect.height  // tracks the animated rectangle height

                required property string name
                required property string description
                required property string actionType
                required property int    index

                readonly property bool isSelected: name === root.currentAction

                Rectangle {
                    id: rowRect
                    anchors {
                        left:  parent.left
                        right: parent.right
                        // 4% margin on each side  (width * 0.04)
                        leftMargin:  actionList.width * 0.04
                        rightMargin: actionList.width * 0.04
                        top: parent.top
                    }

                    // Animate height between 32 (normal) and 48 (selected)
                    height: isSelected ? 48 : 32
                    Behavior on height { NumberAnimation { duration: 200; easing.type: Easing.InOutQuad } }

                    radius: 4
                    color: isSelected
                           ? Theme.accent
                           : (rowHover.hovered ? Theme.hoverBg : "transparent")
                    border.color: isSelected
                                  ? "transparent"
                                  : (rowHover.hovered ? Theme.accentHover : "transparent")
                    border.width: 1

                    Behavior on color { ColorAnimation { duration: 200 } }

                    // Radio circle — 18×18px
                    Rectangle {
                        id: radioCircle
                        anchors {
                            left:           parent.left
                            leftMargin:     15
                            verticalCenter: parent.verticalCenter
                        }
                        width:  18
                        height: 18
                        radius: 9

                        // Unselected: flat grey fill, no border
                        // Selected:   white fill, 6px purple border
                        color:        isSelected ? Theme.activeTabText
                                                 : (rowHover.hovered ? "#EAE6F5" : Theme.inputBg)
                        border.color: isSelected ? Theme.accent : "transparent"
                        border.width: isSelected ? 6 : 0

                        Behavior on color        { ColorAnimation { duration: 200 } }
                        Behavior on border.width { NumberAnimation  { duration: 200 } }
                    }

                    // Label — 14px, normal/bold depending on state
                    Text {
                        anchors {
                            left:           radioCircle.right
                            leftMargin:     15
                            right:          parent.right
                            rightMargin:    8
                            verticalCenter: parent.verticalCenter
                        }
                        text:           name
                        font.pixelSize: 14
                        font.bold:      isSelected
                        color:          isSelected  ? Theme.activeTabText
                                        : (rowHover.hovered ? Theme.accent : Theme.text)
                        elide:          Text.ElideRight
                        maximumLineCount: 1

                        Behavior on color { ColorAnimation { duration: 200 } }
                    }

                    HoverHandler { id: rowHover }

                    MouseArea {
                        anchors.fill: parent
                        cursorShape:  Qt.PointingHandCursor
                        onClicked: {
                            root.currentAction     = name
                            root.currentActionType = actionType
                            root.actionSelected(name, actionType)
                        }
                    }
                }
            }
        }

        // Divider above contextual area
        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: Theme.border
            visible: root.currentActionType.length > 0
        }

        // ── Contextual controls (Loader) ───────────────────────────────────
        Loader {
            id: contextLoader
            Layout.fillWidth: true
            Layout.topMargin: 0
            Layout.bottomMargin: 4
            visible: item !== null

            sourceComponent: {
                switch (root.currentActionType) {
                case "keystroke":      return keystrokeComponent
                case "gesture-trigger": return gestureComponent
                default:               return descriptionComponent
                }
            }
        }
    }

    // ── Contextual component definitions ───────────────────────────────────

    Component {
        id: keystrokeComponent

        Item {
            implicitHeight: col.implicitHeight + 24
            width: parent ? parent.width : 340

            Column {
                id: col
                anchors {
                    left: parent.left; right: parent.right
                    top: parent.top
                    leftMargin: 20; rightMargin: 20; topMargin: 12
                }
                spacing: 8

                Text {
                    text: "Key combination"
                    font.pixelSize: 12
                    font.bold: true
                    color: "#444444"
                }

                KeystrokeCapture {
                    width: parent.width
                }
            }
        }
    }

    Component {
        id: gestureComponent

        Item {
            implicitHeight: gestureCol.implicitHeight + 24
            width: parent ? parent.width : 340

            Column {
                id: gestureCol
                anchors {
                    left: parent.left; right: parent.right; top: parent.top
                    leftMargin: 20; rightMargin: 20; topMargin: 12
                }
                spacing: 4

                Text {
                    text: "Gesture directions"
                    font.pixelSize: 12
                    font.bold: true
                    color: "#444444"
                    bottomPadding: 4
                }

                Repeater {
                    model: [
                        { dir: "\u2191", label: "Up",    key: "up" },
                        { dir: "\u2193", label: "Down",  key: "down" },
                        { dir: "\u2190", label: "Left",  key: "left" },
                        { dir: "\u2192", label: "Right", key: "right" },
                        { dir: "\u25C9", label: "Click", key: "click" },
                    ]

                    delegate: Rectangle {
                        width: parent.width
                        height: 36
                        radius: 4
                        color: gestureRowHover.hovered ? Theme.hoverBg : Theme.cardBg
                        border.color: gestureRowHover.hovered ? Theme.accentHover : Theme.border
                        border.width: 1

                        readonly property string actionName: root._gestureRefresh >= 0 ? DeviceModel.gestureActionName(modelData.key) : ""

                        RowLayout {
                            anchors { fill: parent; leftMargin: 10; rightMargin: 10 }
                            spacing: 8

                            Text {
                                text: modelData.dir
                                font.pixelSize: 14
                                color: Theme.accent
                            }
                            Text {
                                text: modelData.label
                                font.pixelSize: 12
                                color: Theme.text
                                Layout.fillWidth: true
                            }
                            Text {
                                text: actionName || "None"
                                font.pixelSize: 11
                                color: actionName ? Theme.accent : "#AAAAAA"
                            }
                        }

                        HoverHandler { id: gestureRowHover }

                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                gestureDirectionPicker.direction = modelData.key
                                gestureDirectionPicker.dirLabel = modelData.label
                                gestureDirectionPicker.visible = true
                            }
                        }
                    }
                }

                // ── Gesture direction action picker (inline) ─────────
                Column {
                    id: gestureDirectionPicker
                    visible: false
                    width: parent.width
                    spacing: 2
                    topPadding: 8

                    property string direction: ""
                    property string dirLabel: ""

                    Text {
                        text: "Assign action to " + gestureDirectionPicker.dirLabel + ":"
                        font.pixelSize: 11
                        font.bold: true
                        color: "#444444"
                        bottomPadding: 4
                    }

                    Repeater {
                        model: [
                            { name: "None",                 keystroke: "" },
                            { name: "Show desktop",         keystroke: "Super+D" },
                            { name: "Task switcher",        keystroke: "Super+W" },
                            { name: "Switch desktop left",  keystroke: "Ctrl+Super+Left" },
                            { name: "Switch desktop right", keystroke: "Ctrl+Super+Right" },
                            { name: "Copy",                 keystroke: "Ctrl+C" },
                            { name: "Paste",                keystroke: "Ctrl+V" },
                            { name: "Undo",                 keystroke: "Ctrl+Z" },
                            { name: "Redo",                 keystroke: "Ctrl+Shift+Z" },
                            { name: "Close window",         keystroke: "Alt+F4" },
                            { name: "Mute",                 keystroke: "Mute" },
                            { name: "Play/Pause",           keystroke: "Play" },
                        ]

                        Rectangle {
                            width: parent.width
                            height: 28
                            radius: 4
                            color: gpHover.hovered ? Theme.hoverBg : "transparent"

                            Text {
                                anchors { left: parent.left; leftMargin: 8; verticalCenter: parent.verticalCenter }
                                text: modelData.name
                                font.pixelSize: 11
                                color: gpHover.hovered ? Theme.accent : Theme.text
                            }

                            HoverHandler { id: gpHover }

                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    DeviceModel.setGestureAction(
                                        gestureDirectionPicker.direction,
                                        modelData.name === "None" ? "" : modelData.name,
                                        modelData.keystroke)
                                    gestureDirectionPicker.visible = false
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    Component {
        id: descriptionComponent

        Item {
            implicitHeight: descText.implicitHeight + 24
            width: parent ? parent.width : 340

            Text {
                id: descText
                anchors {
                    left: parent.left; right: parent.right; top: parent.top
                    leftMargin: 20; rightMargin: 20; topMargin: 12
                }
                text: {
                    switch (root.currentActionType) {
                    case "none":         return "This button will do nothing when pressed."
                    case "screenshot":   return "Captures the full screen and saves to your pictures folder."
                    case "show-desktop": return "Minimises all windows to reveal the desktop."
                    case "app-expose":   return "Shows all open windows for the current application."
                    case "calculator":   return "Opens the system calculator application."
                    case "mute":         return "Toggles your system audio mute."
                    case "media":        return "Controls media playback (play, pause, skip)."
                    case "volume-up":    return "Increases system volume by one step."
                    case "volume-down":  return "Decreases system volume by one step."
                    default:             return root.currentAction.length > 0
                                                ? root.currentAction + " action assigned."
                                                : ""
                    }
                }
                font.pixelSize: 12
                color: "#888888"
                wrapMode: Text.Wrap
                lineHeight: 1.5
            }
        }
    }
}
