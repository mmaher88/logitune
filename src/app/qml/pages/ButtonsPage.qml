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

    // ── Callout layout data ────────────────────────────────────────────────
    // Only configurable buttons (no left/right click).
    // hotspotXPct / hotspotYPct: percentage of mouse image where the hotspot dot sits.
    // side: "left" means label goes to the left of the hotspot, "right" to the right.
    // buttonId: maps to ButtonModel index.
    readonly property var calloutData: [
        // Middle button — dot at (71%, 15%), RIGHT side label
        { buttonId: 2, hotspotXPct: 0.71, hotspotYPct: 0.15, side: "right",
          actionDefault: "Middle click", buttonLabel: "Middle button" },
        // Top / ModeShift — dot at (81%, 34%), RIGHT side label
        { buttonId: 6, hotspotXPct: 0.81, hotspotYPct: 0.34, side: "right",
          actionDefault: "Shift wheel mode", buttonLabel: "Top button" },
        // ThumbWheel — dot at (55%, 51.5%), RIGHT side label
        { buttonId: 7, hotspotXPct: 0.55, hotspotYPct: 0.515, side: "right",
          actionDefault: "Horizontal scroll", buttonLabel: "Thumb wheel" },
        // Forward — dot at (35%, 43%), LEFT side label
        { buttonId: 4, hotspotXPct: 0.35, hotspotYPct: 0.43, side: "left",
          actionDefault: "Forward", buttonLabel: "Forward button" },
        // Back — dot stays at (45%, 60%) but label is offset down 20%
        { buttonId: 3, hotspotXPct: 0.45, hotspotYPct: 0.60, side: "left", labelOffsetYPct: 0.20,
          actionDefault: "Back", buttonLabel: "Back button" },
        // Gesture — dot at (8%, 58%), LEFT side label
        { buttonId: 5, hotspotXPct: 0.08, hotspotYPct: 0.58, side: "left",
          actionDefault: "Gestures", buttonLabel: "Virtual desktops" },
    ]

    // ── Background (follows system/light theme) ───────────────────────────────
    Rectangle {
        anchors.fill: parent
        color: "#FFFFFF"
    }

    // ── Dismiss panel by clicking the background ─────────────────────────────
    MouseArea {
        anchors.fill: parent
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

            // Centre horizontally; shift left when panel opens
            x: (parent.width - width) / 2 + (root.selectedButton >= 0 ? -60 : 0)

            Behavior on x {
                NumberAnimation { duration: 300; easing.type: Easing.InOutCubic }
            }

            // DeviceRender centred in mouseContainer — side view for buttons page
            DeviceRender {
                id: deviceRender
                anchors.centerIn: parent
                implicitWidth:  280
                implicitHeight: 414
                imageSource: "qrc:/Logitune/qml/assets/mx-master-3s-side.png"

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

                    // Hotspot position in mouseContainer coordinates
                    readonly property real hotX: deviceRender.x + cdata.hotspotXPct * deviceRender.implicitWidth
                    readonly property real hotY: deviceRender.y + cdata.hotspotYPct * deviceRender.implicitHeight

                    // Label offset (some labels need to shift to avoid overlap)
                    readonly property real labelOffY: (cdata.labelOffsetYPct || 0) * deviceRender.implicitHeight

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
    }
}
