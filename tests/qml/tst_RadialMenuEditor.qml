import QtQuick
import QtTest
import Logitune

TestCase {
    name: "RadialMenuEditor"
    when: windowShown

    Component {
        id: iconComponent
        RadialActionIcon { width: 24; height: 24 }
    }

    Component {
        id: ringComponent
        RadialMenuRing {
            width: 400
            height: 400
            editing: true
            entries: [
                {label: "Copy", action: "keystroke:Ctrl+C"},
                {label: "Paste", action: "keystroke:Ctrl+V"}
            ]
        }
    }

    Component {
        id: editorComponent
        RadialMenuEditor { width: 420; height: 600 }
    }

    function test_iconMapping() {
        var icon = createTemporaryObject(iconComponent, this,
                                         {action: "keystroke:Ctrl+C"})
        verify(icon)
        compare(icon.glyph, "copy")
        icon.action = "app-launch:konsole"
        compare(icon.glyph, "terminal")
        icon.action = "default"
        compare(icon.glyph, "circle-off")
    }

    function test_editorAndPreviewInstantiate() {
        var ring = createTemporaryObject(ringComponent, this)
        verify(ring)
        compare(ring.entries.length, 2)

        var editor = createTemporaryObject(editorComponent, this)
        verify(editor)
        compare(editor.error, "")
        verify(DeviceModel.radialItems.length >= 2)
    }
}
