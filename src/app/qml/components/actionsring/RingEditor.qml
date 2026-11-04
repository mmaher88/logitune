import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Logitune

Item {
    id: root
    property alias editorState: state
    readonly property var entries: state.entries
    property alias selectedSlot: state.selectedSlot
    signal backRequested()

    function save(index, entry) { state.select(index, -1); state.assign(entry) }
    function chooseAction(entry) {
        if (entry.kind) state.beginParameter(entry)
        else state.assign(entry)
    }
    function reloadProfile() {
        let key = ProfileModel.wmClassAt(ProfileModel.displayIndex) || "default"
        state.loadProfile(key, ActionsRing.entries, ActionsRing.size)
    }
    Component.onCompleted: reloadProfile()
    Connections {
        target: ProfileModel
        // Backend profile switching completes synchronously before the next event turn.
        function onDisplayIndexChanged() { Qt.callLater(root.reloadProfile) }
        function onProfileRemoved(wmClass) { delete state.histories[wmClass] }
    }
    RingEditorState {
        id: state
        onCommitted: (value, ringSize) => {
            ActionsRing.setEntries(value)
            ActionsRing.setSize(ringSize)
        }
    }
    ColumnLayout {
        anchors.fill: parent; anchors.margins: 20; spacing: 16
        RowLayout {
            Layout.fillWidth: true
            RingFlatButton { text: "← Back"; onClicked: root.backRequested() }
            AppProfileBar { Layout.fillWidth: true; Layout.preferredHeight: 48; compact: true }
            Item { Layout.preferredWidth: 80 }
        }
        Label { text: "Actions Ring"; color: Theme.text; font { pixelSize: 24; bold: true } }
        RowLayout {
            Layout.fillWidth: true; Layout.fillHeight: true; spacing: 20
            // Fills the space so the ring's hub sits in the middle of the visible area.
            RingPreview {
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                editor: state
            }
            Rectangle { Layout.fillHeight: true; width: 1; color: Theme.border }
            ScrollView {
                Layout.preferredWidth: root.width < 900 ? 250 : 300
                Layout.fillHeight: true; clip: true; contentWidth: availableWidth
                ColumnLayout {
                    width: parent.width; spacing: 20
                    RingParameterCard {
                        Layout.fillWidth: true
                        visible: state.pending !== null
                        editor: state
                    }
                    RingSlotEditor { Layout.fillWidth: true; visible: state.pending === null; editor: state }
                    Rectangle { Layout.fillWidth: true; height: 1; color: Theme.border }
                    RingActionLibrary {
                        objectName: "actionLibrary"
                        Layout.fillWidth: true; Layout.preferredHeight: 500
                        onChosen: entry => root.chooseAction(entry)
                    }
                }
            }
        }
        RowLayout {
            Layout.fillWidth: true
            RingFlatButton { text: "↶ Undo"; enabled: state.canUndo; onClicked: state.undo() }
            RingFlatButton { text: "↷ Redo"; enabled: state.canRedo; onClicked: state.redo() }
            Label { text: "Saved automatically"; color: Theme.textSecondary; font.pixelSize: 12 }
            Item { Layout.fillWidth: true }
            RingFlatButton {
                text: "⚙"; Accessible.name: "Ring size settings"
                onClicked: settings.open()
                Menu {
                    id: settings
                    y: -height
                    background: Rectangle {
                        implicitWidth: 160
                        radius: 8
                        color: Theme.cardBg
                        border.color: Theme.border
                    }
                    Repeater {
                        model: [80, 100, 150]
                        MenuItem {
                            id: sizeItem
                            required property int modelData
                            text: "Ring size " + modelData + "%"
                            checkable: true
                            checked: state.size === modelData
                            onTriggered: state.changeSize(modelData)
                            contentItem: Text {
                                text: sizeItem.text
                                color: sizeItem.checked ? Theme.accent : Theme.text
                                font.pixelSize: 13
                                font.bold: sizeItem.checked
                                verticalAlignment: Text.AlignVCenter
                                leftPadding: 8
                            }
                            background: Rectangle {
                                radius: 6
                                color: sizeItem.highlighted ? Theme.hoverBg : "transparent"
                            }
                        }
                    }
                }
            }
        }
    }
}
