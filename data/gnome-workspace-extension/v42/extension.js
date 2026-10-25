// GNOME Shell Extension — interactive workspace scrub for Logitune (GNOME 42-44)
// Snap threshold is applied by Logitune (C++) before End.
const {Gio} = imports.gi;
const Main = imports.ui.main;

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

const UNITS_PER_WORKSPACE = 150;
const SNAP_DURATION_MS = 250;

let _dbus = null;
let _wsProgress = 0;
let _wsActive = false;

function _animationController() {
    return Main.wm && Main.wm._workspaceAnimation ? Main.wm._workspaceAnimation : null;
}

const WorkspaceGesture = {
    BeginWorkspaceGesture() {
        if (Main.overview.visible)
            return;

        const anim = _animationController();
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
        _wsProgress = monitorGroup.progress;
        _wsActive = true;
    },

    UpdateWorkspaceGesture(delta) {
        if (!_wsActive)
            return;

        const anim = _animationController();
        if (!anim || !anim._switchData || !anim._switchData.baseMonitorGroup)
            return;

        _wsProgress += delta / UNITS_PER_WORKSPACE;

        const monitorGroup = anim._switchData.baseMonitorGroup;
        const points = monitorGroup.getSnapPoints();
        if (points.length > 0) {
            const min = Math.min.apply(null, points);
            const max = Math.max.apply(null, points);
            _wsProgress = Math.max(min, Math.min(max, _wsProgress));
        }

        anim._switchWorkspaceUpdate(null, _wsProgress);
    },

    EndWorkspaceGesture() {
        if (!_wsActive)
            return;

        _wsActive = false;
        const anim = _animationController();
        if (!anim || !anim._switchData)
            return;

        anim._switchWorkspaceEnd(null, SNAP_DURATION_MS, _wsProgress);
    },
};

function init() {}

function enable() {
    if (_dbus)
        return;
    try {
        _dbus = Gio.DBusExportedObject.wrapJSObject(WORKSPACE_IFACE, WorkspaceGesture);
        _dbus.export(Gio.DBus.session, '/com/logitune/WorkspaceGesture');
    } catch (e) {
        log(`Logitune workspace D-Bus export failed: ${e}`);
        _dbus = null;
    }
}

function disable() {
    if (_wsActive) {
        const anim = _animationController();
        if (anim && anim._switchData && !anim._switchData.gestureActivated)
            anim.cancelSwitchAnimation();
        _wsActive = false;
    }
    if (_dbus) {
        try {
            _dbus.unexport();
        } catch (e) {
            log(`Logitune workspace D-Bus unexport failed: ${e}`);
        }
        _dbus = null;
    }
}
