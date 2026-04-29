import QtQuick
import QtQuick.Controls
import QtQuick.Window
import Logitune

ApplicationWindow {
    id: root

    // ── Window geometry ─────────────────────────────────────────────────
    // Design minimum keeps the Buttons page sane: the right action
    // panel needs ~360px wide plus the main area needs ~560px for the
    // mouse image and callouts. Vertically we need title bar + tabs +
    // app bar + (action picker 240 + gesture sub-form 200 minimum) so
    // ~640px is the floor below which the gesture config starts
    // clipping its scroll area.
    //
    // We clamp the minimum to the current screen so the window can
    // never be forced larger than the available desktop area (small
    // laptops, scaled-down virtual screens). Initial size is ~70% of
    // the available area, bounded by the design minimum and a
    // comfortable cap so we don't spawn huge on 4K.
    // Comfortable starting target (capped so we don't spawn huge on
    // 4K displays). Scaled to ~70% of the available desktop with the
    // cap as the upper bound and a sane initial floor.
    readonly property int _startCapWidth:    1280
    readonly property int _startCapHeight:   900
    readonly property int _startFloorWidth:  1080
    readonly property int _startFloorHeight: 720

    // Hard minimum the user can shrink to. Kept small so snap-tiling
    // (snap-to-half / snap-to-quarter / window-tile-extension) works
    // — most compositors place tiles around 640x480 minimum. The
    // panel's Flickables handle small heights by scrolling, so the
    // app stays usable down to this floor.
    readonly property int _hardMinWidth:  640
    readonly property int _hardMinHeight: 480

    // Available screen size (excludes taskbar / top panel). Falls
    // back to the cap when the attached Screen isn't ready.
    readonly property int _availW: Screen.desktopAvailableWidth  > 0
                                       ? Screen.desktopAvailableWidth
                                       : _startCapWidth
    readonly property int _availH: Screen.desktopAvailableHeight > 0
                                       ? Screen.desktopAvailableHeight
                                       : _startCapHeight

    // Starting size: 70% of available, bounded by floor + cap, then
    // clamped to fit the screen on small displays.
    readonly property int _startWidth:
        Math.min(_availW,
                 Math.max(Math.min(_startFloorWidth, _availW),
                          Math.min(_startCapWidth, Math.round(_availW * 0.7))))
    readonly property int _startHeight:
        Math.min(_availH,
                 Math.max(Math.min(_startFloorHeight, _availH),
                          Math.min(_startCapHeight, Math.round(_availH * 0.7))))

    // Hard min also clamped to fit small screens, so we never refuse
    // to shrink to whatever the desktop area allows.
    minimumWidth:  Math.min(_hardMinWidth,  _availW)
    minimumHeight: Math.min(_hardMinHeight, _availH)
    width:         _startWidth
    height:        _startHeight

    // Belt and suspenders: some compositors don't honor min-size
    // hints for frameless windows during interactive resize. The
    // hard minimum here is small enough that snap-tiling (half /
    // quarter / etc.) works without the clamp fighting the WM.
    onWidthChanged:  if (width  < minimumWidth)  width  = minimumWidth
    onHeightChanged: if (height < minimumHeight) height = minimumHeight

    visible: true
    title: "Logitune"
    color: Theme.background
    flags: Qt.FramelessWindowHint | Qt.Window

    onClosing: function(close) {
        close.accepted = false
        hide()
    }

    // ── Custom title bar ────────────────────────────────────────────────
    Rectangle {
        id: titleBar
        anchors { top: parent.top; left: parent.left; right: parent.right }
        height: 36
        color: Theme.background
        z: 200

        // Drag to move window (works on both X11 and Wayland)
        TapHandler {
            onTapped: if (tapCount === 2) {
                if (root.visibility === Window.Maximized)
                    root.showNormal()
                else
                    root.showMaximized()
            }
            gesturePolicy: TapHandler.DragThreshold
        }
        DragHandler {
            grabPermissions: TapHandler.CanTakeOverFromAnything
            onActiveChanged: if (active) root.startSystemMove()
        }

        // App title
        Text {
            anchors { left: parent.left; leftMargin: 16; verticalCenter: parent.verticalCenter }
            text: "Logitune"
            font.pixelSize: 12
            color: Theme.textSecondary
        }

        // Window buttons (right side)
        Row {
            anchors { right: parent.right; rightMargin: 8; verticalCenter: parent.verticalCenter }
            spacing: 0

            // Minimize
            Rectangle {
                width: 36; height: 28; radius: 4
                color: minimizeHover.hovered ? Theme.hoverBg : "transparent"
                HoverHandler { id: minimizeHover }
                Text {
                    anchors.centerIn: parent
                    text: "─"
                    font.pixelSize: 11
                    color: Theme.textSecondary
                }
                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.showMinimized()
                }
            }

            // Maximize/Restore
            Rectangle {
                width: 36; height: 28; radius: 4
                color: maximizeHover.hovered ? Theme.hoverBg : "transparent"
                HoverHandler { id: maximizeHover }
                Text {
                    anchors.centerIn: parent
                    text: root.visibility === Window.Maximized ? "❐" : "□"
                    font.pixelSize: 11
                    color: Theme.textSecondary
                }
                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        if (root.visibility === Window.Maximized)
                            root.showNormal()
                        else
                            root.showMaximized()
                    }
                }
            }

            // Close
            Rectangle {
                width: 36; height: 28; radius: 4
                color: closeHover.hovered ? "#cc3333" : "transparent"
                HoverHandler { id: closeHover }
                Text {
                    anchors.centerIn: parent
                    text: "✕"
                    font.pixelSize: 11
                    color: closeHover.hovered ? "#ffffff" : Theme.textSecondary
                }
                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.close()
                }
            }
        }

        // Bottom border
        Rectangle {
            anchors { bottom: parent.bottom; left: parent.left; right: parent.right }
            height: 1
            color: Theme.border
        }
    }

    // ── Resize handles (work on both X11 and Wayland) ─────────────────
    // Right edge
    Item {
        anchors { right: parent.right; top: titleBar.bottom; bottom: parent.bottom }
        width: 4; z: 201
        HoverHandler { cursorShape: Qt.SizeHorCursor }
        DragHandler {
            grabPermissions: DragHandler.CanTakeOverFromAnything
            onActiveChanged: if (active) root.startSystemResize(Qt.RightEdge)
        }
    }
    // Bottom edge
    Item {
        anchors { bottom: parent.bottom; left: parent.left; right: parent.right }
        height: 4; z: 201
        HoverHandler { cursorShape: Qt.SizeVerCursor }
        DragHandler {
            grabPermissions: DragHandler.CanTakeOverFromAnything
            onActiveChanged: if (active) root.startSystemResize(Qt.BottomEdge)
        }
    }
    // Bottom-right corner
    Item {
        anchors { right: parent.right; bottom: parent.bottom }
        width: 8; height: 8; z: 202
        HoverHandler { cursorShape: Qt.SizeFDiagCursor }
        DragHandler {
            grabPermissions: DragHandler.CanTakeOverFromAnything
            onActiveChanged: if (active) root.startSystemResize(Qt.RightEdge | Qt.BottomEdge)
        }
    }
    // Left edge
    Item {
        anchors { left: parent.left; top: titleBar.bottom; bottom: parent.bottom }
        width: 4; z: 201
        HoverHandler { cursorShape: Qt.SizeHorCursor }
        DragHandler {
            grabPermissions: DragHandler.CanTakeOverFromAnything
            onActiveChanged: if (active) root.startSystemResize(Qt.LeftEdge)
        }
    }
    // Bottom-left corner
    Item {
        anchors { left: parent.left; bottom: parent.bottom }
        width: 8; height: 8; z: 202
        HoverHandler { cursorShape: Qt.SizeBDiagCursor }
        DragHandler {
            grabPermissions: DragHandler.CanTakeOverFromAnything
            onActiveChanged: if (active) root.startSystemResize(Qt.LeftEdge | Qt.BottomEdge)
        }
    }

    // ── Content below title bar ─────────────────────────────────────────
    EditorToolbar {
        id: editorToolbar
        anchors { top: titleBar.bottom; left: parent.left; right: parent.right }
        onDevicePage: mainStack.depth > 1
    }

    ConflictBanner {
        id: conflictBanner
        anchors { top: editorToolbar.bottom; left: parent.left; right: parent.right }
        onViewDiffRequested: function(path) {
            diffModal.open(path)
        }
    }

    DiffModal {
        id: diffModal
    }

    StackView {
        id: mainStack
        anchors { top: conflictBanner.bottom; left: parent.left; right: parent.right; bottom: parent.bottom }
        initialItem: homeViewComponent
    }

    Component {
        id: homeViewComponent
        HomeView {
            onDeviceClicked: mainStack.push(deviceViewComponent)
            onSettingsClicked: mainStack.push(appSettingsComponent)
        }
    }
    Component { id: deviceViewComponent; DeviceView {} }
    Component { id: appSettingsComponent; AppSettingsView {} }

    // Permission error overlay
    Rectangle {
        id: permissionError
        anchors.fill: parent
        color: Theme.background
        visible: false
        z: 100

        Column {
            anchors.centerIn: parent
            spacing: 16
            width: 400

            Text {
                text: "Permission Required"
                font { pixelSize: 24; bold: true }
                color: Theme.text
                anchors.horizontalCenter: parent.horizontalCenter
            }
            Text {
                text: "Logitune needs permission to access your mouse.\n\nPlease log out and back in for udev rules to take effect."
                font.pixelSize: 14
                color: Theme.textSecondary
                wrapMode: Text.WordWrap
                width: parent.width
                horizontalAlignment: Text.AlignHCenter
                lineHeight: 1.4
            }
        }
    }

    Toast { id: appToast }
}
