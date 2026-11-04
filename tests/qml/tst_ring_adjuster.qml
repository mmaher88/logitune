import QtQuick
import QtTest
import "../../src/app/qml/components/actionsring"

TestCase {
    name: "RingAdjuster"
    when: windowShown
    visible: true
    width: 800
    height: 800

    QtObject {
        id: bridge
        property var commands: []
        property var reads: []
        property string chosen: ""
        signal levelRead(string output)
        function haptic(kind) {}
        function run(action) { chosen = action }
        property var readsAfterAdjust: []
        function adjust(command, readCommand) {
            commands = commands.concat([command])
            readsAfterAdjust = readsAfterAdjust.concat([readCommand])
        }
        function readLevel(command) { reads = reads.concat([command]) }
        function placed(ring) {}
        function finished() {}
    }

    Ring {
        id: ring
        anchors.fill: parent
        launcher: bridge
        slotList: [
            {icon: "V", name: "Volume", action: "default", adjust: {
                up: "vol-up", down: "vol-down", read: "vol-read", press: "vol-mute"}},
            {icon: "A", name: "Copy", action: "keystroke:Ctrl+C"}]
    }

    function init() {
        ring.hubX = 400
        ring.hubY = 400
        ring.isPlaced = true
        ring.hovered = ring.noTarget
        ring.hoveredChild = -1
        ring.closing = false
        bridge.commands = []
        bridge.reads = []
        bridge.readsAfterAdjust = []
        bridge.chosen = ""
    }

    function test_parsesWpctlAndPlainOutput() {
        compare(ring.parseLevel("Volume: 0.57").percent, 57)
        compare(ring.parseLevel("Volume: 0.40 [MUTED]").muted, true)
        compare(ring.parseLevel("45%").percent, 45)
        compare(ring.parseLevel("80").percent, 80)
        compare(ring.parseLevel("no number here").percent, -1)
    }

    function test_hoveringReadsTheLevel() {
        ring.hovered = 0
        compare(bridge.reads, ["vol-read"])
        bridge.levelRead("Volume: 0.30")
        compare(ring.level, 30)
    }

    function test_eachNotchIsOneStep() {
        ring.hovered = 0
        ring.scrollAdjuster(120)
        ring.scrollAdjuster(-240)
        compare(bridge.commands, ["vol-up", "vol-down", "vol-down"])
    }

    function test_smoothScrollingAddsUp() {
        ring.hovered = 0
        ring.scrollAdjuster(40)
        ring.scrollAdjuster(40)
        compare(bridge.commands, [])
        ring.scrollAdjuster(40)
        compare(bridge.commands, ["vol-up"])
    }

    // Reversing must not need the leftover from the other direction worked off first.
    function test_reversingDropsTheLeftover() {
        ring.hovered = 0
        ring.scrollAdjuster(200)
        compare(bridge.commands, ["vol-up"])
        ring.scrollAdjuster(-120)
        compare(bridge.commands, ["vol-up", "vol-down"])
    }

    // The level is read after each change finishes, not raced against it.
    function test_stepsReadTheLevelAfterwards() {
        ring.hovered = 0
        bridge.reads = []
        ring.scrollAdjuster(120)
        compare(bridge.readsAfterAdjust, ["vol-read"])
        compare(bridge.reads, [])
    }

    function test_scrollOverNormalSlotDoesNothing() {
        ring.hovered = 1
        ring.scrollAdjuster(120)
        compare(bridge.commands, [])
    }

    function test_clickMutesAndKeepsRingOpen() {
        ring.choose(0, -1)
        compare(bridge.commands, ["vol-mute"])
        compare(bridge.chosen, "")
        verify(!ring.closing)
    }
}
