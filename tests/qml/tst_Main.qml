import QtQuick
import QtTest
import Logitune

TestCase {
    name: "MainClosing"
    when: windowShown

    Component {
        id: mainComponent
        Main {}
    }

    function test_close_data() {
        return [
            { tag: "withoutTray", tray: false, accepted: true },
            { tag: "withTray", tray: true, accepted: false }
        ]
    }

    function test_close(data) {
        TestSetup.setTrayAvailable(data.tray)
        var main = createTemporaryObject(mainComponent, null)
        verify(main !== null)
        compare(main.close(), data.accepted)
        compare(main.visible, false)
    }
}
