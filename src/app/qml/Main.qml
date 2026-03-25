import QtQuick
import QtQuick.Controls
import Logitune

ApplicationWindow {
    id: root
    width: 960; height: 640
    visible: true
    title: "Logitune"
    color: "#F5F5F5"
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

    Component { id: homeViewComponent; HomeView {} }
    Component { id: deviceViewComponent; DeviceView {} }
}
