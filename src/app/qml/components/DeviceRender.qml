import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import Logitune

// Mouse device render — MX Master 3S PNG with image-replacement support.
// Hotspot markers are now owned by HotspotControl (ButtonsPage) or inline (PointScrollPage).
Item {
    id: root

    implicitWidth:  280
    implicitHeight: 414

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
