// GNOME Shell Extension for Logitune — GNOME 45+ (ES modules API)
import Gio from 'gi://Gio';
import GLib from 'gi://GLib';
import Shell from 'gi://Shell';
import { Extension } from 'resource:///org/gnome/shell/extensions/extension.js';

export default class LogituneFocusExtension extends Extension {
    _focusHandler = null;

    _reportFocus() {
        let win = global.display.focus_window;
        if (!win) return;

        let appId = win.get_sandboxed_app_id() || '';
        if (!appId) {
            let app = Shell.WindowTracker.get_default().get_window_app(win);
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

    enable() {
        this._focusHandler = global.display.connect(
            'notify::focus-window', () => this._reportFocus());
        this._reportFocus();
    }

    disable() {
        if (this._focusHandler) {
            global.display.disconnect(this._focusHandler);
            this._focusHandler = null;
        }
    }
}
