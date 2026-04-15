import QtQuick
import QtTest
import Logitune

Item {
    width: 600; height: 100

    EditorToolbar {
        id: bar
        width: parent.width
    }

    TestCase {
        name: "EditorToolbar"
        when: windowShown

        function test_invisibleWhenEditorModelNotRegistered() {
            compare(bar.visible, false)
            compare(bar.height, 0)
        }
    }
}
