import QtQuick

// Nerd Font icons sit off-centre in their font box, each by a different amount, so a plain
// anchors.centerIn leaves them visibly misaligned. Centre on the glyph's ink bounds instead.
Item {
    id: glyph

    required property string icon
    property real pixelSize: 20
    property color color: "white"

    anchors.fill: parent

    FontMetrics {
        id: fontMetrics
        font: label.font
    }

    TextMetrics {
        id: ink
        font: label.font
        text: glyph.icon
    }

    Text {
        id: label
        text: themeIcon.visible ? "?" : glyph.icon
        visible: !glyph.isGif && (!themeIcon.visible || themeIcon.status === Image.Error)
        color: glyph.color
        font.family: "JetBrainsMono Nerd Font"
        font.pixelSize: glyph.pixelSize
        // tightBoundingRect is measured from the baseline origin; the Text item's baseline is at its ascent.
        x: (glyph.width - ink.tightBoundingRect.width) / 2 - ink.tightBoundingRect.x
        y: (glyph.height - ink.tightBoundingRect.height) / 2 - ink.tightBoundingRect.y - fontMetrics.ascent
    }

    // An icon is one of three things: a Nerd Font glyph (drawn as text above), a freedesktop theme
    // icon name such as "firefox", or an absolute path to an image file the user picked.
    readonly property bool isImageFile: glyph.icon.startsWith("/") || glyph.icon.startsWith("file:")
    readonly property bool isThemeName: /^[a-zA-Z][a-zA-Z0-9._-]+$/.test(glyph.icon)
    readonly property bool isGif: glyph.isImageFile && /\.gif$/i.test(glyph.icon)
    readonly property string fileUrl: glyph.icon.startsWith("file:") ? glyph.icon : "file://" + glyph.icon

    // A plain Image would only show a GIF's first frame.
    AnimatedImage {
        anchors.centerIn: parent
        width: glyph.pixelSize
        height: glyph.pixelSize
        visible: glyph.isGif
        source: visible ? glyph.fileUrl : ""
        playing: visible
        fillMode: Image.PreserveAspectFit
    }

    Image {
        id: themeIcon
        anchors.centerIn: parent
        width: glyph.pixelSize
        height: glyph.pixelSize
        visible: (glyph.isThemeName || glyph.isImageFile) && !glyph.isGif
        source: {
            if (glyph.isImageFile)
                return glyph.fileUrl
            if (glyph.isThemeName)
                return "image://icon/" + glyph.icon
            return ""
        }
        sourceSize: Qt.size(width, height)
        fillMode: Image.PreserveAspectFit
    }
}
