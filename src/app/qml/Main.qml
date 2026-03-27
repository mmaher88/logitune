import QtQuick
import QtQuick.Controls
import Logitune

ApplicationWindow {
    id: root
    width: 960; height: 640
    visible: true
    title: "Logitune"
    color: Theme.background
    minimumWidth: 800; minimumHeight: 540

    onClosing: function(close) {
        close.accepted = false
        hide()
    }

    StackView {
        id: mainStack
        anchors.fill: parent
        initialItem: homeViewComponent
    }

    Component {
        id: homeViewComponent
        HomeView {
            onDeviceClicked: mainStack.push(deviceViewComponent)
        }
    }
    Component { id: deviceViewComponent; DeviceView {} }

    // Permission error overlay
    Rectangle {
        id: permissionError
        anchors.fill: parent
        color: Theme.background
        visible: false
        z: 100

        Column {
            anchors.centerIn: parent
            spacing: 16
            width: 400

            Text {
                text: "Permission Required"
                font { pixelSize: 24; bold: true }
                color: Theme.text
                anchors.horizontalCenter: parent.horizontalCenter
            }
            Text {
                text: "Logitune needs permission to access your mouse.\n\nPlease log out and back in for udev rules to take effect."
                font.pixelSize: 14
                color: Theme.textSecondary
                wrapMode: Text.WordWrap
                width: parent.width
                horizontalAlignment: Text.AlignHCenter
                lineHeight: 1.4
            }
        }
    }

    Toast { id: appToast }
}
