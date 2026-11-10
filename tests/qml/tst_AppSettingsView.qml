import QtQuick
import QtTest
import Logitune

Item {
    id: container
    width: 960
    height: 900

    AppSettingsView {
        id: settings
        anchors.fill: parent
    }

    SignalSpy {
        id: quitSpy
        signalName: "quitRequested"
    }

    TestCase {
        name: "AppSettingsQuit"
        when: windowShown

        function test_quitButtonRequestsQuit() {
            var button = findChild(settings, "quitButton")
            verify(button !== null, "Settings must offer an explicit Quit control")
            quitSpy.target = settings
            verify(quitSpy.valid)
            mouseClick(button)
            compare(quitSpy.count, 1)
        }

        function test_quitAccessibleAtMinimumWindowHeight() {
            container.height = 480
            var button = findChild(settings, "quitButton")
            verify(button !== null)
            var position = button.mapToItem(container, 0, 0)
            verify(position.y >= 0 && position.y + button.height <= container.height,
                   "Quit must remain reachable in a small window")
            container.height = 900
        }
    }
}
