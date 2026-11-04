import QtQuick
import QtTest
import "../../src/app/qml/components/actionsring"

TestCase {
    name: "RingEditorState"
    Component { id: factory; RingEditorState {} }
    property var state
    function init() {
        state = createTemporaryObject(factory, this)
        state.loadProfile("global", Array.from({length: 8}, () => ({name: "", icon: "", action: "default"})), 100)
    }
    function test_assignmentAndHistory() {
        state.select(2, -1)
        state.assign({name: "Copy", icon: "C", action: "keystroke:Ctrl+C"})
        compare(state.entries[2].name, "Copy")
        verify(state.canUndo)
        state.undo()
        compare(state.entries[2].action, "default")
        state.redo()
        compare(state.entries[2].action, "keystroke:Ctrl+C")
    }
    function test_customCommandRunsThroughShell() {
        state.select(4, -1)
        state.beginParameter({name: "Custom command", kind: "command", icon: ""})
        verify(state.saveParameter('grim -g "$(slurp)" - | wl-copy', "Screenshot", "󰹑"))
        compare(state.entries[4].name, "Screenshot")
        compare(state.entries[4].icon, "󰹑")
        // Quotes inside the command become triple quotes so QProcess::splitCommand keeps them.
        compare(state.entries[4].action, 'app-launch:sh -c "grim -g """$(slurp)""" - | wl-copy"')
    }
    function test_urlWithoutSchemeGetsHttps() {
        state.select(5, -1)
        state.beginParameter({name: "Open URL", kind: "url", icon: ""})
        verify(state.saveParameter("claude.ai/new", "claude.ai/new", ""))
        compare(state.entries[5].action, 'app-launch:xdg-open "https://claude.ai/new"')

        state.select(6, -1)
        state.beginParameter({name: "Open URL", kind: "url", icon: ""})
        verify(state.saveParameter("http://localhost:8080", "local", ""))
        compare(state.entries[6].action, 'app-launch:xdg-open "http://localhost:8080"')
    }
    function test_dropAndSwap() {
        state.drop({entry: {name: "Paste", action: "keystroke:Ctrl+V"}}, 1, -1)
        state.drop({slot: 1, child: -1}, 3, -1)
        compare(state.entries[3].name, "Paste")
        compare(state.entries[1].action, "default")
    }
    function test_submenuAndChildLimit() {
        state.select(0, -1)
        state.assign({name: "Menu", children: []})
        state.drop({entry: {name: "Copy", action: "keystroke:Ctrl+C"}}, 0, 0)
        compare(state.entries[0].children.length, 1)
        compare(state.entries[0].children[0].name, "Copy")
        for (let i = 1; i < 9; ++i)
            state.drop({entry: {name: "Item", action: "media:Play"}}, 0, i)
        compare(state.entries[0].children.length, 8)
        verify(!state.drop({entry: {name: "Nested", children: []}}, 0, 0))
    }
    function test_profileHistoryIsIsolated() {
        state.assign({name: "A", action: "keystroke:A"})
        let a = state.entries
        state.loadProfile("app", Array.from({length: 8}, () => ({action: "default"})), 80)
        verify(!state.canUndo)
        state.assign({name: "B", action: "keystroke:B"})
        state.loadProfile("global", a, 100)
        state.undo()
        compare(state.entries[0].action, "default")
    }
    function test_parameterCancelAndSave() {
        state.beginParameter({name: "Shortcut", kind: "keystroke", icon: "K"})
        state.cancelParameter()
        verify(!state.canUndo)
        state.beginParameter({name: "Shortcut", kind: "keystroke", icon: "K"})
        state.saveParameter("Ctrl+Alt+T", "Terminal", "T")
        compare(state.entries[0].action, "keystroke:Ctrl+Alt+T")
        compare(state.entries[0].name, "Terminal")
    }
    function test_sizeUndoAndRedoInvalidation() {
        state.changeSize(150)
        state.undo()
        compare(state.size, 100)
        state.assign({name: "New", action: "keystroke:N"})
        verify(!state.canRedo)
    }
    function test_openPathQuotedForQProcess() {
        state.beginParameter({name: "File", kind: "file", icon: "F"})
        state.saveParameter('/home/me/a "quote".txt', "File", "F")
        compare(state.entries[0].action, 'app-launch:xdg-open "/home/me/a """quote""".txt"')
    }
}
