import QtQuick
import QtQuick.Controls
import Logitune

Rectangle {
    id: toast
    property string message: ""
    property int duration: 3000

    width: Math.min(parent.width - 48, messageText.implicitWidth + 48)
    height: 44
    radius: 22
    color: Theme.dark ? "#444444" : "#333333"
    opacity: 0
    visible: opacity > 0

    anchors.horizontalCenter: parent.horizontalCenter
    anchors.bottom: parent.bottom
    anchors.bottomMargin: 32

    Text {
        id: messageText
        anchors.centerIn: parent
        text: toast.message
        color: "#FFFFFF"
        font.pixelSize: 13
    }

    function show(msg, ms) {
        message = msg
        duration = ms || 3000
        showAnim.start()
        hideTimer.restart()
    }

    NumberAnimation {
        id: showAnim
        target: toast; property: "opacity"
        from: 0; to: 1; duration: 200
    }

    NumberAnimation {
        id: hideAnim
        target: toast; property: "opacity"
        from: 1; to: 0; duration: 300
    }

    Timer {
        id: hideTimer
        interval: toast.duration
        onTriggered: hideAnim.start()
    }
}
