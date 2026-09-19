import QtQuick
import QtQuick.Window

Image {
    id: root
    property string action: "default"
    readonly property string glyph: {
        var names = {
            "keystroke:Ctrl+C": "copy", "keystroke:Ctrl+V": "paste",
            "keystroke:Ctrl+X": "cut", "keystroke:Ctrl+Z": "undo",
            "keystroke:Ctrl+Shift+Z": "redo", "keystroke:Ctrl+Y": "redo",
            "keystroke:Play": "play", "keystroke:Next": "next",
            "keystroke:Previous": "previous", "keystroke:Stop": "stop",
            "keystroke:VolumeUp": "volume", "keystroke:VolumeDown": "volume-down",
            "keystroke:Mute": "mute", "keystroke:Alt+Left": "back",
            "keystroke:Alt+Right": "forward", "preset:screenshot": "screenshot",
            "preset:show-desktop": "desktop", "preset:task-switcher": "windows",
            "preset:close-window": "close", "preset:calculator": "calculator",
            "preset:switch-desktop-left": "back", "preset:switch-desktop-right": "forward",
            "keystroke:BrightnessUp": "brightness", "keystroke:BrightnessDown": "brightness",
            "dpi-cycle": "dpi", "smartshift-toggle": "wheel", "default": "none"
        }
        var key = action.replace(/^media:/, "keystroke:")
        if (names[key]) return names[key]
        if (key.indexOf("keystroke:") === 0) return "keyboard"
        if (key.indexOf("app-launch:") === 0) return "launch"
        return "action"
    }
    source: "qrc:/Logitune/qml/assets/actions/" + glyph + ".svg"
    sourceSize.width: width * Screen.devicePixelRatio
    sourceSize.height: height * Screen.devicePixelRatio
    fillMode: Image.PreserveAspectFit
    smooth: true
    asynchronous: false
}
