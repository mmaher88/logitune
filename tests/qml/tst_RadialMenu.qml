import QtQuick
import QtQuick.Controls
import QtTest
import Logitune

TestCase {
    name: "RadialMenuEditor"
    when: windowShown
    width: 640; height: 700
    ApplicationWindow {
        id: host
        width: 600; height: 650
        visible: true
        RadialMenuEditor { id: editor; anchors.fill: parent }
    }
    RadialMenuOverlay { id: overlay }
    SignalSpy { id: actionSpy; target: RadialMenu; signalName: "triggered" }
    property var initialItems: []
    function initTestCase() { initialItems = JSON.parse(JSON.stringify(DeviceModel.radialItems)) }
    function init() {
        failOnWarning(/.*(Binding loop|Error decoding|Rejecting image|Failed to get image).*/)
        TestSupport.hideRadialMenu()
        DeviceModel.saveRadialMenu(initialItems, DeviceModel.radialClick)
        editor.resetView()
    }
    function cleanup() { TestSupport.hideRadialMenu(); editor.resetView(); host.width = 600 }
    function test_editCommandAndPersist() {
        editor.editEntry(0)
        var dialog = findChild(host.contentItem, "editDialog")
        // Popup is reparented to the window overlay.
        if (!dialog) dialog = findChild(editor, "editDialog")
        verify(dialog !== null)
        var label = findChild(dialog, "labelField")
        var kind = findChild(dialog, "actionKind")
        var payload = findChild(dialog, "payloadField")
        verify(label !== null)
        label.text = "Browser"
        kind.currentIndex = 2
        payload.text = "firefox --new-window"
        var save = findChild(dialog, "saveEntry")
        save.clicked()
        compare(DeviceModel.radialItems[0].label, "Browser")
        compare(DeviceModel.radialItems[0].action, "app-launch:firefox --new-window")
        tryCompare(dialog, "visible", false)
        compare(overlay.visible, false)
    }
    function test_overlayShowsWithoutTakingFocus() {
        host.requestActivate()
        wait(100)
        var wasActive = host.active
        TestSupport.showRadialMenu()
        tryCompare(overlay, "visible", true)
        wait(100)
        if (TestSupport.hasWindowManager()) {
            verify(!overlay.active)
            if (wasActive) verify(host.active)
        }
        var closeButton = findChild(overlay.contentItem, "ringClose")
        verify(closeButton !== null)
        mouseClick(closeButton)
        tryCompare(overlay, "visible", false)
    }
    function test_cancelPendingCursorQueryAndReopen() {
        TestSupport.showRadialMenu()
        TestSupport.hideRadialMenu()
        wait(300)
        compare(overlay.visible, false)
        TestSupport.showRadialMenu()
        tryCompare(overlay, "visible", true)
        TestSupport.hideRadialMenu()
        tryCompare(overlay, "visible", false)
    }
    function test_clickActionClosesMenuAndEmitsOnce() {
        actionSpy.clear()
        TestSupport.showRadialMenu()
        tryCompare(overlay, "visible", true)
        wait(300)
        compare(overlay.visible, true)
        compare(actionSpy.count, 0)
        var item = findChild(overlay.contentItem, "ringItem-0")
        verify(item !== null)
        mouseClick(item)
        tryCompare(overlay, "visible", false)
        compare(actionSpy.count, 1)
    }
    function test_invalidDoesNotReplaceConfiguration() {
        var original = DeviceModel.radialItems
        verify(!editor.persist([{label: "One", action: "default"}], DeviceModel.radialClick))
        compare(DeviceModel.radialItems.length, original.length)
    }
    function test_folderEditorAndRuntimeNavigation() {
        editor.assignEntry(0, {kind: "folder", label: "Editing", icon: "folder", children: []}, -1)
        editor.folderIndex = 0
        compare(editor.currentItems.length, 9)
        editor.assignEntry(8, {label: "Undo", action: "keystroke:Ctrl+Z", icon: "undo"}, -1)
        compare(DeviceModel.radialItems[0].children[8].label, "Undo")
        editor.assignEntry(7, {kind: "folder", label: "Nested", children: []}, -1)
        compare(DeviceModel.radialItems[0].children[7].kind, "empty")
        actionSpy.clear()
        TestSupport.showRadialMenu()
        tryCompare(overlay, "visible", true)
        mouseClick(findChild(overlay.contentItem, "ringItem-0"))
        compare(RadialMenu.inFolder, true)
        compare(actionSpy.count, 0)
        mouseClick(findChild(overlay.contentItem, "ringClose"))
        compare(RadialMenu.inFolder, false)
        compare(overlay.visible, true)
        mouseClick(findChild(overlay.contentItem, "ringItem-0"))
        mouseClick(findChild(overlay.contentItem, "ringItem-8"))
        tryCompare(overlay, "visible", false)
        compare(actionSpy.count, 1)
    }
    function test_reorderKeepsAllSlotsAndUndoSnapshot() {
        var first = DeviceModel.radialItems[0].action
        var second = DeviceModel.radialItems[1].action
        editor.assignEntry(1, DeviceModel.radialItems[0], 0)
        compare(DeviceModel.radialItems.length, 8)
        compare(DeviceModel.radialItems[0].action, second)
        compare(DeviceModel.radialItems[1].action, first)
        compare(editor.undoItems[0].action, first)
        editor.selectedSlot = 0
        editor.clearSelected()
        compare(DeviceModel.radialItems[0].kind, "empty")
        compare(DeviceModel.radialItems[1].action, first)
    }
    function test_scrollAdjustsWithoutClosing() {
        editor.assignEntry(0, {kind: "adjustment", label: "Volume", parameter: "volume"}, -1)
        actionSpy.clear()
        TestSupport.showRadialMenu()
        tryCompare(overlay, "visible", true)
        var item = findChild(overlay.contentItem, "ringItem-0")
        mouseWheel(item, item.width / 2, item.height / 2, 0, 120)
        tryCompare(actionSpy, "count", 1)
        compare(overlay.visible, true)
        compare(RadialMenu.adjustmentSteps, 1)
    }

    function test_dragFromCatalogAndSwapSlots() {
        host.width = 1100
        wait(100)
        var mediaCategory = findChild(editor, "catalogCategory-media")
        verify(mediaCategory !== null)
        mediaCategory.clicked()
        wait(60)
        var volume = findChild(editor, "catalogSpecial-3")
        var ring = findChild(editor, "editorRing")
        var destination = findChild(ring, "ringItem-0")
        verify(volume !== null && destination !== null)
        var target = destination.mapToItem(host.contentItem, destination.width / 2, destination.height / 2)
        mousePress(volume, 30, 25)
        mouseMove(volume, 10, 25, 30)
        mouseMove(host.contentItem, target.x, target.y, 100)
        mouseRelease(host.contentItem, target.x, target.y)
        tryCompare(DeviceModel.radialItems[0], "kind", "adjustment")
        compare(DeviceModel.radialItems[0].parameter, "volume")
        var source = findChild(ring, "ringItem-0")
        destination = findChild(ring, "ringItem-1")
        target = destination.mapToItem(host.contentItem, destination.width / 2, destination.height / 2)
        mousePress(source, 27, 27)
        mouseMove(source, 47, 27, 30)
        mouseMove(host.contentItem, target.x, target.y, 100)
        mouseRelease(host.contentItem, target.x, target.y)
        compare(DeviceModel.radialItems[1].parameter, "volume")
        compare(DeviceModel.radialItems[0].action, "keystroke:Ctrl+V")
    }

    function test_presetsAreValidAndCopyIsIndependent() {
        for (var i = 0; i < 4; ++i) {
            var before = JSON.stringify(DeviceModel.radialItems)
            editor.applyPreset(i)
            compare(JSON.stringify(DeviceModel.radialItems), before) // Confirmation precedes replacement.
            verify(DeviceModel.saveRadialMenu(editor.pendingItems, DeviceModel.radialClick))
            compare(DeviceModel.radialItems.length, 8)
            editor.resetView()
        }
        editor.copiedItems = editor.clone(DeviceModel.radialItems)
        var copied = JSON.stringify(editor.copiedItems)
        editor.selectedSlot = 0
        editor.clearSelected()
        compare(JSON.stringify(editor.copiedItems), copied)
    }
    function test_sizeChangesOverlayAndCanBeRestored() {
        var original = RadialMenu.sizeIndex
        try {
            RadialMenu.sizeIndex = 0
            TestSupport.showRadialMenu()
            tryCompare(overlay, "visible", true)
            compare(overlay.width, 320)
            RadialMenu.sizeIndex = 2
            tryCompare(overlay, "width", 460)
            compare(overlay.visible, true)
        } finally { RadialMenu.sizeIndex = original }
    }

    function test_applicationPickerSearchSaveAndReopen() {
        var before = JSON.stringify(DeviceModel.radialItems)
        editor.assignEntry(0, {action: "application-picker:"}, -1)
        editor.applications = [
            {title: "Browser", desktopId: "browser.desktop", desktopFile: "/tmp/browser.desktop", icon: "web-browser", command: 'gio launch "/tmp/browser.desktop"'},
            {title: "Files", desktopId: "files.desktop", desktopFile: "/tmp/files.desktop", icon: "system-file-manager", command: 'gio launch "/tmp/files.desktop"'}
        ]
        var picker = findChild(editor, "applicationPicker")
        verify(picker !== null)
        var search = findChild(picker, "applicationSearch")
        search.text = "BROWSER"
        compare(editor.filteredApplications.length, 1)
        compare(editor.filteredApplications[0].title, "Browser")
        compare(JSON.stringify(DeviceModel.radialItems), before)
        editor.chooseApplication(editor.filteredApplications[0])
        var dialog = findChild(editor, "editDialog")
        findChild(dialog, "saveEntry").clicked()
        compare(DeviceModel.radialItems[0].action, 'app-launch:gio launch "/tmp/browser.desktop"')
        compare(DeviceModel.radialItems[0].appIcon, "web-browser")
        compare(DeviceModel.radialItems[0].label, "Browser")
        editor.editEntry(0)
        compare(findChild(dialog, "actionKind").currentIndex, 7)
        compare(editor.selectedApplication.command, 'gio launch "/tmp/browser.desktop"')
        findChild(dialog, "labelField").text = "My browser"
        findChild(dialog, "saveEntry").clicked()
        compare(DeviceModel.radialItems[0].appIcon, "web-browser")
        compare(DeviceModel.radialItems[0].label, "My browser")
        actionSpy.clear()
        TestSupport.showRadialMenu()
        tryCompare(overlay, "visible", true)
        mouseClick(findChild(overlay.contentItem, "ringItem-0"))
        tryCompare(overlay, "visible", false)
        compare(actionSpy.count, 1)
    }
    function test_cancelApplicationSelectionKeepsBinding() {
        var before = JSON.stringify(DeviceModel.radialItems)
        editor.assignEntry(0, {action: "application-picker:"}, -1)
        editor.resetView()
        compare(JSON.stringify(DeviceModel.radialItems), before)
    }

    function test_catalogSearchClearRestoresVisibleRows_data() {
        return [{tag: "narrow", width: 620}, {tag: "wide", width: 1100}]
    }
    function test_catalogSearchClearRestoresVisibleRows(data) {
        host.width = data.width
        wait(80)
        var search = findChild(editor, "ringSearch")
        var list = findChild(editor, "ringCatalogList")
        var catalogView = findChild(editor, "actionCatalog")
        function assertLaunchVisible() {
            var row = findChild(editor, "catalogSpecial-5")
            return row !== null && row.visible && row.width > 100 && row.height > 0
        }
        tryVerify(assertLaunchVisible)
        var originalCount = list.count
        for (var i = 0; i < 3; ++i) {
            search.text = "zz-no-matching-action-12345"
            tryCompare(list, "count", 0)
            verify(findChild(editor, "catalogNoResults").visible)
            findChild(editor, "clearRingSearch").clicked()
            tryCompare(list, "count", originalCount)
            tryVerify(assertLaunchVisible)
            verify(!findChild(editor, "catalogNoResults").visible)
        }
        // Backspace/selection deletion must restore the catalog too.
        search.text = "missing"
        host.requestActivate()
        wait(50)
        search.forceActiveFocus()
        search.selectAll()
        keyClick(Qt.Key_Backspace)
        tryCompare(search, "text", "")
        tryVerify(assertLaunchVisible)
        // Clearing whitespace also restores the same entries.
        search.text = "  "
        tryVerify(assertLaunchVisible)
        compare(catalogView.expandedCategory, "launch")
    }
    function test_catalogSearchIncludesCollapsedCategoriesAndResetsScroll() {
        host.width = 1100
        wait(80)
        var search = findChild(editor, "ringSearch")
        var list = findChild(editor, "ringCatalogList")
        var catalogView = findChild(editor, "actionCatalog")
        compare(catalogView.expandedCategory, "launch")
        search.text = "VOLUME"
        tryVerify(function() { return findChild(editor, "catalogSpecial-3") !== null })
        verify(catalogView.rows.some(function(row) { return row.entry && row.entry.action === "keystroke:VolumeUp" }))
        // A previously scrolled list must not leave new results off-screen.
        search.clear()
        wait(40)
        list.positionViewAtEnd()
        search.text = "Launch application"
        tryCompare(list, "count", 2)
        wait(40)
        var application = findChild(editor, "catalogSpecial-5")
        verify(application !== null && application.width > 100)
        var position = application.mapToItem(list, 0, 0)
        verify(position.y >= 0 && position.y < list.height)
        application.chosen(application.entry)
        tryCompare(findChild(editor, "applicationPicker"), "visible", true)
    }
    function test_catalogWorksAfterClearingSlotAndEnteringFolder() {
        host.width = 1100
        wait(60)
        editor.selectedSlot = 0
        editor.clearSelected()
        compare(DeviceModel.radialItems[0].kind, "empty")
        var catalogView = findChild(editor, "actionCatalog")
        verify(catalogView.rows.some(function(row) { return row.entry && row.entry.action === "application-picker:" }))
        editor.assignEntry(0, {kind: "folder", label: "Apps", children: []}, -1)
        editor.folderIndex = 0
        wait(40)
        verify(!catalogView.rows.some(function(row) { return row.key === "folders" }))
        verify(catalogView.rows.some(function(row) { return row.entry && row.entry.action === "application-picker:" }))
    }

    function test_createFolderRenameLastChildAndReturn() {
        host.width = 1100
        wait(60)
        editor.assignEntry(3, editor.specialEntries[0], -1)
        var dialog = findChild(editor, "editDialog")
        tryCompare(dialog, "visible", true)
        findChild(dialog, "labelField").text = "Tools"
        findChild(dialog, "saveEntry").clicked()
        compare(editor.folderIndex, 3)
        compare(editor.currentItems.length, 9)
        compare(DeviceModel.radialItems[3].label, "Tools")
        editor.assignEntry(8, {label: "Find", action: "keystroke:Ctrl+F", icon: "search"}, -1)
        editor.editEntry(8)
        findChild(dialog, "labelField").text = "Search files"
        findChild(dialog, "saveEntry").clicked()
        compare(DeviceModel.radialItems[3].children[8].label, "Search files")
        findChild(editor, "editFolder").clicked()
        findChild(dialog, "labelField").text = "My tools"
        findChild(dialog, "saveEntry").clicked()
        compare(editor.folderIndex, 3)
        compare(DeviceModel.radialItems[3].label, "My tools")
        compare(DeviceModel.radialItems[3].children[8].label, "Search files")
        tryCompare(dialog, "visible", false)
        host.requestActivate()
        wait(200)
        findChild(editor, "folderBack").clicked()
        compare(editor.folderIndex, -1)
        compare(editor.selectedSlot, 3)
        var ring = findChild(editor, "editorRing")
        // Let the folder header disappear and the preview layout settle before clicking.
        wait(60)
        mouseClick(findChild(ring, "ringItem-3"))
        compare(editor.folderIndex, 3)
        wait(60)
        mouseClick(findChild(ring, "ringClose"))
        compare(editor.folderIndex, -1)
        compare(editor.selectedSlot, 3)
    }
    function test_folderReplacementRequiresConfirmationAndSupportsUndoRedo() {
        editor.assignEntry(2, {kind: "folder", label: "Tools", children: [{label: "Copy", action: "keystroke:Ctrl+C"}]}, -1)
        var original = JSON.stringify(DeviceModel.radialItems)
        editor.assignEntry(2, {label: "Undo", action: "keystroke:Ctrl+Z"}, -1)
        compare(JSON.stringify(DeviceModel.radialItems), original)
        compare(editor.pendingItems[2].action, "keystroke:Ctrl+Z")
        verify(editor.persist(editor.pendingItems, DeviceModel.radialClick))
        findChild(editor, "undoRing").clicked()
        compare(DeviceModel.radialItems[2].kind, "folder")
        compare(DeviceModel.radialItems[2].children[0].action, "keystroke:Ctrl+C")
        findChild(editor, "redoRing").clicked()
        compare(DeviceModel.radialItems[2].action, "keystroke:Ctrl+Z")
        editor.restoreSnapshot(false)
        editor.assignEntry(1, {label: "Paste", action: "keystroke:Ctrl+V"}, -1)
        compare(editor.redoItems, null)
    }
    function test_allIconChoicesSaveAndReopen() {
        for (var i = 0; i < editor.iconNames.length; ++i) {
            editor.editEntry(0)
            var dialog = findChild(editor, "editDialog")
            var choice
            tryVerify(function() {
                choice = findChild(dialog.contentItem, "iconChoice-" + (editor.iconNames[i] || "automatic"))
                return choice !== null
            }, 1000, "Missing icon choice: " + editor.iconNames[i])
            choice.clicked()
            findChild(dialog, "saveEntry").clicked()
            compare(editor.error, "")
            compare(DeviceModel.radialItems[0].icon || "", editor.iconNames[i])
            editor.editEntry(0)
            compare(findChild(dialog, "iconPicker").currentIndex, i)
            dialog.close()
        }
    }

    function test_dropOntoFolderAddsInsideWithoutReplacingIt() {
        host.width = 1100
        editor.assignEntry(0, {kind: "folder", label: "Tools", children: []}, -1)
        wait(80)
        findChild(editor, "catalogCategory-media").clicked()
        wait(60)
        var volume = findChild(editor, "catalogSpecial-3")
        var ring = findChild(editor, "editorRing")
        var folder = findChild(ring, "ringItem-0")
        var point = folder.mapToItem(host.contentItem, folder.width / 2, folder.height / 2)
        mousePress(volume, 30, 25)
        mouseMove(volume, 10, 25, 30)
        mouseMove(host.contentItem, point.x, point.y, 100)
        mouseRelease(host.contentItem, point.x, point.y)
        compare(DeviceModel.radialItems[0].kind, "folder")
        compare(DeviceModel.radialItems[0].children[0].parameter, "volume")
        compare(editor.folderIndex, 0)
        editor.leaveFolder()
        var movedAction = DeviceModel.radialItems[1].action
        editor.dropEntry(0, DeviceModel.radialItems[1], 1)
        compare(DeviceModel.radialItems[1].kind, "empty")
        compare(DeviceModel.radialItems[0].children[1].action, movedAction)
        editor.restoreSnapshot(false)
        compare(DeviceModel.radialItems[1].action, movedAction)
        compare(DeviceModel.radialItems[0].children[1].kind, "empty")
        var children = []
        for (var i = 0; i < 9; ++i) children.push({label: "Copy", action: "keystroke:Ctrl+C"})
        editor.assignEntry(0, {kind: "folder", label: "Full", children: children}, -1)
        var original = JSON.stringify(DeviceModel.radialItems)
        editor.dropEntry(0, DeviceModel.radialItems[1], 1)
        compare(JSON.stringify(DeviceModel.radialItems), original)
        verify(editor.error.length > 0)
    }

}
