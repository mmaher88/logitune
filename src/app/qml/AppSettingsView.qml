import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Logitune

Item {
    id: root

    RowLayout {
        id: header
        anchors { top: parent.top; left: parent.left; right: parent.right }
        height: 60
        spacing: 8

        Item { width: 16 }

        Rectangle {
            width: 48; height: 48
            color: "transparent"

            Text {
                anchors.centerIn: parent
                text: "\u2190"
                font.pixelSize: 22
                color: backHover.hovered ? Theme.accent : "#444444"
                Behavior on color { ColorAnimation { duration: 120 } }
            }

            HoverHandler { id: backHover }
            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                onClicked: root.StackView.view.pop()
            }
        }

        Text {
            text: "Settings"
            font.pixelSize: 22
            font.bold: true
            color: Theme.text
        }

        Item { Layout.fillWidth: true }
    }

    Column {
        anchors {
            top: header.bottom
            left: parent.left
            margins: 32
        }
        spacing: 24
        width: 400

        // Dark mode toggle
        Row {
            width: parent.width
            Text {
                text: "Dark mode"
                font.pixelSize: 13
                color: Theme.text
                width: parent.width - darkToggle.width
                anchors.verticalCenter: parent.verticalCenter
            }
            LogituneToggle {
                id: darkToggle
                checked: Theme.dark
                onToggled: function(val) {
                    Theme.dark = val
                    SettingsModel.saveThemeDark(val)
                }
            }
        }

        // Separator
        Rectangle { width: parent.width; height: 1; color: Theme.border }

        // Debug logging toggle
        Row {
            width: parent.width
            Text {
                text: "Debug logging"
                font.pixelSize: 13
                color: Theme.text
                width: parent.width - loggingToggle.width
                anchors.verticalCenter: parent.verticalCenter
            }
            LogituneToggle {
                id: loggingToggle
                checked: SettingsModel.loggingEnabled
                onToggled: function(val) { SettingsModel.loggingEnabled = val }
            }
        }

        // Log file path
        Text {
            visible: SettingsModel.loggingEnabled
            text: SettingsModel.logFilePath || ""
            font.pixelSize: 11
            color: Theme.textSecondary
            wrapMode: Text.WrapAnywhere
            width: parent.width
        }

        // Separator
        Rectangle { width: parent.width; height: 1; color: Theme.border }

        // Report Bug button
        Rectangle {
            width: 180; height: 40
            radius: 4
            color: SettingsModel.loggingEnabled
                ? (reportHover.hovered ? Theme.accent : "transparent")
                : "transparent"
            border.color: SettingsModel.loggingEnabled ? Theme.accent : Theme.border
            border.width: 1
            opacity: SettingsModel.loggingEnabled ? 1.0 : 0.4

            Text {
                anchors.centerIn: parent
                text: "Report Bug"
                font.pixelSize: 13
                color: SettingsModel.loggingEnabled
                    ? (reportHover.hovered ? "#000000" : Theme.accent)
                    : Theme.textSecondary
            }

            HoverHandler { id: reportHover; enabled: SettingsModel.loggingEnabled }

            MouseArea {
                anchors.fill: parent
                cursorShape: SettingsModel.loggingEnabled ? Qt.PointingHandCursor : Qt.ArrowCursor
                enabled: SettingsModel.loggingEnabled
                onClicked: SettingsModel.openBugReport()
            }
        }

        // Test crash button — debug builds only. This deliberately throws, so
        // it must never be reachable in a shipped package (see issue #146).
        Rectangle {
            visible: SettingsModel.debugBuild && SettingsModel.loggingEnabled
            width: 180; height: 40
            radius: 4; color: "transparent"
            border.color: "#ff4444"; border.width: 1

            Text {
                anchors.centerIn: parent
                text: "Test Exception"
                font.pixelSize: 13; color: "#ff4444"
            }

            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                onClicked: SettingsModel.testCrash()
            }
        }

        // Separator
        Rectangle { width: parent.width; height: 1; color: Theme.border }

        // Reset button
        Rectangle {
            width: 180; height: 40
            radius: 4; color: "transparent"
            border.color: Theme.border; border.width: 1

            Text {
                anchors.centerIn: parent
                text: "Reset to defaults"
                font.pixelSize: 13; color: Theme.textSecondary
            }

            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                onClicked: resetDialog.open()
            }
        }

        // App version
        Text {
            text: "Logitune v" + Qt.application.version
            font.pixelSize: 11
            color: Theme.textSecondary
            topPadding: 16
        }
    }

    Dialog {
        id: resetDialog
        title: "Reset to defaults?"
        modal: true
        anchors.centerIn: parent
        standardButtons: Dialog.Ok | Dialog.Cancel
    }
}
