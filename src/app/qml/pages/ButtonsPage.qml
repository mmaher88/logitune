import QtQuick
import QtQuick.Layouts
import Logitune

// ─────────────────────────────────────────────────────────────────────────────
// ButtonsPage — main button-remapping screen (Options+ dark style).
//
// Layout:
//   Centre: mouseContainer (DeviceRender + HotspotControl cards, move together)
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

        // Scale math is shared between the device image (scaled) and the
        // callout overlay (unscaled, reads back the scale to position itself).
        // The 460 gutter accounts for callout cards spilling outside the
        // painted image area; landscape composites like MX Vertical rely on
        // the floor so the image does not vanish in a narrow window.
        readonly property real fitScale: Math.min(1.0, Math.max(0.55,
            renderArea.width / (deviceRender.implicitWidth + 460)))
        readonly property real horizontalShift: root.selectedButton >= 0 ? -60 : 0

        // ── Scaled image stage ───────────────────────────────────────────
        Item {
            id: mouseContainer

            width:  deviceRender.implicitWidth
            height: deviceRender.implicitHeight

            anchors.verticalCenter: parent.verticalCenter
            scale: renderArea.fitScale
            transformOrigin: Item.Center

            Behavior on scale {
                NumberAnimation { duration: 300; easing.type: Easing.InOutCubic }
            }

            x: (parent.width - width) / 2 + renderArea.horizontalShift

            Behavior on x {
                NumberAnimation { duration: 300; easing.type: Easing.InOutCubic }
            }

            DeviceRender {
                id: deviceRender
                anchors.centerIn: parent
                targetHeight: 414
                imageSource: DeviceModel.sideImage
            }
        }

        // ── Unscaled callout overlay ─────────────────────────────────────
        // Callouts live outside the scaled container so their text stays
        // legible when the image has to shrink (landscape side composites
        // on narrow windows). Positions project the scaled painted bounds
        // back into renderArea coordinates via mapToItem, which resolves
        // the transform in one step and avoids the chain of derived
        // properties that Qt's binding engine can mis-flag as a loop.
        Item {
            id: calloutLayer
            anchors.fill: parent

            function refreshBounds() {
                var tl = deviceRender.mapToItem(calloutLayer,
                    deviceRender.paintedX, deviceRender.paintedY)
                var br = deviceRender.mapToItem(calloutLayer,
                    deviceRender.paintedX + deviceRender.paintedW,
                    deviceRender.paintedY + deviceRender.paintedH)
                scaledPaintedX = tl.x
                scaledPaintedY = tl.y
                scaledPaintedW = br.x - tl.x
                scaledPaintedH = br.y - tl.y
            }

            property real scaledPaintedX: 0
            property real scaledPaintedY: 0
            property real scaledPaintedW: 0
            property real scaledPaintedH: 0

            Connections {
                target: mouseContainer
                function onXChanged()      { calloutLayer.refreshBounds() }
                function onYChanged()      { calloutLayer.refreshBounds() }
                function onScaleChanged()  { calloutLayer.refreshBounds() }
                function onWidthChanged()  { calloutLayer.refreshBounds() }
                function onHeightChanged() { calloutLayer.refreshBounds() }
            }
            Connections {
                target: deviceRender
                function onPaintedXChanged() { calloutLayer.refreshBounds() }
                function onPaintedYChanged() { calloutLayer.refreshBounds() }
                function onPaintedWChanged() { calloutLayer.refreshBounds() }
                function onPaintedHChanged() { calloutLayer.refreshBounds() }
            }
            Component.onCompleted: refreshBounds()

            Repeater {
                model: root.calloutData.length

                HotspotControl {
                    required property int modelData

                    readonly property var cdata: root.calloutData[modelData]
                    readonly property int btnId: cdata.buttonId

                    anchors.fill: parent

                    imageX: calloutLayer.scaledPaintedX
                    imageY: calloutLayer.scaledPaintedY
                    imageW: calloutLayer.scaledPaintedW
                    imageH: calloutLayer.scaledPaintedH

                    hotspotXPct: cdata.hotspotXPct
                    hotspotYPct: cdata.hotspotYPct
                    side: cdata.side
                    labelOffsetYPct: cdata.labelOffsetYPct || 0
                    configurable: cdata.configurable

                    buttonName: cdata.buttonLabel
                    actionName: {
                        var an = ButtonModel.actionNameForButton(btnId)
                        return an.length > 0 ? an : cdata.actionDefault
                    }
                    selected: root.selectedButton === btnId
                    buttonId: btnId
                    hotspotIndex: modelData

                    pageWidth: renderArea.width
                    pageHeight: renderArea.height

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
