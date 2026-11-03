import QtQuick
import QtTest
import "../../src/app/qml/components/actionsring"

TestCase {
    name: "ActionsRing"
    when: windowShown
    width: 800
    height: 800
    QtObject {
        id: bridge
        property int pulses: 0
        property string chosen: ""
        property bool closed: false
        function haptic(kind) { pulses++ }
        function run(action) { chosen = action }
        function placed(ring) {}
        function finished() { closed = true }
    }
    Ring {
        id: ring
        anchors.fill: parent
        launcher: bridge
        slotList: [
            {icon: "A", name: "One", action: "keystroke:Ctrl+C"},
            {icon: "B", name: "Two", action: "media:VolumeMute"},
            {icon: "C", name: "Three", action: "default"},
            {icon: "D", name: "Four", action: "default"},
            {icon: "E", name: "Media", children: [
                {icon: "F", name: "Previous", action: "media:MediaPrevious"},
                {icon: "G", name: "Play", action: "media:MediaPlay"},
                {icon: "H", name: "Next", action: "media:MediaNext"}]}]
    }
    function init() {
        ring.hubX = 400; ring.hubY = 400
        ring.expandedParent = -1
        ring.hovered = ring.noTarget
        ring.hoveredChild = -1
        bridge.pulses = 0; bridge.chosen = ""
    }
    function test_centreAndReach() {
        compare(ring.hitTest(400, 400).slot, ring.centreTarget)
        compare(ring.hitTest(400, 318).slot, 0)
        compare(ring.hitTest(790, 400).slot, ring.noTarget)
    }
    function test_parentWedgeSelectsNearestChild() {
        ring.updateHover(400, 482)
        compare(ring.expandedParent, 4)
        compare(ring.hitTest(400, 482).child, 1)
        compare(ring.hitTest(400, 520).child, 1)
    }
    function test_hoverOnlyPulsesOnChange() {
        ring.updateHover(400, 318)
        compare(bridge.pulses, 1)
        ring.updateHover(401, 318)
        compare(bridge.pulses, 1)
    }
    function test_integratedActionNotShellCommand() {
        ring.choose(0, -1)
        compare(bridge.chosen, "keystroke:Ctrl+C")
    }
    Component { id: editorComponent; RingEditor {} }
    function test_editorLoadsAndEdits() {
        let editor = createTemporaryObject(editorComponent, this, {width: 360, height: 500})
        verify(editor !== null)
        compare(editor.entries.length, 8)
        let entry = Object.assign({}, editor.entries[0])
        entry.name = "Edited terminal"
        editor.save(0, entry)
        compare(editor.entries[0].name, "Edited terminal")
        editor.selectedSlot = 6
        entry = Object.assign({}, editor.entries[6])
        entry.name = "Edited undo"
        editor.save(6, entry)
        compare(editor.selectedSlot, 6)
        compare(editor.entries[6].name, "Edited undo")
    }
}
