import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import Logitune

// Mouse device render — MX Master 3S PNG with image-replacement support.
// Hotspot markers are now owned by HotspotControl (ButtonsPage) or inline (PointScrollPage).
Item {
    id: root

    // Caller sets the render's height budget (in canvas units). Width is
    // derived from the loaded image aspect, so landscape composites (e.g.
    // MX Vertical side stitch) expand horizontally to the full image width
    // instead of being squished into a portrait-sized box, and portrait
    // single-angle shots (e.g. MX Master 3S) keep their existing footprint.
    property real targetHeight: 414

    // Aspect from the loaded image. Fall back to MX Master's portrait ratio
    // until the PNG's sourceSize is known, so the initial layout is stable.
    readonly property real imageAspect: {
        if (mouseImage.sourceSize.width > 0 && mouseImage.sourceSize.height > 0)
            return mouseImage.sourceSize.width / mouseImage.sourceSize.height
        return 0.676
    }

    implicitHeight: targetHeight
    implicitWidth:  targetHeight * imageAspect

    // Allow parent to override the image source per page
    property string imageSource: "qrc:/Logitune/qml/assets/mx-master-3s.png"
    // Role used for EditorModel.replaceImage — matches the "images" key in the descriptor JSON
    property string imageRole: "side"

    // Painted-rect properties — actual rendered area after PreserveAspectFit
    readonly property real paintedX: mouseImage.paintedWidth > 0 ? (width - mouseImage.paintedWidth) / 2 : 0
    readonly property real paintedY: mouseImage.paintedHeight > 0 ? (height - mouseImage.paintedHeight) / 2 : 0
    readonly property real paintedW: mouseImage.paintedWidth > 0 ? mouseImage.paintedWidth : width
    readonly property real paintedH: mouseImage.paintedHeight > 0 ? mouseImage.paintedHeight : height

    Image {
        id: mouseImage
        anchors.fill: parent
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

    Rectangle {
        id: replaceDeviceImageButton
        visible: typeof EditorModel !== 'undefined' && EditorModel.editing
        anchors {
            top: mouseImage.top
            right: mouseImage.right
            margins: 4
        }
        width: 32; height: 28
        radius: 4
        color: replaceHover.hovered ? Theme.hoverBg : Theme.inputBg
        Behavior on color { ColorAnimation { duration: 150 } }

        Text {
            anchors.centerIn: parent
            text: "\uD83D\uDDBC"
            font.pixelSize: 16
            color: Theme.text
        }

        HoverHandler { id: replaceHover }
        MouseArea {
            anchors.fill: parent
            cursorShape: Qt.PointingHandCursor
            onClicked: deviceImageDialog.open()
            hoverEnabled: true
            ToolTip.visible: replaceHover.hovered
            ToolTip.text: "Replace image"
            ToolTip.delay: 500
        }
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

}
