// GNOME Shell Extension for Logitune — GNOME 42-44 (imports-based API)
const { Gio, GLib } = imports.gi;

let _focusHandler = null;

function _reportFocus() {
    let win = global.display.focus_window;
    if (!win) return;

    let appId = win.get_sandboxed_app_id() || '';
    if (!appId) {
        let app = imports.gi.Shell.WindowTracker.get_default().get_window_app(win);
        appId = app ? app.get_id().replace(/\.desktop$/, '') : '';
    }
    if (!appId) appId = win.get_wm_class() || '';

    let title = win.get_title() || '';

    try {
        Gio.DBus.session.call(
            'com.logitune.app',
            '/FocusWatcher',
            'com.logitune.FocusWatcher',
            'focusChanged',
            new GLib.Variant('(ss)', [appId, title]),
            null,
            Gio.DBusCallFlags.NO_AUTO_START,
            -1, null, null);
    } catch (e) {
        // Logitune not running — ignore silently
    }
}

function init() {}

function enable() {
    _focusHandler = global.display.connect('notify::focus-window', _reportFocus);
    _reportFocus();
}

function disable() {
    if (_focusHandler) {
        global.display.disconnect(_focusHandler);
        _focusHandler = null;
    }
}
