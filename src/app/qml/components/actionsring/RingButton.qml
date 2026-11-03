import QtQuick

// One round ring button, positioned by its centre relative to the hub.
Rectangle {
    id: button

    property point centre
    property real size: 48
    property string icon
    property real iconSize: 20
    property bool hovered: false
    property bool open: false
    property bool hasSubmenu: false
    property var theme
    property color hoverColor: theme.accent
    property real hoverScale: 1.12
    property real baseScale: 1.0
    // Resting buttons are slightly see-through so the desktop shows faintly behind the ring.
    readonly property color restColor: Qt.rgba(Qt.color(theme.surface).r, Qt.color(theme.surface).g,
                                               Qt.color(theme.surface).b, 0.88)

    x: centre.x - width / 2
    y: centre.y - height / 2
    width: size
    height: size
    radius: size / 2
    color: hovered || open ? hoverColor : restColor
    border.color: theme.border
    border.width: 1
    scale: baseScale * hoverAmount

    property real hoverAmount: hovered ? hoverScale : 1.0
    Behavior on hoverAmount { NumberAnimation { duration: 120; easing.type: Easing.OutCubic } }
    Behavior on color { ColorAnimation { duration: 120 } }

    // The nub: a small bump on the outer edge of a slot that opens a submenu. Same colour as the
    // button so it reads as part of it; the droplet grows out of this spot.
    Rectangle {
        visible: button.hasSubmenu
        z: -1
        width: 12
        height: 12
        radius: 6
        color: button.color
        border.color: button.border.color
        readonly property real outward: Math.atan2(button.centre.y, button.centre.x)
        x: button.width / 2 + (button.width / 2 - 2) * Math.cos(outward) - width / 2
        y: button.height / 2 + (button.height / 2 - 2) * Math.sin(outward) - height / 2
    }

    Glyph {
        icon: button.icon
        pixelSize: button.iconSize
        color: button.hovered || button.open ? button.theme.onAccent : button.theme.text
    }

}
