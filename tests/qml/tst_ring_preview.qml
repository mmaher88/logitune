import QtQuick
import QtTest
import "../../src/app/qml/components/actionsring"

TestCase {
    name: "RingPreview"
    when: windowShown
    visible: true
    width: 1000; height: 900
    RingEditorState { id: state }
    RingPreview { id: preview; anchors.fill: parent; editor: state }
    function init() {
        state.loadProfile("test", Array.from({length: 8}, (_, i) => ({name: "Slot " + i, icon: "C", action: "keystroke:C"})), 100)
    }
    function test_clickSelectsBubble() {
        let bubble = findChild(preview, "bubble-2--1")
        verify(bubble !== null)
        mouseClick(bubble, bubble.width / 2, bubble.height / 2)
        compare(state.selectedSlot, 2)
    }
    function test_realGeometryAndFan() {
        let top = findChild(preview, "bubble-0--1")
        compare(Math.round(top.y + top.height / 2), Math.round(preview.height / 2 - 82))
        state.assign({name: "Menu", icon: "M", children: []})
        tryVerify(() => findChild(preview, "bubble-0-0") !== null)
        let child = findChild(preview, "bubble-0-0")
        compare(Math.round(child.y + child.height / 2), Math.round(preview.height / 2 - 182))
    }
    function test_bubbleDragSwaps() {
        state.select(0, -1); state.rename("Moved")
        let from = findChild(preview, "bubble-0--1")
        let to = findChild(preview, "bubble-2--1")
        let point = from.mapFromItem(to, to.width / 2, to.height / 2)
        mouseDrag(from, from.width / 2, from.height / 2, point.x - from.width / 2, point.y - from.height / 2, Qt.LeftButton)
        tryCompare(state.entries[2], "name", "Moved")
    }
}
