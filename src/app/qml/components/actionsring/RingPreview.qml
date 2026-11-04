import QtQuick
import QtQuick.Controls
import Logitune

Item {
    id: root
    required property var editor
    // The ring's geometry: first slot at 12 o'clock, clockwise in 45 degree steps; children 22.5
    // degrees apart, centred on their parent. Kept here rather than in a shared .js file: a relative
    // JS import clashes with "import Logitune" once the module only exists inside the binary, which
    // is how it's installed, and the ring then fails to load.
    function rootAngle(index) {
        return index * 45 - 90
    }
    function childAngle(parentIndex, childIndex, count) {
        return rootAngle(parentIndex) + (childIndex - (count - 1) / 2) * 22.5
    }
    function point(angleDegrees, radius) {
        let radians = angleDegrees * Math.PI / 180
        return Qt.point(Math.cos(radians) * radius, Math.sin(radians) * radius)
    }
    readonly property real sizeScale: editor.size / 100
    readonly property var targets: {
        let result = []
        for (let i = 0; i < editor.entries.length; ++i) {
            let p = root.point(root.rootAngle(i), 82 * sizeScale)
            result.push({slot: i, child: -1, entry: editor.entries[i], x: p.x, y: p.y})
        }
        let parent = editor.entries[editor.selectedSlot]
        if (parent && parent.children !== undefined) {
            let count = Math.min(8, parent.children.length + 1)
            for (let j = 0; j < count; ++j) {
                let p = root.point(root.childAngle(editor.selectedSlot, j, count), 182 * sizeScale)
                result.push({slot: editor.selectedSlot, child: j, entry: parent.children[j] || {}, x: p.x, y: p.y})
            }
        }
        return result
    }
    // Each label sits right beside its own bubble on the side facing away from the hub, so no
    // leader lines are needed. While a submenu is fanned out, main-slot labels pointing into the
    // fan are hidden; they would sit on top of the child bubbles.
    readonly property bool submenuOpen: targets.some(t => t.child >= 0)
    readonly property var labels: {
        let result = []
        for (let t of targets) {
            let isMainSlot = t.child < 0
            let fanAngle = root.rootAngle(editor.selectedSlot)
            let slotAngle = root.rootAngle(t.slot)
            let pointsIntoFan = isMainSlot && submenuOpen && angleBetween(slotAngle, fanAngle) <= 70
            if (!pointsIntoFan)
                result.push(t)
        }
        return result
    }

    function angleBetween(first, second) {
        let difference = Math.abs(first - second) % 360
        return difference > 180 ? 360 - difference : difference
    }
    Rectangle {
        anchors.centerIn: parent
        width: 32 * root.sizeScale; height: width; radius: width / 2
        color: Theme.surface; border.color: Theme.border
        Glyph { icon: "󰍽"; pixelSize: 20 * root.sizeScale; color: Theme.textSecondary }
    }
    Repeater {
        model: root.labels
        Rectangle {
            id: pill
            required property var modelData
            readonly property real gap: 24 * root.sizeScale + 8
            // Bubbles on the hub's vertical line get their label above or below; the rest to the side.
            readonly property bool onVerticalLine: Math.abs(modelData.x) < 1
            readonly property bool onLeft: modelData.x < -1

            // Submenu children sit only ~70 px apart, so a side label would run into the next
            // child. Their labels go straight outward from the hub instead, and in a fan that runs
            // across the top or bottom every other label is pushed further out so neighbours clear.
            readonly property bool isChild: modelData.child >= 0
            readonly property real angle: Math.atan2(modelData.y, modelData.x)
            readonly property bool fanIsHorizontal: Math.abs(Math.cos(angle)) < 0.5
            readonly property real stagger: isChild && fanIsHorizontal && modelData.child % 2 === 1 ? 30 : 0
            readonly property point outwardCentre: {
                let reach = Math.hypot(modelData.x, modelData.y) + gap + stagger
                // Push the pill's centre out by half its own size along the same direction, so its
                // nearest edge (not its middle) sits next to the bubble.
                let along = Math.abs(Math.cos(angle)) * width / 2 + Math.abs(Math.sin(angle)) * height / 2
                return Qt.point((reach + along) * Math.cos(angle), (reach + along) * Math.sin(angle))
            }

            width: Math.min(pillText.implicitWidth + 20, 150)
            height: 26
            radius: height / 2
            x: {
                if (isChild)
                    return root.width / 2 + outwardCentre.x - width / 2
                let centreX = root.width / 2 + modelData.x
                if (onVerticalLine)
                    return centreX - width / 2
                return onLeft ? centreX - gap - width : centreX + gap
            }
            y: {
                if (isChild)
                    return root.height / 2 + outwardCentre.y - height / 2
                let centreY = root.height / 2 + modelData.y
                if (!onVerticalLine)
                    return centreY - height / 2
                return modelData.y < 0 ? centreY - gap - height : centreY + gap
            }
            color: Theme.cardBg
            border.color: Theme.border

            Text {
                id: pillText
                anchors.centerIn: parent
                width: Math.min(implicitWidth, 130)
                text: pill.modelData.entry.name || "Add action"
                elide: Text.ElideRight
                color: pill.modelData.entry.name ? Theme.text : Theme.textSecondary
                font.pixelSize: 12
            }
            TapHandler { onTapped: root.editor.select(pill.modelData.slot, pill.modelData.child) }
        }
    }
    Repeater {
        model: root.targets
        RingEditorBubble {
            required property var modelData
            editor: root.editor
            slot: modelData.slot; child: modelData.child; entry: modelData.entry
            bubbleSize: 48 * root.sizeScale
            x: root.width / 2 + modelData.x - width / 2
            y: root.height / 2 + modelData.y - height / 2
        }
    }
}
