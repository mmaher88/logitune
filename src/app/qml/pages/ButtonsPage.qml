import QtQuick
import QtQuick.Layouts
import Logitune

// ─────────────────────────────────────────────────────────────────────────────
// ButtonsPage — main button-remapping screen (Options+ dark style).
//
// Layout:
//   Centre: mouseContainer (DeviceRender + ButtonCallout cards, move together)
//   Right:  ActionsPanel (slides in when a button is selected)
// ─────────────────────────────────────────────────────────────────────────────
Item {
    id: root

    // Currently selected button (-1 = none)
    property int selectedButton: -1

    // Close side panel when profile tab changes
    Connections {
        target: DeviceModel
        function onActiveProfileNameChanged() {
            root.selectedButton = -1
        }
    }

    // ── Callout layout data — read from device descriptor ────────────────
    // Each entry has: buttonId, hotspotXPct, hotspotYPct, side, labelOffsetYPct,
    //                  actionDefault, buttonLabel, configurable
    readonly property var calloutData: DeviceModel.buttonHotspots

    // ── Background (follows system theme) ────────────────────────────────────
    Rectangle {
        anchors.fill: parent
        color: Theme.background
    }

    // ── Dismiss panel by clicking the background (only covers render area, not panel) ──
    MouseArea {
        anchors {
            left: parent.left
            top: parent.top
            bottom: parent.bottom
            right: actionsPanel.left
        }
        enabled: root.selectedButton >= 0
        onClicked: root.selectedButton = -1
    }

    // ── Centre area (fills space left of actions panel) ──────────────────────
    Item {
        id: renderArea
        anchors {
            left:   parent.left
            top:    parent.top
            bottom: parent.bottom
            right:  actionsPanel.left
        }

        // ── Mouse + callouts container — moves together when panel opens ─────
        Item {
            id: mouseContainer

            width:  deviceRender.implicitWidth + 460   // extra space for callouts
            height: deviceRender.implicitHeight

            anchors.verticalCenter: parent.verticalCenter

            // Scale down when available space is tight
            readonly property real fitScale: Math.min(1.0, Math.max(0.55, renderArea.width / width))
            scale: fitScale
            transformOrigin: Item.Center

            Behavior on scale {
                NumberAnimation { duration: 300; easing.type: Easing.InOutCubic }
            }

            // Centre horizontally; shift left when panel opens
            x: (parent.width - width) / 2 + (root.selectedButton >= 0 ? -60 : 0)

            Behavior on x {
                NumberAnimation { duration: 300; easing.type: Easing.InOutCubic }
            }

            // DeviceRender — side view for buttons page
            DeviceRender {
                id: deviceRender
                anchors.centerIn: parent
                implicitWidth:  280
                implicitHeight: 414
                imageSource: DeviceModel.sideImage

                onButtonClicked: function(buttonId) {
                    selectButton(buttonId)
                }
            }

            // ── Callout cards (children of mouseContainer) ───────────────────
            Repeater {
                model: root.calloutData.length

                ButtonCallout {
                    required property int modelData

                    readonly property var cdata: root.calloutData[modelData]
                    readonly property int btnId: cdata.buttonId

                    // Hotspot position in mouseContainer coordinates (using painted rect)
                    readonly property real hotX: deviceRender.x + deviceRender.paintedX + cdata.hotspotXPct * deviceRender.paintedW
                    readonly property real hotY: deviceRender.y + deviceRender.paintedY + cdata.hotspotYPct * deviceRender.paintedH

                    // Label offset (some labels need to shift to avoid overlap)
                    readonly property real labelOffY: (cdata.labelOffsetYPct || 0) * deviceRender.paintedH

                    // Position: left-side labels to the left, right-side to the right
                    x: cdata.side === "left"
                       ? hotX - width - 24
                       : hotX + 24
                    y: hotY - height / 2 + labelOffY

                    // Connector line endpoint (the hotspot dot)
                    lineToX: hotX
                    lineToY: hotY
                    lineSide: cdata.side

                    actionName: {
                        var an = ButtonModel.actionNameForButton(btnId)
                        return an.length > 0 ? an : cdata.actionDefault
                    }
                    buttonName: cdata.buttonLabel
                    selected:   root.selectedButton === btnId

                    onClicked: selectButton(btnId)

                    Connections {
                        target: ButtonModel
                        function onDataChanged() {
                            var an = ButtonModel.actionNameForButton(btnId)
                            actionName = an.length > 0 ? an : cdata.actionDefault
                        }
                    }
                }
            }
        }
    }

    // ── Actions Panel ────────────────────────────────────────────────────────
    ActionsPanel {
        id: actionsPanel

        anchors {
            top:    parent.top
            bottom: parent.bottom
        }

        // Slide in/out from right
        x: root.selectedButton >= 0
           ? parent.width - width
           : parent.width

        Behavior on x {
            NumberAnimation { duration: 200; easing.type: Easing.OutQuad }
        }

        onClosed: root.selectedButton = -1

        onActionSelected: function(actionName, actionType) {
            if (root.selectedButton >= 0) {
                ButtonModel.setAction(root.selectedButton, actionName, actionType)
            }
        }

        onWheelModeSelected: function(mode) {
            DeviceModel.setThumbWheelMode(mode)
            // Update callout label
            var modeNames = {"scroll": "Horizontal scroll", "zoom": "Zoom in/out", "volume": "Volume control", "none": "No action"}
            ButtonModel.setAction(7, modeNames[mode] || mode, mode === "scroll" ? "default" : "wheel-mode")
        }
    }

    // ── Helper ───────────────────────────────────────────────────────────────
    function selectButton(buttonId) {
        root.selectedButton = buttonId

        // Find the callout data for this button
        var label = ""
        for (var i = 0; i < calloutData.length; i++) {
            if (calloutData[i].buttonId === buttonId) {
                label = calloutData[i].buttonLabel
                break
            }
        }

        var actionName = ButtonModel.actionNameForButton(buttonId)
        actionsPanel.buttonId          = buttonId
        actionsPanel.buttonName        = label
        actionsPanel.currentAction     = actionName
        actionsPanel.currentActionType = ButtonModel.actionTypeForButton(buttonId)
        actionsPanel.isWheel           = ButtonModel.isThumbWheel(buttonId)
    }
}
