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

    function test_closeAlwaysAccepted() {
        var main = createTemporaryObject(mainComponent, null)
        verify(main !== null)
        compare(main.close(), true)
        compare(main.visible, false)
    }
}
