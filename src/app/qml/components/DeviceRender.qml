import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import Logitune

// Mouse device render — MX Master 3S PNG with invisible clickable button zones overlaid.
// White hotspot circles on configurable buttons (no left/right click zones shown).
Item {
    id: root

    implicitWidth:  280
    implicitHeight: 414

    signal buttonClicked(int buttonId)

    // Allow parent to override the image source per page
    property string imageSource: "qrc:/Logitune/qml/assets/mx-master-3s.png"
    property bool showHotspots: true  // set false on Point & Scroll page
    // Role used for EditorModel.replaceImage — matches the "images" key in the descriptor JSON
    property string imageRole: "side"

    // Painted-rect properties — actual rendered area after PreserveAspectFit
    readonly property real paintedX: (width - mouseImage.paintedWidth) / 2
    readonly property real paintedY: (height - mouseImage.paintedHeight) / 2
    readonly property real paintedW: mouseImage.paintedWidth
    readonly property real paintedH: mouseImage.paintedHeight

    // ── Half-extents for the invisible hit zone around each hotspot dot ──────
    readonly property real zoneHalfW: 0.11   // fraction of paintedW
    readonly property real zoneHalfH: 0.07   // fraction of paintedH

    Image {
        id: mouseImage
        anchors.centerIn: parent
        width: parent.implicitWidth
        height: parent.implicitHeight
        source: root.imageSource
        fillMode: Image.PreserveAspectFit
        smooth: true
        mipmap: true
    }

    DropArea {
        id: deviceImageDrop
        anchors.fill: mouseImage
        enabled: typeof EditorModel !== 'undefined' && EditorModel.editing
        onDropped: function(drop) {
            if (drop.hasUrls && drop.urls.length > 0) {
                var url = drop.urls[0].toString()
                if (url.toLowerCase().endsWith(".png")) {
                    var path = url.replace(/^file:\/\//, "")
                    EditorModel.replaceImage(root.imageRole, path)
                }
            }
        }
    }

    Button {
        id: replaceDeviceImageButton
        visible: typeof EditorModel !== 'undefined' && EditorModel.editing
        anchors {
            top: mouseImage.top
            right: mouseImage.right
            margins: 4
        }
        text: "Replace image"
        onClicked: deviceImageDialog.open()
    }

    FileDialog {
        id: deviceImageDialog
        nameFilters: ["PNG (*.png)"]
        onAccepted: {
            var url = selectedFile.toString()
            var path = url.replace(/^file:\/\//, "")
            EditorModel.replaceImage(root.imageRole, path)
        }
    }

    Repeater {
        model: root.showHotspots ? DeviceModel.buttonHotspots.length : 0

        Item {
            id: markerItem
            required property int modelData

            readonly property var hp: DeviceModel.buttonHotspots[modelData]

            // Skip non-configurable buttons
            visible: hp.configurable

            // Target dot centre in root coordinates
            readonly property real targetX: root.paintedX + hp.hotspotXPct * root.paintedW
            readonly property real targetY: root.paintedY + hp.hotspotYPct * root.paintedH

            // 24x24 hit area — follows drag in editor mode, otherwise snaps to target.
            width: 24; height: 24
            // Conditional binding: active except while DragHandler is mutating x/y.
            // A self-referencing ternary here would sever the binding after first drag.
            Binding on x {
                value: markerItem.targetX - markerItem.width / 2
                when: !drag.active
            }
            Binding on y {
                value: markerItem.targetY - markerItem.height / 2
                when: !drag.active
            }

            // Invisible click hit zone centred on dot (for button selection, non-edit mode)
            MouseArea {
                anchors.centerIn: parent
                width: root.zoneHalfW * root.paintedW * 2
                height: root.zoneHalfH * root.paintedH * 2
                cursorShape: Qt.PointingHandCursor
                enabled: !drag.enabled
                onClicked: root.buttonClicked(markerItem.hp.buttonId)
            }

            // 18x18 hotspot circle — white for dark background
            Rectangle {
                anchors.centerIn: parent
                width: 18; height: 18
                radius: 9
                color: "transparent"
                border.color: Theme.accent
                border.width: 2
                opacity: 0.7

                Behavior on opacity { NumberAnimation { duration: 200 } }

                // Inner dot
                Rectangle {
                    anchors.centerIn: parent
                    width: 6; height: 6
                    radius: 3
                    color: Theme.accent
                    opacity: 0.6
                }
            }

            // Editor-mode drag handler — disabled (and effectively absent) in production.
            DragHandler {
                id: drag
                enabled: typeof EditorModel !== 'undefined' && EditorModel.editing
                target: parent
                onActiveChanged: {
                    if (!active) {
                        var cx = markerItem.x + markerItem.width / 2
                        var cy = markerItem.y + markerItem.height / 2
                        var xPct = (cx - root.paintedX) / root.paintedW
                        var yPct = (cy - root.paintedY) / root.paintedH
                        xPct = Math.max(0, Math.min(1, xPct))
                        yPct = Math.max(0, Math.min(1, yPct))
                        EditorModel.updateHotspot(markerItem.modelData,
                                                   xPct, yPct,
                                                   markerItem.hp.side,
                                                   markerItem.hp.labelOffsetYPct)
                    }
                }
            }
        }
    }
}
