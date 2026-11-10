import QtQuick
import QtQuick.Controls

Item {
    id: root
    property string action: "default"
    property color color: "white"

    readonly property string glyph: {
        var names = {
            "keystroke:Ctrl+C": "copy",
            "keystroke:Ctrl+V": "clipboard-paste",
            "keystroke:Ctrl+X": "scissors",
            "keystroke:Ctrl+Z": "undo-2",
            "keystroke:Ctrl+Shift+Z": "redo-2",
            "keystroke:Ctrl+Y": "redo-2",
            "keystroke:Play": "play",
            "keystroke:Next": "skip-forward",
            "keystroke:Previous": "skip-back",
            "keystroke:VolumeUp": "volume-2",
            "keystroke:VolumeDown": "volume-1",
            "keystroke:Mute": "volume-x",
            "keystroke:Alt+Left": "arrow-left",
            "keystroke:Alt+Right": "arrow-right",
            "keystroke:BrightnessUp": "sun",
            "keystroke:BrightnessDown": "sun",
            "preset:screenshot": "camera",
            "preset:show-desktop": "monitor",
            "preset:switch-desktop-left": "arrow-left",
            "preset:switch-desktop-right": "arrow-right",
            "preset:calculator": "calculator",
            "preset:close-window": "x",
            "dpi-cycle": "gauge",
            "smartshift-toggle": "mouse",
            "default": "circle-off"
        }
        var key = action.replace(/^media:/, "keystroke:")
        if (names[key]) return names[key]
        if (key.indexOf("keystroke:") === 0) return "keyboard"
        if (key.indexOf("app-launch:") === 0) return "terminal"
        return "sparkles"
    }

    ToolButton {
        anchors.fill: parent
        focusPolicy: Qt.NoFocus
        hoverEnabled: false
        background: null
        display: AbstractButton.IconOnly
        icon.source: "qrc:/Logitune/qml/assets/actions/" + root.glyph + ".svg"
        icon.color: root.color
        icon.width: Math.min(width, height)
        icon.height: Math.min(width, height)
    }
}
