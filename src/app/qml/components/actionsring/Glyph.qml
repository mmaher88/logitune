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
        visible: !themeIcon.visible || themeIcon.status === Image.Error
        color: glyph.color
        font.family: "JetBrainsMono Nerd Font"
        font.pixelSize: glyph.pixelSize
        // tightBoundingRect is measured from the baseline origin; the Text item's baseline is at its ascent.
        x: (glyph.width - ink.tightBoundingRect.width) / 2 - ink.tightBoundingRect.x
        y: (glyph.height - ink.tightBoundingRect.height) / 2 - ink.tightBoundingRect.y - fontMetrics.ascent
    }

    Image {
        id: themeIcon
        anchors.centerIn: parent
        width: glyph.pixelSize
        height: glyph.pixelSize
        visible: /^[a-zA-Z][a-zA-Z0-9-]+$/.test(glyph.icon)
        source: visible ? "image://icon/" + glyph.icon : ""
        sourceSize: Qt.size(width, height)
    }
}
