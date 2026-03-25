import QtQuick

// Mouse device render — MX Master 3S PNG with invisible clickable button zones overlaid.
Item {
    id: root

    implicitWidth:  220
    implicitHeight: 326

    signal buttonClicked(int buttonId)

    // ── Mouse image ──────────────────────────────────────────────────────────
    Image {
        id: mouseImage
        anchors.centerIn: parent
        width: parent.implicitWidth
        height: parent.implicitHeight
        source: "qrc:/Logitune/qml/assets/mx-master-3s.png"
        fillMode: Image.PreserveAspectFit
        smooth: true
        mipmap: true
    }

    // ── Button zone overlays (invisible hit areas) ───────────────────────────
    // Positions tuned for the 220×326 MX Master 3S render at 3/4 angle.

    // Button 0 — Left click (top-left half)
    MouseArea {
        x: 20; y: 10
        width: 90; height: 130
        cursorShape: Qt.PointingHandCursor
        onClicked: root.buttonClicked(0)
    }

    // Button 1 — Right click (top-right half)
    MouseArea {
        x: 115; y: 10
        width: 90; height: 130
        cursorShape: Qt.PointingHandCursor
        onClicked: root.buttonClicked(1)
    }

    // Button 2 — Middle / scroll wheel click
    MouseArea {
        x: 85; y: 40
        width: 40; height: 70
        cursorShape: Qt.PointingHandCursor
        onClicked: root.buttonClicked(2)
    }

    // Button 3 — Back (thumb rear, left side lower)
    MouseArea {
        x: 0; y: 195
        width: 45; height: 40
        cursorShape: Qt.PointingHandCursor
        onClicked: root.buttonClicked(3)
    }

    // Button 4 — Forward (thumb front, left side upper)
    MouseArea {
        x: 0; y: 150
        width: 45; height: 40
        cursorShape: Qt.PointingHandCursor
        onClicked: root.buttonClicked(4)
    }

    // Button 5 — Thumb / gesture button (left side middle)
    MouseArea {
        x: 0; y: 170
        width: 40; height: 30
        cursorShape: Qt.PointingHandCursor
        onClicked: root.buttonClicked(5)
    }

    // Button 6 — Top button (behind scroll wheel, right side)
    MouseArea {
        x: 150; y: 30
        width: 40; height: 25
        cursorShape: Qt.PointingHandCursor
        onClicked: root.buttonClicked(6)
    }
}
