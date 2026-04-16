import QtQuick
import QtQuick.Controls
import Logitune

ApplicationWindow {
    id: root
    width: 960; height: 640
    visible: true
    title: "Logitune"
    color: Theme.background
    minimumWidth: 960; minimumHeight: 680
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
