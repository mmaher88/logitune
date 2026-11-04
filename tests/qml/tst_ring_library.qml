import QtQuick
import QtTest
import "../../src/app/qml/components/actionsring"

TestCase {
    name: "RingLibrary"
    when: windowShown
    width: 400; height: 700
    Component { id: factory; RingActionLibrary {} }
    function test_searchAcrossGroups() {
        let library = createTemporaryObject(factory, this, {width: 360, height: 650})
        verify(library !== null)
        library.query = "copy"
        verify(library.filtered.length > 0)
        compare(library.filtered[0].name, "Copy")
        library.query = "volume"
        verify(library.filtered.length >= 2)
        library.query = "no such action"
        compare(library.filtered.length, 0)
    }
    function test_parameterAndSubmenuEntries() {
        let library = createTemporaryObject(factory, this)
        verify(library.actions.some(a => a.kind === "app"))
        verify(library.actions.some(a => a.kind === "url"))
        verify(library.actions.some(a => a.children !== undefined))
        verify(!library.actions.some(a => a.action === "dpi-cycle:"))
    }
}
