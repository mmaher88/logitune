import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Logitune

ScrollView {
    id: root
    clip: true
    contentWidth: availableWidth
    property bool navigationEnabled: false
    signal backRequested()
    signal buttonsRequested()
    property bool openFolderAfterSave: false
    property int editingIndex: -1
    property int editingFolder: -1
    property int folderIndex: -1
    property int selectedSlot: 0
    property string error: ""
    property var applications: []
    property var selectedApplication: null
    property bool launcherAvailable: true
    readonly property var filteredApplications: applications.filter(function(app) {
        return (app.title + " " + app.desktopId).toLowerCase().indexOf(applicationSearch.text.toLowerCase()) >= 0
    })
    property var copiedItems: null
    property var undoItems: null
    property var redoItems: null
    property var pendingItems: null
    property string pendingMessage: ""
    readonly property var iconNames: ["", "folder", "keyboard", "launch", "copy", "paste", "cut", "undo", "redo", "play", "next", "previous", "volume", "mute", "brightness", "desktop", "windows", "screenshot", "calculator", "dpi", "wheel", "action", "browser", "terminal", "brush", "edit", "crop", "eyedropper", "star", "heart", "mail", "chat", "camera", "video", "microphone", "file", "save", "link"]
    readonly property var currentFolder: folderIndex >= 0 ? DeviceModel.radialItems[folderIndex] : null
    readonly property var currentItems: currentFolder && currentFolder.kind === "folder" ? padded(currentFolder.children || [], 9) : DeviceModel.radialItems
    readonly property var selectedEntry: currentItems[selectedSlot] || emptyEntry()
    readonly property var specialEntries: [
        {kind: "folder", label: qsTr("New folder"), children: [], icon: "folder", createFolder: true},
        {kind: "action", label: qsTr("Keyboard shortcut"), action: "keystroke:"},
        {kind: "action", label: qsTr("Launch command"), action: "app-launch:"},
        {kind: "adjustment", label: qsTr("Volume"), parameter: "volume"},
        {kind: "adjustment", label: qsTr("Brightness"), parameter: "brightness"},
        {kind: "action", label: qsTr("Launch application"), action: "application-picker:", icon: "grid"}
    ]
    function openApplicationPicker() {
        launcherAvailable = DeviceModel.applicationLauncherAvailable()
        applications = DeviceModel.installedApplications()
        applicationSearch.text = ""
        applicationPicker.open()
        applicationSearch.forceActiveFocus()
    }
    function chooseApplication(app) {
        selectedApplication = clone(app)
        labelField.text = app.title.slice(0, 80)
        iconPicker.currentIndex = 0
        actionKind.currentIndex = 7
        error = ""
        applicationPicker.close()
    }
    function clone(value) { return JSON.parse(JSON.stringify(value)) }
    function emptyEntry() { return {kind: "empty", label: "", action: "default"} }
    function padded(items, count) {
        var result = clone(items)
        while (result.length < count) result.push(emptyEntry())
        return result
    }
    function persist(items, click) {
        var before = clone(DeviceModel.radialItems)
        error = DeviceModel.saveRadialMenu(items, click) ? "" : qsTr("Enter a name and a valid action.")
        if (!error.length) { undoItems = before; redoItems = null }
        return error.length === 0
    }
    function resetView() {
        applicationPicker.close(); editDialog.close(); confirmDialog.close()
        slotMenu.close(); ringMenu.close(); settingsPopup.close(); helpPopup.close(); presetsPopup.close()
        actionCatalog.reset()
        folderIndex = -1; selectedSlot = 0; undoItems = null; redoItems = null; pendingItems = null; error = ""
    }
    function enterFolder(index) {
        if (folderIndex >= 0 || !DeviceModel.radialItems[index] || DeviceModel.radialItems[index].kind !== "folder") return
        folderIndex = index
        var empty = currentItems.findIndex(function(entry) { return entry.kind === "empty" })
        selectedSlot = empty >= 0 ? empty : 0
        error = ""
    }
    function leaveFolder() {
        var parentSlot = folderIndex
        folderIndex = -1
        selectedSlot = Math.max(0, parentSlot)
        error = ""
    }
    function restoreSnapshot(redo) {
        var snapshot = redo ? redoItems : undoItems
        if (!snapshot) return
        var before = clone(DeviceModel.radialItems)
        if (DeviceModel.saveRadialMenu(snapshot, DeviceModel.radialClick)) {
            undoItems = redo ? before : null
            redoItems = redo ? null : before
            folderIndex = -1; selectedSlot = 0; error = ""
        }
    }
    function writeEntry(index, entry, folder) {
        var items = clone(DeviceModel.radialItems)
        if (folder >= 0) {
            var children = padded(items[folder].children || [], 9)
            children[index] = clone(entry); items[folder].children = children
        } else {
            var oldEntry = items[index]
            items[index] = clone(entry)
            if (oldEntry.kind === "folder" && entry.kind !== "folder") {
                confirmReplacement(items, qsTr("Replace this folder and remove its actions?"))
                return false
            }
        }
        return persist(items, DeviceModel.radialClick)
    }
    function editEntry(index, parentFolder) {
        if (index < 0) return
        openFolderAfterSave = false
        editingFolder = parentFolder ? -1 : folderIndex
        var entries = editingFolder >= 0 ? currentItems : DeviceModel.radialItems
        if (index >= entries.length) return
        editingIndex = index
        var entry = entries[index]
        selectedApplication = entry.desktopFile ? {title: entry.label, icon: entry.appIcon || "", desktopFile: entry.desktopFile, command: entry.action.substring(11)} : null
        labelField.text = entry.label || ""
        var action = entry.action || "default"
        var found = catalog.indexOfValue(action)
        catalog.currentIndex = Math.max(0, found)
        actionKind.currentIndex = entry.kind === "folder" ? 4 : entry.kind === "adjustment" ? (entry.parameter === "volume" ? 5 : 6)
            : selectedApplication ? 7 : action === "default" ? 3 : found >= 0 ? 0 : action.indexOf("app-launch:") === 0 ? 2 : 1
        payloadField.text = action.substring(action.indexOf(":") + 1)
        iconPicker.currentIndex = Math.max(0, iconNames.indexOf(entry.icon || ""))
        error = ""
        editDialog.open()
    }
    function assignEntry(index, entry, sourceIndex) {
        if (folderIndex >= 0 && entry.kind === "folder") {
            error = qsTr("Folders can only be placed in the main ring."); return
        }
        selectedSlot = index
        if (sourceIndex >= 0) {
            if (sourceIndex === index) return
            var items = clone(DeviceModel.radialItems)
            var entries = folderIndex >= 0 ? padded(items[folderIndex].children || [], 9) : items
            var previous = entries[index]; entries[index] = entries[sourceIndex]; entries[sourceIndex] = previous
            if (folderIndex >= 0) items[folderIndex].children = entries
            persist(items, DeviceModel.radialClick)
        } else if (entry.createFolder) {
            editEntry(index)
            actionKind.currentIndex = 4
            labelField.text = qsTr("New folder")
            iconPicker.currentIndex = iconNames.indexOf("folder")
            openFolderAfterSave = true
            labelField.forceActiveFocus(); labelField.selectAll()
        } else if (entry.action === "application-picker:") {
            editEntry(index)
            selectedApplication = null
            actionKind.currentIndex = 7
            labelField.text = ""
            openApplicationPicker()
        } else if (entry.action === "keystroke:" || entry.action === "app-launch:") {
            editEntry(index)
            labelField.text = ""
            actionKind.currentIndex = entry.action === "keystroke:" ? 1 : 2
            payloadField.text = ""
        } else writeEntry(index, entry, folderIndex)
    }
    function dropEntry(index, entry, sourceIndex) {
        var target = currentItems[index]
        if (folderIndex >= 0 || target.kind !== "folder" || entry.kind === "folder") {
            assignEntry(index, entry, sourceIndex)
            return
        }
        if (entry.kind === "empty") return
        var children = padded(target.children || [], 9)
        var freeSlot = children.findIndex(function(child) { return child.kind === "empty" })
        if (freeSlot < 0) { error = qsTr("This folder is full. Remove an action to add another."); return }
        if (sourceIndex >= 0) {
            var items = clone(DeviceModel.radialItems)
            children[freeSlot] = clone(entry)
            items[index].children = children
            items[sourceIndex] = emptyEntry()
            if (!persist(items, DeviceModel.radialClick)) return
            enterFolder(index)
            selectedSlot = freeSlot
        } else {
            enterFolder(index)
            assignEntry(freeSlot, entry, -1)
        }
    }
    function clearSelected() {
        var items = clone(DeviceModel.radialItems)
        if (folderIndex >= 0) {
            items[folderIndex].children = padded(items[folderIndex].children || [], 9)
            items[folderIndex].children[selectedSlot] = emptyEntry()
        } else items[selectedSlot] = emptyEntry()
        if (selectedEntry.kind === "folder") confirmReplacement(items, qsTr("Remove this folder and its actions?"))
        else persist(items, DeviceModel.radialClick)
    }
    function confirmReplacement(items, message) {
        pendingItems = clone(items); pendingMessage = message; confirmDialog.open()
    }
    function applyPreset(index) {
        function action(label, key) { return {label: label, action: "keystroke:" + key} }
        var clipboard = {kind: "folder", label: qsTr("Clipboard"), icon: "folder", children: [
            action(qsTr("Copy"), "Ctrl+C"), action(qsTr("Paste"), "Ctrl+V"), action(qsTr("Cut"), "Ctrl+X")
        ]}
        var items
        if (index === 1) items = [action(qsTr("New tab"), "Ctrl+T"), action(qsTr("Reopen tab"), "Ctrl+Shift+T"),
            action(qsTr("Next tab"), "Ctrl+Tab"), action(qsTr("Previous tab"), "Ctrl+Shift+Tab"),
            action(qsTr("Address bar"), "Ctrl+L"), action(qsTr("Find"), "Ctrl+F"), action(qsTr("Reload"), "Ctrl+R"), clipboard]
        else if (index === 2) items = [action(qsTr("New folder"), "Ctrl+Shift+N"), action(qsTr("Rename"), "F2"),
            action(qsTr("Parent folder"), "Alt+Up"), action(qsTr("Hidden files"), "Ctrl+H"),
            action(qsTr("Address bar"), "Ctrl+L"), action(qsTr("Select all"), "Ctrl+A"), action(qsTr("New tab"), "Ctrl+T"), clipboard]
        else if (index === 3) items = [action(qsTr("Open file"), "Ctrl+P"), action(qsTr("Command palette"), "Ctrl+Shift+P"),
            action(qsTr("Save"), "Ctrl+S"), action(qsTr("Find"), "Ctrl+F"), action(qsTr("Find in files"), "Ctrl+Shift+F"),
            action(qsTr("Undo"), "Ctrl+Z"), action(qsTr("Redo"), "Ctrl+Shift+Z"), clipboard]
        else items = [clipboard, action(qsTr("Undo"), "Ctrl+Z"), action(qsTr("Redo"), "Ctrl+Shift+Z"),
            {kind: "adjustment", label: qsTr("Volume"), parameter: "volume"},
            {kind: "adjustment", label: qsTr("Brightness"), parameter: "brightness"},
            action(qsTr("Play / pause"), "Play"), action(qsTr("Mute"), "Mute"), action(qsTr("Find"), "Ctrl+F")]
        confirmReplacement(items, qsTr("Replace the current profile’s ring with this preset? Other button settings stay unchanged."))
    }
    Connections {
        target: DeviceModel
        function onSelectedChanged() { root.resetView() }
        function onActiveProfileNameChanged() { root.resetView() }
    }
    GridLayout {
        width: root.availableWidth
        columns: root.availableWidth >= 760 ? 2 : 1
        columnSpacing: 0; rowSpacing: 0
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredWidth: 700
            Layout.preferredHeight: Math.max(510, root.availableHeight)
            color: Theme.dark ? "#141416" : "#FAFAFA"
            ColumnLayout {
                anchors.fill: parent; anchors.margins: 24
                spacing: 12
                RowLayout {
                    Layout.fillWidth: true; spacing: 12
                    RingIconButton {
                        visible: root.navigationEnabled
                        glyph: "back"; hint: qsTr("Back")
                        onClicked: root.backRequested()
                    }
                    AppProfileBar { Layout.fillWidth: true; compact: true }
                }
                RowLayout {
                    Layout.fillWidth: true; visible: root.folderIndex >= 0
                    RingIconButton {
                        objectName: "folderBack"
                        glyph: "back"; hint: qsTr("Main ring")
                        onClicked: root.leaveFolder()
                    }
                    Text {
                        Layout.fillWidth: true; color: Theme.text; font.pixelSize: 14; font.weight: Font.Medium
                        text: root.currentFolder ? root.currentFolder.label : ""
                        elide: Text.ElideRight
                    }
                    RingIconButton {
                        objectName: "editFolder"
                        glyph: "edit"; hint: qsTr("Edit folder")
                        onClicked: root.editEntry(root.folderIndex, true)
                    }
                }
                RadialMenuRing {
                    id: previewRing
                    objectName: "editorRing"
                    Layout.fillWidth: true; Layout.fillHeight: true
                    Layout.minimumHeight: 320
                    items: root.currentItems
                    selected: root.selectedSlot
                    inFolder: root.folderIndex >= 0
                    interactive: true
                    dragLayer: Overlay.overlay
                    onActionClicked: function(index) {
                        root.selectedSlot = index
                        if (root.currentItems[index].kind === "folder") root.enterFolder(index)
                    }
                    onEditRequested: function(index) { root.editEntry(index) }
                    onContextRequested: function(index) { root.selectedSlot = index; slotMenu.popup() }
                    onCloseRequested: root.leaveFolder()
                    onEntryDropped: function(index, entry, sourceIndex) { root.dropEntry(index, entry, sourceIndex) }
                }
                Text {
                    Layout.fillWidth: true; visible: root.error.length > 0
                    text: root.error; color: Theme.text; wrapMode: Text.Wrap; font.pixelSize: 12
                }
                RowLayout {
                    Layout.fillWidth: true; spacing: 8
                    RingIconButton {
                        objectName: "undoRing"
                        glyph: "undo"; hint: qsTr("Undo change"); enabled: root.undoItems !== null
                        onClicked: root.restoreSnapshot(false)
                    }
                    RingIconButton {
                        objectName: "redoRing"
                        glyph: "redo"; hint: qsTr("Redo change"); enabled: root.redoItems !== null
                        onClicked: root.restoreSnapshot(true)
                    }
                    Item { Layout.fillWidth: true }
                    RingIconButton {
                        objectName: "editSelectedAction"
                        glyph: "edit"; hint: qsTr("Edit action")
                        onClicked: root.editEntry(root.selectedSlot)
                    }
                    RingIconButton {
                        objectName: "clearSelectedAction"
                        glyph: "trash"; hint: qsTr("Clear slot"); enabled: root.selectedEntry.kind !== "empty"
                        onClicked: root.clearSelected()
                    }
                    RingIconButton {
                        id: moreButton
                        glyph: "more"; hint: qsTr("Ring actions")
                        onClicked: ringMenu.popup(moreButton, 0, -ringMenu.height)
                    }
                    RingIconButton {
                        id: helpButton
                        glyph: "help"; hint: qsTr("Help")
                        onClicked: helpPopup.open()
                    }
                    RingIconButton {
                        id: settingsButton
                        glyph: "settings"; hint: qsTr("Ring settings")
                        onClicked: settingsPopup.open()
                    }
                }
            }
        }
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredWidth: 350
            Layout.maximumWidth: root.availableWidth >= 760 ? 380 : -1
            Layout.minimumWidth: Math.min(290, root.availableWidth)
            Layout.preferredHeight: Math.max(510, root.availableHeight)
            color: Theme.background
            Rectangle { width: 1; height: parent.height; color: Theme.border; visible: root.availableWidth >= 760 }
            RadialActionCatalog {
                id: actionCatalog
                objectName: "actionCatalog"
                anchors.fill: parent; anchors.margins: 20
                specialEntries: root.specialEntries
                inFolder: root.folderIndex >= 0
                dragLayer: Overlay.overlay
                onChosen: function(entry) { root.assignEntry(root.selectedSlot, entry, -1) }
            }
        }
    }
    Menu {
        id: slotMenu
        width: 240
        background: Rectangle { color: Theme.surface; radius: 10; border.color: Theme.border }
        RingMenuItem { text: qsTr("Edit action"); onTriggered: root.editEntry(root.selectedSlot) }
        RingMenuItem { text: qsTr("Clear slot"); enabled: root.selectedEntry.kind !== "empty"; onTriggered: root.clearSelected() }
    }
    Menu {
        id: ringMenu
        width: 250
        background: Rectangle { color: Theme.surface; radius: 10; border.color: Theme.border }
        RingMenuItem { text: qsTr("Copy ring"); onTriggered: root.copiedItems = root.clone(DeviceModel.radialItems) }
        RingMenuItem {
            text: qsTr("Paste ring"); enabled: root.copiedItems !== null
            onTriggered: root.confirmReplacement(root.copiedItems, qsTr("Replace this profile’s ring with the copied ring?"))
        }
        MenuSeparator { contentItem: Rectangle { implicitHeight: 1; color: Theme.border } }
        RingMenuItem { text: qsTr("Apply preset"); onTriggered: presetsPopup.open() }
    }
    Popup {
        id: presetsPopup
        parent: Overlay.overlay; anchors.centerIn: parent
        width: Math.min(350, parent ? parent.width - 32 : 350)
        modal: true; padding: 20
        background: Rectangle { color: Theme.surface; radius: 12; border.color: Theme.border }
        contentItem: ColumnLayout {
            spacing: 12
            Text { text: qsTr("Apply preset"); font.pixelSize: 18; font.bold: true; color: Theme.text }
            Repeater {
                model: [qsTr("Everyday actions"), qsTr("Browser"), qsTr("File manager"), qsTr("Code editor (VS Code)")]
                RingButton {
                    required property int index
                    required property string modelData
                    Layout.fillWidth: true; text: modelData
                    onClicked: { presetsPopup.close(); root.applyPreset(index) }
                }
            }
        }
    }
    Popup {
        id: helpPopup
        parent: Overlay.overlay; anchors.centerIn: parent
        width: Math.min(380, parent ? parent.width - 32 : 380)
        modal: true; padding: 24
        background: Rectangle { color: Theme.surface; radius: 12; border.color: Theme.border }
        contentItem: ColumnLayout {
            spacing: 16
            Text { text: qsTr("Action Ring"); color: Theme.text; font.pixelSize: 20; font.bold: true }
            Text {
                Layout.fillWidth: true; wrapMode: Text.Wrap; color: Theme.textSecondary
                text: qsTr("Select a position and choose an action on the right, or drag it onto the ring. Double-click an action to edit it.")
            }
            Text {
                Layout.fillWidth: true; wrapMode: Text.Wrap; color: Theme.textSecondary
                text: qsTr("Click a folder to open it. Use the arrow in the centre to return to the main ring. Right-click an item for more options.")
            }
            RingButton { Layout.alignment: Qt.AlignRight; text: qsTr("Close"); onClicked: helpPopup.close() }
        }
    }
    Popup {
        id: settingsPopup
        objectName: "ringSettingsPopup"
        parent: Overlay.overlay; anchors.centerIn: parent
        width: Math.min(360, parent ? parent.width - 32 : 360)
        modal: true; padding: 24
        background: Rectangle { color: Theme.surface; radius: 12; border.color: Theme.border }
        contentItem: ColumnLayout {
            spacing: 16
            Text { text: qsTr("Ring settings"); font.pixelSize: 20; font.bold: true; color: Theme.text }
            RingComboBox {
                Layout.fillWidth: true; visible: DeviceModel.count > 1
                model: DeviceModel; textRole: "deviceName"; currentIndex: DeviceModel.selectedIndex
                onActivated: function(index) { DeviceModel.selectedIndex = index }
            }
            Text { text: qsTr("Ring size"); color: Theme.textSecondary }
            RingComboBox {
                Layout.fillWidth: true
                model: [qsTr("Small"), qsTr("Medium"), qsTr("Large")]
                currentIndex: RadialMenu.sizeIndex
                onActivated: function(index) { RadialMenu.sizeIndex = index }
            }
            RingButton {
                visible: root.navigationEnabled
                Layout.fillWidth: true; text: qsTr("Button settings")
                onClicked: { settingsPopup.close(); root.buttonsRequested() }
            }
            RingButton { Layout.alignment: Qt.AlignRight; text: qsTr("Close"); onClicked: settingsPopup.close() }
        }
    }
    Dialog {
        id: confirmDialog
        parent: Overlay.overlay; anchors.centerIn: parent
        width: Math.min(440, parent ? parent.width - 32 : 440)
        modal: true; padding: 20
        background: Rectangle { radius: 12; color: Theme.surface; border.color: Theme.cardBorder }
        ColumnLayout {
            width: parent.width; spacing: 16
            Text { Layout.fillWidth: true; text: root.pendingMessage; wrapMode: Text.Wrap; color: Theme.text }
            RowLayout {
                Layout.alignment: Qt.AlignRight
                RingButton { text: qsTr("Cancel"); onClicked: confirmDialog.close() }
                RingButton {
                    text: qsTr("Apply"); primary: true
                    onClicked: {
                        if (root.persist(root.pendingItems, DeviceModel.radialClick)) {
                            root.folderIndex = -1; root.selectedSlot = 0; confirmDialog.close(); editDialog.close()
                        }
                    }
                }
            }
        }
    }
    Dialog {
        id: applicationPicker
        objectName: "applicationPicker"
        parent: Overlay.overlay; anchors.centerIn: parent
        width: Math.min(520, parent ? parent.width - 32 : 520)
        height: Math.min(560, parent ? parent.height - 32 : 560)
        modal: true; padding: 20
        background: Rectangle { radius: 12; color: Theme.surface; border.color: Theme.cardBorder }
        ColumnLayout {
            anchors.fill: parent; spacing: 12
            Text { text: qsTr("Launch application"); color: Theme.text; font.pixelSize: 20; font.bold: true }
            RingTextField {
                id: applicationSearch
                objectName: "applicationSearch"
                Layout.fillWidth: true
                placeholderText: qsTr("Search installed applications…")
            }
            Text {
                Layout.fillWidth: true; wrapMode: Text.Wrap; color: Theme.textSecondary
                visible: !root.launcherAvailable
                text: qsTr("Application selection requires gio. You can use Launch command instead.")
            }
            ListView {
                id: applicationList
                objectName: "applicationList"
                Layout.fillWidth: true; Layout.fillHeight: true
                clip: true; spacing: 6
                model: root.filteredApplications
                ScrollBar.vertical: ScrollBar {}
                delegate: ItemDelegate {
                    required property var modelData
                    width: applicationList.width
                    height: 54
                    enabled: root.launcherAvailable
                    background: Rectangle { radius: 8; color: parent.hovered ? Theme.hoverBg : Theme.cardBg; border.color: Theme.cardBorder }
                    contentItem: RowLayout {
                        spacing: 12
                        RadialActionIcon {
                            Layout.preferredWidth: 28; Layout.preferredHeight: 28
                            Layout.minimumWidth: 28; Layout.maximumWidth: 28
                            Layout.minimumHeight: 28; Layout.maximumHeight: 28
                            action: "app-launch:"
                            applicationIcon: modelData.icon || ""
                        }
                        ColumnLayout {
                            Layout.fillWidth: true; spacing: 2
                            Text { Layout.fillWidth: true; text: modelData.title; color: Theme.text; font.pixelSize: 13; elide: Text.ElideRight }
                            Text { Layout.fillWidth: true; text: modelData.desktopId; color: Theme.textSecondary; font.pixelSize: 10; elide: Text.ElideRight }
                        }
                    }
                    onClicked: root.chooseApplication(modelData)
                }
                Text {
                    anchors.centerIn: parent; width: parent.width; horizontalAlignment: Text.AlignHCenter
                    visible: applicationList.count === 0; wrapMode: Text.Wrap
                    text: qsTr("No applications found. You can enter a launch command manually.")
                    color: Theme.textSecondary
                }
            }
            RingButton { Layout.alignment: Qt.AlignRight; text: qsTr("Cancel"); onClicked: applicationPicker.close() }
        }
    }
    Dialog {
        id: editDialog
        objectName: "editDialog"
        parent: Overlay.overlay
        anchors.centerIn: parent
        width: Math.min(440, parent ? parent.width - 32 : 440)
        modal: true
        title: qsTr("Menu item")
        padding: 20
        background: Rectangle {
            radius: 12
            color: Theme.surface
            border.color: Theme.cardBorder
        }
        header: Text {
            text: editDialog.title
            color: Theme.text
            font.pixelSize: 18
            font.bold: true
            padding: 20
        }
        ColumnLayout {
            width: parent.width
            RingTextField {
                id: labelField
                objectName: "labelField"
                Layout.fillWidth: true
                placeholderText: qsTr("Name")
                maximumLength: 80
            }
            RingComboBox {
                id: actionKind
                objectName: "actionKind"
                Layout.fillWidth: true
                model: [qsTr("Existing action"), qsTr("Keyboard shortcut"), qsTr("Launch command"), qsTr("Empty slot"), qsTr("Folder"), qsTr("Volume adjustment"), qsTr("Brightness adjustment"), qsTr("Launch application")]
                onActivated: if (currentIndex === 7) root.openApplicationPicker()
            }
            RingComboBox {
                id: catalog
                objectName: "catalog"
                Layout.fillWidth: true
                visible: actionKind.currentIndex === 0
                model: RadialActionModel
                textRole: "name"
                valueRole: "serializedAction"
                displayText: qsTranslate("ActionModel", currentText)
                delegate: ItemDelegate {
                    required property string name
                    required property int index
                    width: catalog.width
                    text: qsTranslate("ActionModel", name)
                    highlighted: catalog.highlightedIndex === index
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
                onActivated: {
                    if (!labelField.text.length)
                        labelField.text = qsTranslate("ActionModel", currentText);
                    if (currentValue === "keystroke:")
                        actionKind.currentIndex = 1;
                }
            }
            RingTextField {
                id: payloadField
                objectName: "payloadField"
                Layout.fillWidth: true
                visible: actionKind.currentIndex === 1 || actionKind.currentIndex === 2
                placeholderText: actionKind.currentIndex === 1 ? "Ctrl+Shift+C" : "firefox --new-window"
            }
            RowLayout {
                Layout.fillWidth: true
                visible: actionKind.currentIndex === 7
                RadialActionIcon {
                    Layout.preferredWidth: 24; Layout.preferredHeight: 24
                    Layout.minimumWidth: 24; Layout.maximumWidth: 24
                    Layout.minimumHeight: 24; Layout.maximumHeight: 24
                    action: "app-launch:"
                    applicationIcon: root.selectedApplication ? root.selectedApplication.icon : ""
                }
                Text {
                    objectName: "chosenApplication"
                    Layout.fillWidth: true; color: Theme.text; elide: Text.ElideRight
                    text: root.selectedApplication ? root.selectedApplication.title : qsTr("No application selected")
                }
                RingButton { text: qsTr("Choose…"); onClicked: root.openApplicationPicker() }
            }
            KeystrokeCapture {
                Layout.fillWidth: true
                visible: actionKind.currentIndex === 1
                keystroke: payloadField.text
                onKeystrokeCaptured: function (ks) {
                    payloadField.text = ks;
                }
            }
            ColumnLayout {
                id: iconPicker
                objectName: "iconPicker"
                property int currentIndex: 0
                Layout.fillWidth: true
                spacing: 8
                Text { text: qsTr("Icon"); color: Theme.textSecondary; font.pixelSize: 12 }
                GridLayout {
                    Layout.fillWidth: true
                    columns: 8; columnSpacing: 4; rowSpacing: 4
                    Repeater {
                        model: root.iconNames
                        RingIconButton {
                            required property int index
                            required property string modelData
                            objectName: "iconChoice-" + (modelData || "automatic")
                            Layout.fillWidth: true
                            implicitWidth: 36; implicitHeight: 34; padding: 7
                            glyph: modelData || "action"
                            checked: iconPicker.currentIndex === index
                            hint: index === 0 ? qsTr("Automatic icon") : ""
                            Accessible.name: hint || modelData
                            onClicked: iconPicker.currentIndex = index
                        }
                    }
                }
            }
            Label {
                Layout.fillWidth: true
                visible: root.error.length > 0
                text: root.error
                color: Theme.text
                wrapMode: Text.Wrap
            }
            RowLayout {
                Layout.alignment: Qt.AlignRight
                RingButton {
                    text: qsTr("Cancel")
                    onClicked: editDialog.close()
                }
                RingButton {
                    primary: true
                    objectName: "saveEntry"
                    text: qsTr("Save")
                    onClicked: {
                        var action = actionKind.currentIndex === 0 ? catalog.currentValue : actionKind.currentIndex === 1 ? "keystroke:" + payloadField.text.trim() : actionKind.currentIndex === 2 ? "app-launch:" + payloadField.text.trim() : "default"
                        var entry = {label: labelField.text.trim(), action: action || "", icon: root.iconNames[iconPicker.currentIndex]}
                        if (actionKind.currentIndex === 3) entry = root.emptyEntry()
                        if (actionKind.currentIndex === 4) {
                            if (root.editingFolder >= 0) { root.error = qsTr("Folders can only be placed in the main ring."); return }
                            entry.kind = "folder"
                            entry.children = DeviceModel.radialItems[root.editingIndex].children || []
                        }
                        if (actionKind.currentIndex === 5 || actionKind.currentIndex === 6) {
                            entry.kind = "adjustment"
                            entry.parameter = actionKind.currentIndex === 5 ? "volume" : "brightness"
                        }
                        if (actionKind.currentIndex === 7) {
                            if (!root.selectedApplication) { root.error = qsTr("Choose an application first."); return }
                            entry.action = "app-launch:" + root.selectedApplication.command
                            entry.desktopFile = root.selectedApplication.desktopFile
                            entry.appIcon = root.selectedApplication.icon
                        }
                        if (root.writeEntry(root.editingIndex, entry, root.editingFolder)) {
                            if (root.openFolderAfterSave && entry.kind === "folder") root.enterFolder(root.editingIndex)
                            if (root.folderIndex >= 0 && DeviceModel.radialItems[root.folderIndex].kind !== "folder") root.folderIndex = -1
                            editDialog.close()
                        }
                    }
                }
            }
        }
    }
}
