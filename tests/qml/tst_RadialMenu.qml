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
        compare(RadialMenu.selected, 0)
        wait(100)
        if (TestSupport.hasWindowManager()) {
            verify(!overlay.active)
            if (wasActive) verify(host.active)
        }
        TestSupport.hideRadialMenu()
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
    function test_invalidDoesNotReplaceConfiguration() {
        var original = DeviceModel.radialItems
        verify(!editor.persist([{label: "One", action: "default"}], DeviceModel.radialClick))
        compare(DeviceModel.radialItems.length, original.length)
    }
}
