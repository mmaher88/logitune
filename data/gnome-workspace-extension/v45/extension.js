// GNOME Shell Extension — interactive workspace scrub for Logitune (GNOME 45+)
// Snap threshold is applied by Logitune (C++) before End — End stays arg-less
// so a live Shell session can keep working across updates without a restart.
import Gio from 'gi://Gio';
import * as Main from 'resource:///org/gnome/shell/ui/main.js';
import {Extension} from 'resource:///org/gnome/shell/extensions/extension.js';

const WORKSPACE_IFACE = `
<node>
  <interface name="com.logitune.WorkspaceGesture">
    <method name="BeginWorkspaceGesture"/>
    <method name="UpdateWorkspaceGesture">
      <arg type="i" direction="in" name="delta"/>
    </method>
    <method name="EndWorkspaceGesture"/>
  </interface>
</node>`;

/** Thumb-wheel units that map to one full workspace of progress. */
const UNITS_PER_WORKSPACE = 150;
const SNAP_DURATION_MS = 250;

export default class LogituneWorkspaceExtension extends Extension {
    _dbus = null;
    _wsProgress = 0;
    _wsActive = false;

    _animationController() {
        return Main.wm?._workspaceAnimation ?? null;
    }

    BeginWorkspaceGesture() {
        if (Main.overview.visible)
            return;

        const anim = this._animationController();
        if (!anim)
            return;

        if (anim._switchData && anim._switchData.gestureActivated) {
            for (const group of anim._switchData.monitors)
                group.remove_all_transitions();
        } else if (!anim._switchData) {
            anim._prepareWorkspaceSwitch();
        }

        if (!anim._switchData)
            return;

        const monitorGroup = anim._findMonitorGroup(Main.layoutManager.primaryIndex);
        if (!monitorGroup)
            return;

        anim._switchData.baseMonitorGroup = monitorGroup;
        this._wsProgress = monitorGroup.progress;
        this._wsActive = true;
    }

    UpdateWorkspaceGesture(delta) {
        if (!this._wsActive)
            return;

        const anim = this._animationController();
        if (!anim?._switchData?.baseMonitorGroup)
            return;

        // Direction is applied by Logitune (invert toggle); use delta as-is.
        this._wsProgress += delta / UNITS_PER_WORKSPACE;

        const monitorGroup = anim._switchData.baseMonitorGroup;
        const points = monitorGroup.getSnapPoints();
        if (points.length > 0) {
            const min = Math.min(...points);
            const max = Math.max(...points);
            this._wsProgress = Math.max(min, Math.min(max, this._wsProgress));
        }

        anim._switchWorkspaceUpdate(null, this._wsProgress);
    }

    EndWorkspaceGesture() {
        if (!this._wsActive)
            return;

        this._wsActive = false;
        const anim = this._animationController();
        if (!anim?._switchData)
            return;

        // Closest snap — Logitune pads progress to the threshold target first.
        anim._switchWorkspaceEnd(null, SNAP_DURATION_MS, this._wsProgress);
    }

    enable() {
        if (this._dbus)
            return;
        try {
            this._dbus = Gio.DBusExportedObject.wrapJSObject(WORKSPACE_IFACE, this);
            this._dbus.export(Gio.DBus.session, '/com/logitune/WorkspaceGesture');
        } catch (e) {
            console.error(`Logitune workspace D-Bus export failed: ${e}`);
            this._dbus = null;
        }
    }

    disable() {
        if (this._wsActive) {
            const anim = this._animationController();
            if (anim?._switchData && !anim._switchData.gestureActivated)
                anim.cancelSwitchAnimation();
            this._wsActive = false;
        }
        if (this._dbus) {
            try {
                this._dbus.unexport();
            } catch (e) {
                console.error(`Logitune workspace D-Bus unexport failed: ${e}`);
            }
            this._dbus = null;
        }
    }
}
