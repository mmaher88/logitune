import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Logitune

Rectangle {
    id: root
    property string conflictPath: ""

    signal viewDiffRequested(string path)

    visible: conflictPath.length > 0
    height: visible ? 40 : 0
    color: "#FFF3CD"
    border.color: "#FFEEBA"
    border.width: visible ? 1 : 0

    Connections {
        target: typeof EditorModel !== 'undefined' ? EditorModel : null
        function onExternalChangeDetected(path) {
            root.conflictPath = path
        }
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 12
        anchors.rightMargin: 12
        spacing: 8

        Text {
            text: "This file was changed on disk."
            font.pixelSize: 12
            color: "#856404"
            Layout.fillWidth: true
        }

        Button {
            text: "Keep my edits"
            onClicked: root.conflictPath = ""
        }
        Button {
            text: "Load disk version"
            onClicked: {
                if (typeof EditorModel !== 'undefined') {
                    EditorModel.reset()
                }
                root.conflictPath = ""
            }
        }
        Button {
            text: "View diff"
            onClicked: root.viewDiffRequested(root.conflictPath)
        }
    }
}
