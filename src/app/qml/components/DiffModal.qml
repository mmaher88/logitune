import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Logitune

Dialog {
    id: root
    modal: true
    width: Math.min(900, parent ? parent.width - 40 : 900)
    height: Math.min(640, parent ? parent.height - 40 : 640)
    anchors.centerIn: parent
    title: "External change diff"
    standardButtons: Dialog.Close

    property string diskText: ""
    property string memoryText: ""

    function open(path) {
        diskText = readFile(path + "/descriptor.json")
        if (typeof EditorModel !== 'undefined') {
            var pending = EditorModel.pendingFor(path)
            memoryText = JSON.stringify(pending, null, 2)
        } else {
            memoryText = "(EditorModel not registered)"
        }
        visible = true
    }

    function readFile(p) {
        var xhr = new XMLHttpRequest()
        try {
            xhr.open("GET", "file://" + p, false)
            xhr.send(null)
            return xhr.responseText
        } catch (e) {
            return "(failed to read " + p + ": " + e + ")"
        }
    }

    contentItem: ColumnLayout {
        spacing: 8

        RowLayout {
            Layout.fillWidth: true
            spacing: 0
            Text {
                text: "On disk"
                font.bold: true
                font.pixelSize: 12
                color: Theme.text
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignHCenter
            }
            Text {
                text: "In memory"
                font.bold: true
                font.pixelSize: 12
                color: Theme.text
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignHCenter
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 8

            Flickable {
                Layout.fillWidth: true
                Layout.fillHeight: true
                contentWidth: leftText.implicitWidth
                contentHeight: leftText.implicitHeight
                clip: true
                Text {
                    id: leftText
                    text: root.diskText
                    font.family: "monospace"
                    font.pixelSize: 11
                    color: Theme.text
                }
            }

            Flickable {
                Layout.fillWidth: true
                Layout.fillHeight: true
                contentWidth: rightText.implicitWidth
                contentHeight: rightText.implicitHeight
                clip: true
                Text {
                    id: rightText
                    text: root.memoryText
                    font.family: "monospace"
                    font.pixelSize: 11
                    color: Theme.text
                }
            }
        }
    }
}
