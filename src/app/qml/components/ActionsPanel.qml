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

    // True when the current action is a custom keystroke (not a predefined action)
    readonly property bool isCustomKeystroke: currentActionType === "keystroke"
        && currentAction !== "" && currentAction !== "Keyboard shortcut"
        && !_predefinedNames.has(currentAction)
    property var _predefinedNames: new Set()

    // True when the contextual sub-form below the action list is non-empty
    // (gesture direction config, keystroke capture, or custom keystroke
    // re-bind). Drives the Loader's fillHeight and the action list's height
    // cap so the sub-form has room without the picker pushing it offscreen.
    readonly property bool _hasContextual: currentActionType === "gesture-trigger"
        || currentAction === "Keyboard shortcut"
        || isCustomKeystroke

    Component.onCompleted: {
        var s = new Set()
        for (var i = 0; i < ActionModel.rowCount(); i++) {
            var idx = ActionModel.index(i, 0)
            s.add(ActionModel.data(idx, 0x101)) // NameRole = Qt::UserRole+1 = 257
        }
        _predefinedNames = s
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

        // ── Action list (hidden for wheel) ────────────────────────────────
        // When a contextual sub-form is present, the picker prefers ~240px
        // (scrollable internally) and shrinks below that on short windows
        // so the sub-form keeps a minimum visible footprint. Otherwise the
        // picker fills the whole panel.
        ListView {
            visible: !root.isWheel
            id: actionList
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.preferredHeight: root._hasContextual ? 240 : -1
            Layout.maximumHeight: root._hasContextual ? 240 : Number.POSITIVE_INFINITY
            Layout.topMargin: 10
            clip: true
            model: ActionModel
            spacing: 5

            section.property: "category"
            section.criteria: ViewSection.FullString
            section.delegate: Rectangle {
                width: actionList.width
                height: 24
                color: "transparent"
                Text {
                    anchors {
                        left: parent.left; leftMargin: 33
                        verticalCenter: parent.verticalCenter
                    }
                    text: section.toUpperCase()
                    font.pixelSize: 11
                    font.letterSpacing: 0.8
                    font.bold: true
                    font.capitalization: Font.AllUppercase
                    color: "#888888"
                }
                Rectangle {
                    anchors {
                        left: parent.left; leftMargin: 33
                        right: parent.right; rightMargin: 16
                        bottom: parent.bottom
                    }
                    height: 1
                    color: Theme.border
                    opacity: 0.5
                }
            }

            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

            delegate: Item {
                // Row height animates between 32px (unselected) and 48px (selected)
                width: actionList.width

                required property string name
                required property string description
                required property string actionType
                required property string category
                required property int    index

                readonly property bool matchesSearch: {
                    if (searchInput.text.length === 0) return true
                    var q = searchInput.text.toLowerCase()
                    return name.toLowerCase().indexOf(q) !== -1
                        || category.toLowerCase().indexOf(q) !== -1
                }

                visible: matchesSearch
                height: matchesSearch ? rowRect.height : 0

                // Select matching action, or highlight "Keyboard shortcut" for custom keystrokes
                readonly property bool isSelected: name === root.currentAction
                    || (name === "Keyboard shortcut" && root.isCustomKeystroke)

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
            Layout.fillHeight: root._hasContextual
            Layout.minimumHeight: root._hasContextual ? 200 : 0
            Layout.topMargin: 0
            Layout.bottomMargin: 4
            visible: item !== null

            sourceComponent: {
                if (root.currentActionType === "gesture-trigger")
                    return gestureComponent
                if (root.currentAction === "Keyboard shortcut" || root.isCustomKeystroke)
                    return keystrokeComponent
                return descriptionComponent
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
                    keystroke: root.isCustomKeystroke ? root.currentAction : ""
                    onKeystrokeCaptured: function(ks) {
                        if (ks.length > 0 && root.buttonId >= 0) {
                            root.currentAction = ks
                            root.actionSelected(ks, "keystroke")
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
                    if (root.currentActionType === "none")
                        return "This button will do nothing when pressed."
                    if (root.currentActionType === "smartshift-toggle")
                        return "Toggles scroll wheel between ratchet and free-spin."
                    return root.currentAction.length > 0
                           ? root.currentAction + " action assigned."
                           : ""
                }
                font.pixelSize: 12
                color: "#888888"
                wrapMode: Text.Wrap
                lineHeight: 1.5
            }
        }
    }

    // ── Gesture-trigger sub-form ───────────────────────────────────────────
    // Loaded when currentActionType === "gesture-trigger". Renders the
    // Presets quick-set, the 5 direction rows (up/down/left/right/click),
    // and an inline action picker (gesturePickerComponent) that expands
    // beneath whichever direction row was tapped.
    //
    // Bound to GestureActionModel (the gesture-mode ActionFilterModel
    // wrapper) so the rows shown in the picker are the actions supported
    // on the active DE, minus the gesture-incompatible ones.
    Component {
        id: gestureComponent

        Item {
            id: gestureRoot
            implicitHeight: gestureCol.implicitHeight + 24
            width: parent ? parent.width : 340

            // Counter to force gesture action name re-evaluation when the
            // backend reports a change.
            property int _gestureRefresh: 0
            Connections {
                target: DeviceModel
                function onGestureChanged() { gestureRoot._gestureRefresh++ }
            }

            // Set of all action names listed directly in GestureActionModel.
            // Used to detect custom captured keystrokes (any binding whose
            // name isn't a known action) so we can highlight the
            // "Keyboard shortcut" entry for them in the picker.
            property var _predefinedNames: new Set()
            Component.onCompleted: {
                var s = new Set()
                for (var i = 0; i < GestureActionModel.rowCount(); i++) {
                    var idx = GestureActionModel.index(i, 0)
                    s.add(GestureActionModel.data(idx, 0x101)) // NameRole = Qt::UserRole+1 = 257
                }
                _predefinedNames = s
            }

            // Inline action picker shown beneath whichever direction
            // row is currently expanded. Bound to the registered
            // GestureActionModel (ActionFilterModel wrapper). Includes
            // a "None" sentinel at the top to clear the binding.
            Component {
                id: gesturePickerComponent

                Column {
                    id: pickerRoot
                    width: parent.width
                    spacing: 4
                    topPadding: 8
                    bottomPadding: 8

                    // Sub-flow state: "" = main list, "keystroke" =
                    // KeystrokeCapture.
                    property string subFlow: ""

                    // ── Main list (default sub-flow) ─────────────────
                    Column {
                        width: parent.width
                        spacing: 4
                        visible: pickerRoot.subFlow === ""

                        Text {
                            text: "Assign action to " + gestureCol.expandedLabel + ":"
                            font.pixelSize: 11
                            font.bold: true
                            color: "#444444"
                            bottomPadding: 4
                        }

                        // "None" sentinel row, always at the top.
                        Rectangle {
                            width: parent.width
                            height: 28
                            radius: 4

                            readonly property bool isCurrent:
                                DeviceModel.gestureActionName(gestureCol.expandedDirection).length === 0

                            color: isCurrent
                                ? Theme.accent
                                : (noneHover.hovered ? Theme.hoverBg : "transparent")
                            Text {
                                anchors { left: parent.left; leftMargin: 8; verticalCenter: parent.verticalCenter }
                                text: "None"
                                font.pixelSize: 11
                                font.bold: parent.isCurrent
                                color: parent.isCurrent
                                    ? Theme.activeTabText
                                    : (noneHover.hovered ? Theme.accent : Theme.text)
                            }
                            HoverHandler { id: noneHover }
                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    DeviceModel.setGestureAction(
                                        gestureCol.expandedDirection, "", "none", "")
                                    gestureCol.expandedDirection = ""
                                }
                            }
                        }

                        // Scrollable list of all DE-supported actions
                        // (filtered by ActionFilterModel).
                        ScrollView {
                            width: parent.width
                            height: Math.min(actionsList.contentHeight, 280)
                            clip: true
                            ScrollBar.vertical.policy: ScrollBar.AsNeeded

                            ListView {
                                id: actionsList
                                model: GestureActionModel
                                spacing: 2
                                interactive: true

                                section.property: "category"
                                section.criteria: ViewSection.FullString
                                section.delegate: Rectangle {
                                    width: actionsList.width
                                    height: 20
                                    color: "transparent"
                                    Text {
                                        anchors {
                                            left: parent.left; leftMargin: 8
                                            verticalCenter: parent.verticalCenter
                                        }
                                        text: section.toUpperCase()
                                        font.pixelSize: 9
                                        font.letterSpacing: 0.8
                                        font.bold: true
                                        font.capitalization: Font.AllUppercase
                                        color: "#888888"
                                    }
                                }

                                delegate: Rectangle {
                                    required property string name
                                    required property string actionType
                                    required property string payload
                                    width: actionsList.width
                                    height: 28
                                    radius: 4

                                    readonly property bool isCurrent: {
                                        if (gestureRoot._gestureRefresh < 0) return false
                                        var bound = DeviceModel.gestureActionName(gestureCol.expandedDirection)
                                        if (bound.length === 0) return false
                                        if (name === bound) return true
                                        // Highlight "Keyboard shortcut" when the bound name is a
                                        // captured combo: not in the predefined action list.
                                        if (name === "Keyboard shortcut"
                                            && !gestureRoot._predefinedNames.has(bound))
                                            return true
                                        return false
                                    }

                                    color: isCurrent
                                        ? Theme.accent
                                        : (itemHover.hovered ? Theme.hoverBg : "transparent")

                                    Text {
                                        anchors { left: parent.left; leftMargin: 8; verticalCenter: parent.verticalCenter }
                                        text: name
                                        font.pixelSize: 11
                                        font.bold: parent.isCurrent
                                        color: parent.isCurrent
                                            ? Theme.activeTabText
                                            : (itemHover.hovered ? Theme.accent : Theme.text)
                                    }
                                    HoverHandler { id: itemHover }
                                    MouseArea {
                                        anchors.fill: parent
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: {
                                            if (name === "Keyboard shortcut") {
                                                pickerRoot.subFlow = "keystroke"
                                            } else {
                                                DeviceModel.setGestureAction(
                                                    gestureCol.expandedDirection,
                                                    name, actionType, payload)
                                                gestureCol.expandedDirection = ""
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }

                    // ── Keystroke sub-flow ───────────────────────────
                    Column {
                        visible: pickerRoot.subFlow === "keystroke"
                        width: parent.width
                        spacing: 4

                        Text {
                            text: "Capture keystroke for " + gestureCol.expandedLabel + ":"
                            font.pixelSize: 11
                            font.bold: true
                            color: "#444444"
                            bottomPadding: 4
                        }

                        KeystrokeCapture {
                            width: parent.width
                            keystroke: ""
                            onKeystrokeCaptured: function(ks) {
                                if (ks.length > 0) {
                                    DeviceModel.setGestureAction(
                                        gestureCol.expandedDirection,
                                        ks, "keystroke", ks)
                                    gestureCol.expandedDirection = ""
                                }
                            }
                        }

                        Rectangle {
                            width: parent.width
                            height: 24
                            radius: 4
                            color: kbBackHover.hovered ? Theme.hoverBg : "transparent"
                            Text {
                                anchors { left: parent.left; leftMargin: 8; verticalCenter: parent.verticalCenter }
                                text: "← Back"
                                font.pixelSize: 11
                                color: Theme.accent
                            }
                            HoverHandler { id: kbBackHover }
                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: pickerRoot.subFlow = ""
                            }
                        }
                    }
                }
            }

            // Gesture body (presets + directions). Wrapped in a Flickable
            // so the picker has somewhere to go on short windows.
            Flickable {
                anchors.fill: parent
                anchors.topMargin: 12
                contentWidth: width
                contentHeight: gestureCol.implicitHeight + 24
                clip: true
                ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

                Column {
                    id: gestureCol
                    anchors {
                        left: parent.left; right: parent.right; top: parent.top
                        leftMargin: 20; rightMargin: 20; topMargin: 0
                    }
                    spacing: 4

                    // State for which direction row currently has the picker open.
                    property string expandedDirection: ""
                    property string expandedLabel: ""

                    Text {
                        text: "Presets"
                        font.pixelSize: 12
                        font.bold: true
                        color: "#444444"
                        bottomPadding: 4
                    }

                    Row {
                        width: parent.width
                        spacing: 6

                        Repeater {
                            // Each direction is { name, type, payload }. type is one of
                            // "preset" | "keystroke" | "none". Preset ids resolve via
                            // IDesktopIntegration so they fire correctly on every DE
                            // (KDE, GNOME, sway, ...) without a hardcoded keystroke.
                            model: [
                                {
                                    label: "Navigation",
                                    up:    { name: "",                     type: "none",      payload: "" },
                                    down:  { name: "Show desktop",         type: "preset",    payload: "show-desktop" },
                                    left:  { name: "Switch desktop left",  type: "preset",    payload: "switch-desktop-left" },
                                    right: { name: "Switch desktop right", type: "preset",    payload: "switch-desktop-right" },
                                    click: { name: "Task switcher",        type: "preset",    payload: "task-switcher" }
                                },
                                {
                                    label: "Media",
                                    up:    { name: "",            type: "none",      payload: "" },
                                    down:  { name: "Mute",        type: "keystroke", payload: "Mute" },
                                    left:  { name: "Play/Pause",  type: "keystroke", payload: "Play" },
                                    right: { name: "Play/Pause",  type: "keystroke", payload: "Play" },
                                    click: { name: "",            type: "none",      payload: "" }
                                },
                                {
                                    label: "Window",
                                    up:    { name: "",             type: "none",   payload: "" },
                                    down:  { name: "Show desktop", type: "preset", payload: "show-desktop" },
                                    left:  { name: "",             type: "none",   payload: "" },
                                    right: { name: "",             type: "none",   payload: "" },
                                    click: { name: "Close window", type: "preset", payload: "close-window" }
                                }
                            ]

                            Rectangle {
                                width: (parent.width - 12) / 3
                                height: 30
                                radius: 4
                                color: presetHover.hovered ? Theme.hoverBg : Theme.cardBg
                                border.color: presetHover.hovered ? Theme.accentHover : Theme.border
                                border.width: 1

                                Text {
                                    anchors.centerIn: parent
                                    text: modelData.label
                                    font.pixelSize: 11
                                    font.bold: true
                                    color: presetHover.hovered ? Theme.accent : Theme.text
                                }

                                HoverHandler { id: presetHover }

                                MouseArea {
                                    anchors.fill: parent
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: {
                                        var dirs = ["up", "down", "left", "right", "click"]
                                        for (var i = 0; i < dirs.length; i++) {
                                            var d = dirs[i]
                                            var preset = modelData[d]
                                            DeviceModel.setGestureAction(d, preset.name, preset.type, preset.payload)
                                        }
                                    }
                                }
                            }
                        }
                    }

                    // Spacer between presets and directions
                    Item { width: 1; height: 8 }

                    Text {
                        text: "Gesture directions"
                        font.pixelSize: 12
                        font.bold: true
                        color: "#444444"
                        bottomPadding: 4
                    }

                    Repeater {
                        model: [
                            { dir: "↑", label: "Up",    key: "up" },
                            { dir: "↓", label: "Down",  key: "down" },
                            { dir: "←", label: "Left",  key: "left" },
                            { dir: "→", label: "Right", key: "right" },
                            { dir: "◉", label: "Click", key: "click" },
                        ]

                        delegate: Column {
                            width: parent.width
                            spacing: 0

                            Rectangle {
                                id: gestureRowRect
                                width: parent.width
                                height: 36
                                radius: 4
                                color: gestureRowHover.hovered ? Theme.hoverBg : Theme.cardBg
                                border.color: gestureRowHover.hovered ? Theme.accentHover : Theme.border
                                border.width: 1

                                readonly property string actionName: gestureRoot._gestureRefresh >= 0 ? DeviceModel.gestureActionName(modelData.key) : ""

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
                                        text: gestureRowRect.actionName || "None"
                                        font.pixelSize: 11
                                        color: gestureRowRect.actionName ? Theme.accent : "#AAAAAA"
                                        Layout.preferredWidth: implicitWidth
                                        Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
                                    }
                                }

                                HoverHandler { id: gestureRowHover }

                                MouseArea {
                                    anchors.fill: parent
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: {
                                        if (gestureCol.expandedDirection === modelData.key) {
                                            gestureCol.expandedDirection = ""
                                        } else {
                                            gestureCol.expandedDirection = modelData.key
                                            gestureCol.expandedLabel = modelData.label
                                        }
                                    }
                                }
                            }

                            Loader {
                                width: parent.width
                                active: gestureCol.expandedDirection === modelData.key
                                sourceComponent: gesturePickerComponent
                                visible: active
                                height: active ? implicitHeight : 0
                            }
                        }
                    }
                }
            }
        }
    }
}
