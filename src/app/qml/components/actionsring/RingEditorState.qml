import QtQuick

QtObject {
    id: root
    property var entries: []
    property int size: 100
    property int selectedSlot: 0
    property int selectedChild: -1
    property string profileKey: ""
    property var histories: ({})
    property var past: []
    property var future: []
    property var pending: null
    readonly property bool canUndo: past.length > 0
    readonly property bool canRedo: future.length > 0
    readonly property var selectedEntry: entryAt(selectedSlot, selectedChild) || ({})
    signal committed(var value, int ringSize)

    function clone(value) { return JSON.parse(JSON.stringify(value)) }
    function snapshot() { return {entries: clone(entries), size: size} }
    function loadProfile(key, value, ringSize) {
        if (profileKey) histories[profileKey] = {past: past, future: future}
        profileKey = key
        entries = clone(value)
        size = ringSize
        let history = histories[key] || {past: [], future: []}
        past = history.past; future = history.future
        select(0, -1)
        pending = null
    }
    function select(slot, child) { selectedSlot = slot; selectedChild = child }
    function entryAt(slot, child) {
        let entry = entries[slot]
        return child < 0 ? entry : (entry && entry.children ? entry.children[child] : null)
    }
    function commit(value, ringSize) {
        if (JSON.stringify(value) === JSON.stringify(entries) && ringSize === size) return
        past = past.concat([snapshot()]); future = []
        entries = value; size = ringSize
        committed(entries, size)
    }
    function put(value, slot, child, entry) {
        if (slot < 0 || slot >= value.length || child >= 8) return false
        if (child < 0) value[slot] = clone(entry)
        else {
            if (entry.children || !value[slot].children || child > value[slot].children.length) return false
            value[slot].children[child] = clone(entry)
        }
        return true
    }
    function assign(entry) {
        let value = clone(entries)
        if (!put(value, selectedSlot, selectedChild, entry)) return false
        commit(value, size)
        return true
    }
    function drop(source, slot, child) {
        if (source.entry) {
            select(slot, child)
            if (source.entry.kind) { beginParameter(source.entry); return true }
            return assign(source.entry)
        }
        if (source.slot === slot && source.child === child) return false
        // A root cannot be swapped into its own children, nor a submenu into any child.
        if (source.slot === slot && (source.child < 0 || child < 0)) return false
        let from = entryAt(source.slot, source.child)
        let to = entryAt(slot, child) || {name: "", icon: "", action: "default"}
        if (!from) return false
        let value = clone(entries)
        if (!put(value, slot, child, from) || !put(value, source.slot, source.child, to)) return false
        commit(value, size); select(slot, child)
        return true
    }
    function clear() {
        let value = clone(entries)
        if (selectedChild >= 0) value[selectedSlot].children.splice(selectedChild, 1)
        else value[selectedSlot] = {name: "", icon: "", action: "default"}
        commit(value, size); selectedChild = -1
    }
    function rename(name) { let entry = clone(selectedEntry); entry.name = name; assign(entry) }
    function setIcon(icon) { let entry = clone(selectedEntry); entry.icon = icon; assign(entry) }
    function changeSize(value) { if ([80, 100, 150].indexOf(value) >= 0) commit(clone(entries), value) }
    function undo() {
        if (!canUndo) return
        future = future.concat([snapshot()])
        let value = past[past.length - 1]; past = past.slice(0, -1)
        entries = clone(value.entries); size = value.size; pending = null
        selectedChild = -1; committed(entries, size)
    }
    function redo() {
        if (!canRedo) return
        past = past.concat([snapshot()])
        let value = future[future.length - 1]; future = future.slice(0, -1)
        entries = clone(value.entries); size = value.size; pending = null
        selectedChild = -1; committed(entries, size)
    }
    function beginParameter(action) { pending = {action: clone(action), slot: selectedSlot, child: selectedChild} }
    function cancelParameter() { pending = null }
    function saveParameter(payload, name, icon) {
        if (!pending || !payload.trim()) return false
        let action = pending.action
        let kind = action.kind === "keystroke" ? "keystroke" : "app-launch"
        // xdg-open treats "claude.ai/new" as a file path and silently fails; a URL needs its scheme.
        let hasScheme = /^[a-z][a-z0-9+.-]*:/i.test(payload.trim())
        if (action.kind === "url" && !hasScheme)
            payload = "https://" + payload.trim()
        if (["url", "file", "folder"].indexOf(action.kind) >= 0)
            payload = 'xdg-open "' + payload.replace(/"/g, '"""') + '"'
        // The app-launch executor splits a command into words and runs it without a shell, so pipes,
        // $(...) and ~ would not work. Hand custom commands to sh instead. Triple quotes are how
        // QProcess::splitCommand reads a literal quote inside a quoted argument.
        if (action.kind === "command")
            payload = 'sh -c "' + payload.replace(/"/g, '"""') + '"'
        select(pending.slot, pending.child)
        let saved = assign({name: name || action.name, icon: icon || action.icon, action: kind + ":" + payload})
        if (saved) pending = null
        return saved
    }
}
