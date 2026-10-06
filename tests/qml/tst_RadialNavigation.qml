import QtQuick
import QtQuick.Controls
import QtTest
import Logitune

TestCase {
    name: "RadialNavigation"
    when: windowShown
    width: 1000; height: 720
    ApplicationWindow {
        id: host
        width: 1000; height: 720; visible: true
        StackView {
            id: stack
            anchors.fill: parent
            initialItem: homeComponent
        }
    }
    Component {
        id: homeComponent
        HomeView { onRadialMenuClicked: stack.push(settingsComponent) }
    }
    Component { id: settingsComponent; RadialMenuSettingsView {} }
    Component {
        id: panelComponent
        ActionsPanel { height: 640; currentAction: "Radial menu"; currentActionType: "radial-menu" }
    }
    SignalSpy { id: navigationSpy; signalName: "radialMenuRequested" }
    function test_homeEntryOpensSeparateSettingsWithoutDevice() {
        var entry = findChild(stack.currentItem, "openActionRing")
        verify(entry !== null)
        mouseClick(entry)
        tryCompare(stack, "depth", 2)
        tryCompare(stack.currentItem, "objectName", "actionRingSettings")
        compare(stack.currentItem.hasDevice, false)
        var editor = findChild(stack.currentItem, "actionRingEditor")
        verify(editor !== null)
        compare(editor.enabled, false)
        stack.pop(null, StackView.Immediate)
    }
    function test_buttonAssignmentKeepsConfigurationLink() {
        var panel = createTemporaryObject(panelComponent, host.contentItem)
        verify(panel !== null)
        navigationSpy.target = panel
        navigationSpy.clear()
        var link = findChild(panel, "configureActionRing")
        verify(link !== null)
        verify(findChild(panel, "editDialog") === null)
        mouseClick(link)
        compare(navigationSpy.count, 1)
    }
}
