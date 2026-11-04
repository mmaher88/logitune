import QtQuick
import Logitune

// Sizes and timings follow ~/re/logioptionsplus/findings/02-ring-behaviour.md. Styling is our own.
Item {
    id: root

    // The hub is unknown until the pointer reaches this overlay; see place().
    property real hubX: 0
    property real hubY: 0
    property bool isPlaced: false
    property string screenName
    required property var slotList
    required property var launcher
    property var themeOverrides: ({})
    readonly property var theme: Object.assign({surface: Theme.surface, surfaceRaised: Theme.cardBg,
        text: Theme.text, accent: Theme.accent, onAccent: Theme.activeTabText,
        border: Theme.border, error: Theme.error}, themeOverrides)

    property real sizeScale: 1.0
    readonly property real slotRadius: 82 * sizeScale
    readonly property real childRadius: 182 * sizeScale
    readonly property real buttonSize: 48 * sizeScale
    readonly property real closeSize: 32 * sizeScale
    readonly property real centreHitRadius: 24 * sizeScale
    // Hover reach is much larger than the drawn buttons, so the gaps between them still select something.
    readonly property real hoverReach: 162.5 * sizeScale

    readonly property int noTarget: -2
    readonly property int centreTarget: -1

    // A target is a main slot (or centre/none) plus, inside an open submenu, a child index.
    property int hovered: noTarget
    property int hoveredChild: -1
    property int pressedSlot: noTarget
    property int pressedChild: -1
    property int expandedParent: -1
    property real expansion: 0
    property bool closing: false

    readonly property var expandedChildren: expandedParent >= 0 ? slotList[expandedParent].children : []

    function slotAngle(index) {
        // First slot at 12 o'clock, then clockwise in 45 degree steps.
        return index * 45 - 90
    }

    function childAngle(parentIndex, childIndex, childCount) {
        // Children are 22.5 degrees apart, as a group centred on the parent's angle.
        let offset = (childIndex - (childCount - 1) / 2) * 22.5
        return slotAngle(parentIndex) + offset
    }

    function pointOnCircle(radius, angleDegrees) {
        let angle = angleDegrees * Math.PI / 180
        return Qt.point(radius * Math.cos(angle), radius * Math.sin(angle))
    }

    function angleBetween(firstDegrees, secondDegrees) {
        let difference = Math.abs(firstDegrees - secondDegrees) % 360
        return difference > 180 ? 360 - difference : difference
    }

    // Centres of every button currently on screen, so the legend can avoid covering them.
    function visibleButtonCentres() {
        let centres = [Qt.point(0, 0)]
        for (let i = 0; i < slotList.length; i++)
            centres.push(pointOnCircle(slotRadius, slotAngle(i)))
        for (let c = 0; c < expandedChildren.length; c++)
            centres.push(pointOnCircle(childRadius, childAngle(expandedParent, c, expandedChildren.length)))
        return centres
    }

    function rectangleHitsButton(left, top, width, height, ownCentre) {
        let clearance = buttonSize / 2 + 4
        let centres = visibleButtonCentres()
        for (let i = 0; i < centres.length; i++) {
            let centre = centres[i]
            let isOwnButton = Math.abs(centre.x - ownCentre.x) < 1 && Math.abs(centre.y - ownCentre.y) < 1
            if (isOwnButton)
                continue
            let nearestX = Math.max(left, Math.min(centre.x, left + width))
            let nearestY = Math.max(top, Math.min(centre.y, top + height))
            if (Math.hypot(centre.x - nearestX, centre.y - nearestY) < clearance)
                return true
        }
        return false
    }

    // Tries right, then left, then straight out from the hub; keeps the first spot that covers no button.
    function legendPlacement(anchor, width, height) {
        let gap = buttonSize / 2 + 10
        let outwardDown = anchor.y >= 0
        let candidates = [
            { side: "right", x: anchor.x + gap, y: anchor.y - height / 2 },
            { side: "left", x: anchor.x - gap - width, y: anchor.y - height / 2 },
            outwardDown ? { side: "below", x: anchor.x - width / 2, y: anchor.y + gap }
                        : { side: "above", x: anchor.x - width / 2, y: anchor.y - gap - height },
        ]
        for (let i = 0; i < candidates.length; i++) {
            let spot = candidates[i]
            if (!rectangleHitsButton(spot.x, spot.y, width, height, anchor))
                return spot
        }
        return candidates[0]
    }

    function easeOut(progress) {
        return 1 - Math.pow(1 - progress, 3)
    }

    function hasChildren(index) {
        return index >= 0 && slotList[index].children !== undefined
    }

    function hitTest(x, y) {
        let dx = x - hubX
        let dy = y - hubY
        if (Math.hypot(dx, dy) < centreHitRadius)
            return { slot: centreTarget, child: -1 }

        let bestSlot = noTarget
        let bestSlotDistance = hoverReach
        for (let i = 0; i < slotList.length; i++) {
            let centre = pointOnCircle(slotRadius, slotAngle(i))
            let distance = Math.hypot(dx - centre.x, dy - centre.y)
            if (distance < bestSlotDistance) {
                bestSlot = i
                bestSlotDistance = distance
            }
        }

        let bestChild = -1
        let bestChildDistance = hoverReach
        for (let c = 0; c < expandedChildren.length; c++) {
            let centre = pointOnCircle(childRadius, childAngle(expandedParent, c, expandedChildren.length))
            let distance = Math.hypot(dx - centre.x, dy - centre.y)
            if (distance < bestChildDistance) {
                bestChild = c
                bestChildDistance = distance
            }
        }

        // Once a submenu is open, everything 30% of the way from the main ring to the outer ring
        // belongs to the submenu, so a short move outward already reaches the children.
        let fromHub = Math.hypot(dx, dy)
        let submenuBoundary = slotRadius + 0.3 * (childRadius - slotRadius)
        if (bestChild >= 0 && fromHub > submenuBoundary)
            return { slot: expandedParent, child: bestChild }

        // The open submenu owns the wedge from its parent out to its children, and inside it the
        // nearest child is selected, even with the pointer still on the parent. So a click on the
        // parent picks the child in front of it, and a small nudge sideways picks a neighbour.
        // The wedge also stops a diagonal move from hovering a neighbouring main slot and
        // collapsing the submenu on the way out.
        if (expandedParent >= 0 && fromHub > slotRadius - buttonSize / 2) {
            let fanHalfWidth = (expandedChildren.length - 1) / 2 * 22.5 + 11.25
            let pointerAngle = Math.atan2(dy, dx) * 180 / Math.PI
            let insideWedge = angleBetween(pointerAngle, slotAngle(expandedParent)) <= fanHalfWidth
            if (insideWedge)
                return { slot: expandedParent, child: bestChild }
        }
        return { slot: bestSlot, child: -1 }
    }

    function updateHover(x, y) {
        let target = hitTest(x, y)
        let changed = target.slot !== hovered || target.child !== hoveredChild
        // Moving onto a different item while the button is down cancels that click.
        if (changed) {
            pressedSlot = noTarget
            pressedChild = -1
        }
        hovered = target.slot
        hoveredChild = target.child
        if (changed && hovered >= 0)
            launcher.haptic("hover")

        if (hasChildren(hovered))
            expandedParent = hovered
        else if (hovered >= 0 && hovered !== expandedParent)
            expandedParent = -1
    }

    // The slot or submenu child under the pointer, or null.
    function hoveredEntry() {
        if (hoveredChild >= 0)
            return expandedChildren[hoveredChild]
        if (hovered >= 0)
            return slotList[hovered]
        return null
    }

    function isAdjuster(entry) {
        return entry !== null && entry !== undefined && entry.adjust !== undefined
    }

    // Level shown on an adjuster's label: a percentage, or -1 while unknown.
    property int level: -1
    property bool levelMuted: false
    property real wheelRemainder: 0

    function refreshLevel() {
        let entry = hoveredEntry()
        if (isAdjuster(entry) && entry.adjust.read)
            launcher.readLevel(entry.adjust.read)
    }

    // Turns a command's output into a percentage. Handles wpctl ("Volume: 0.45 [MUTED]", a 0..1
    // fraction) and anything that prints a plain number or "45%".
    function parseLevel(output) {
        let match = output.match(/(\d*\.?\d+)\s*(%?)/)
        if (!match)
            return {percent: -1, muted: false}
        let value = parseFloat(match[1])
        let isFraction = match[2] === "" && match[1].indexOf(".") >= 0 && value <= 1.5
        let percent = Math.round(isFraction ? value * 100 : value)
        return {percent: percent, muted: /mute/i.test(output)}
    }

    Connections {
        target: root.launcher
        function onLevelRead(output) {
            let parsed = root.parseLevel(output)
            root.level = parsed.percent
            root.levelMuted = parsed.muted
        }
    }

    // One step per notch of the wheel (120 units); smooth-scrolling mice send smaller deltas,
    // so the remainder carries over between events.
    function scrollAdjuster(delta) {
        let entry = hoveredEntry()
        if (!isAdjuster(entry))
            return
        // Turning back the other way starts fresh; otherwise the leftover from the old direction
        // has to be scrolled off first and the first step back feels late.
        let reversed = delta * wheelRemainder < 0
        if (reversed)
            wheelRemainder = 0
        wheelRemainder += delta
        while (Math.abs(wheelRemainder) >= 120) {
            let goingUp = wheelRemainder > 0
            // The service reads the level once the change has finished, so the label never shows
            // the value from one step earlier.
            launcher.adjust(goingUp ? entry.adjust.up : entry.adjust.down, entry.adjust.read || "")
            wheelRemainder -= goingUp ? 120 : -120
        }
    }

    onHoveredChanged: { level = -1; wheelRemainder = 0; refreshLevel() }
    onHoveredChildChanged: { level = -1; wheelRemainder = 0; refreshLevel() }

    function choose(slot, child) {
        if (closing)
            return
        // Clicking an adjuster runs its press command (mute, for volume) and keeps the ring open,
        // so you can keep scrolling.
        let entry = child >= 0 ? slotList[slot].children[child] : slotList[slot]
        if (slot >= 0 && isAdjuster(entry)) {
            if (entry.adjust.press)
                launcher.adjust(entry.adjust.press, entry.adjust.read || "")
            return
        }
        if (child >= 0) {
            launcher.run(slotList[slot].children[child].action)
            dismiss()
            return
        }
        // Clicking a parent only keeps its submenu open.
        if (hasChildren(slot))
            return
        if (slot >= 0) {
            launcher.run(slotList[slot].action)
        }
        dismiss()
    }

    // Holding the trigger past 500 ms and releasing it picks whatever is under the pointer.
    function selectHovered() {
        choose(hovered, hoveredChild)
    }

    function dismiss() {
        if (closing)
            return
        closing = true
        closeAnimation.start()
    }

    onExpandedParentChanged: {
        if (expandedParent >= 0) {
            expansion = 0
            expandAnimation.restart()
        } else {
            expandAnimation.stop()
            expansion = 0
        }
    }

    focus: true
    Keys.onEscapePressed: dismiss()

    MouseArea {
        anchors.fill: parent
        hoverEnabled: true

        // Only real movement places the ring here (the fallback when the compositor can't be asked).
        // onEntered is deliberately unused: Qt fires it on show with a stale, often wrong position.
        onPositionChanged: (mouse) => {
            if (!root.isPlaced) {
                root.place(mouse.x, mouse.y)
                return
            }
            root.updateHover(mouse.x, mouse.y)
        }
        onPressed: (mouse) => {
            if (!root.isPlaced)
                return
            root.updateHover(mouse.x, mouse.y)
            root.pressedSlot = root.hovered
            root.pressedChild = root.hoveredChild
        }
        onReleased: {
            if (!root.isPlaced)
                return
            let sameTarget = root.pressedSlot === root.hovered && root.pressedChild === root.hoveredChild
            if (sameTarget)
                root.choose(root.hovered, root.hoveredChild)
        }
    }

    // Scrolling over an adjuster changes its level. Vertical wheel up raises it; the MX Master's
    // thumb wheel sends horizontal deltas, and scrolling it forward raises it too.
    WheelHandler {
        acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
        onWheel: (event) => {
            if (!root.isPlaced)
                return
            let delta = event.angleDelta.y !== 0 ? event.angleDelta.y : -event.angleDelta.x
            root.scrollAdjuster(delta)
        }
    }

    Item {
        id: ring
        x: root.hubX
        y: root.hubY
        opacity: 0
        scale: 0

        // Submenu opening, timed from the parent being hovered (320 ms in total):
        //   0-100 ms   the nub swells into a short neck
        //   100-220 ms a teardrop pinches off and flies out to the child nearest the parent's direction,
        //              growing to full size, rounded front first, tail pointing back at the parent
        //   220-320 ms it becomes that child; the other children grow from dots in place
        Item {
            id: teardrop

            readonly property real elapsed: root.expansion * 320
            readonly property real neck: root.easeOut(Math.min(elapsed / 100, 1))
            readonly property real flight: root.easeOut(Math.min(Math.max((elapsed - 100) / 120, 0), 1))
            readonly property real arrive: root.easeOut(Math.min(Math.max((elapsed - 220) / 100, 0), 1))

            readonly property int childCount: root.expandedChildren.length
            readonly property int landingChild: Math.floor((childCount - 1) / 2)
            readonly property real parentAngle: root.slotAngle(root.expandedParent)
            readonly property point launch: root.pointOnCircle(root.slotRadius + root.buttonSize / 2, parentAngle)
            readonly property point landing: root.pointOnCircle(root.childRadius,
                root.childAngle(root.expandedParent, landingChild, childCount))
            readonly property real heading: Math.atan2(landing.y - launch.y, landing.x - launch.x)

            readonly property point blobCentre: Qt.point(launch.x + (landing.x - launch.x) * flight,
                                                         launch.y + (landing.y - launch.y) * flight)
            readonly property real blobSize: 16 + (root.buttonSize - 16) * flight

            visible: root.expandedParent >= 0 && arrive < 1

            // The neck: a short capsule growing out of the nub before the drop pinches off.
            Rectangle {
                visible: teardrop.elapsed < 100
                readonly property real length: 4 + 14 * teardrop.neck
                readonly property real thickness: 12 + 4 * teardrop.neck
                readonly property point middle: root.pointOnCircle(
                    root.slotRadius + root.buttonSize / 2 - 4 + length / 2, teardrop.parentAngle)
                width: length + thickness
                height: thickness
                radius: thickness / 2
                x: middle.x - width / 2
                y: middle.y - height / 2
                rotation: teardrop.parentAngle
                color: root.theme.accent
            }

            // The tail: a square turned 45 degrees behind the drop, its far corner pointing at the parent.
            // It shrinks away as the drop slows down, so the drop lands round.
            Rectangle {
                visible: teardrop.elapsed >= 100
                readonly property real size: teardrop.blobSize * 0.5 * (1 - teardrop.flight)
                readonly property real behind: teardrop.blobSize * 0.42
                width: size
                height: size
                x: teardrop.blobCentre.x - behind * Math.cos(teardrop.heading) - width / 2
                y: teardrop.blobCentre.y - behind * Math.sin(teardrop.heading) - height / 2
                rotation: teardrop.heading * 180 / Math.PI + 45
                color: root.theme.accent
            }

            Rectangle {
                visible: teardrop.elapsed >= 100
                width: teardrop.blobSize
                height: teardrop.blobSize
                radius: width / 2
                x: teardrop.blobCentre.x - width / 2
                y: teardrop.blobCentre.y - height / 2
                color: root.theme.accent
                opacity: 1 - teardrop.arrive
            }
        }

        Repeater {
            model: root.expandedChildren

            RingButton {
                required property int index
                required property var modelData
                readonly property bool isLanding: index === teardrop.landingChild

                centre: root.pointOnCircle(root.childRadius,
                                           root.childAngle(root.expandedParent, index, root.expandedChildren.length))
                // The child the drop lands on appears the moment it arrives; the rest grow from dots.
                opacity: isLanding ? (teardrop.elapsed >= 220 ? 1 : 0) : Math.min(teardrop.arrive * 3, 1)
                baseScale: isLanding ? 1 : 0.2 + 0.8 * teardrop.arrive
                theme: root.theme
                size: root.buttonSize
                icon: modelData.icon
                iconSize: 20 * root.sizeScale
                hovered: root.hoveredChild === index
            }
        }

        Repeater {
            model: root.slotList

            RingButton {
                required property int index
                required property var modelData

                centre: root.pointOnCircle(root.slotRadius, root.slotAngle(index))
                theme: root.theme
                size: root.buttonSize
                icon: modelData.icon
                iconSize: 20 * root.sizeScale
                hovered: root.hovered === index && root.hoveredChild < 0
                open: root.expandedParent === index
                hasSubmenu: root.hasChildren(index)
            }
        }

        RingButton {
            centre: Qt.point(0, 0)
            theme: root.theme
            size: root.closeSize
            icon: ""
            iconSize: 14 * root.sizeScale
            hovered: root.hovered === root.centreTarget
            hoverColor: root.theme.error
            hoverScale: 1.0
        }

        // Legend: a pill beside the hovered button, on the side facing away from the hub, shown only
        // after the pointer has rested on the button for a while.
        Item {
            id: legend

            // Slots that open a submenu get no legend; their children say what's inside.
            readonly property string name: {
                if (root.hoveredChild >= 0)
                    return root.expandedChildren[root.hoveredChild].name
                if (root.hovered >= 0 && !root.hasChildren(root.hovered))
                    return root.slotList[root.hovered].name
                return ""
            }
            // Adjusters show their live level and appear at once: you're about to scroll, and
            // waiting 800 ms to see the value would defeat the point.
            readonly property bool forAdjuster: root.isAdjuster(root.hoveredEntry())
            readonly property string levelText: {
                if (root.levelMuted)
                    return "muted"
                return root.level >= 0 ? root.level + "%" : ""
            }
            readonly property string label: forAdjuster && levelText !== "" ? name + "  " + levelText : name
            readonly property point anchorCentre: {
                if (root.hoveredChild >= 0)
                    return root.pointOnCircle(root.childRadius, root.childAngle(root.expandedParent,
                                              root.hoveredChild, root.expandedChildren.length))
                if (root.hovered >= 0)
                    return root.pointOnCircle(root.slotRadius, root.slotAngle(root.hovered))
                return Qt.point(0, 0)
            }
            readonly property var placement: root.legendPlacement(anchorCentre, pill.width, pill.height)
            property bool shown: false

            visible: label !== ""
            opacity: shown ? 1 : 0
            x: placement.x
            y: placement.y

            Behavior on opacity { NumberAnimation { duration: 200 } }

            // Keyed on the name, not the full label, so a changing level doesn't restart the delay.
            onNameChanged: {
                shown = false
                if (name === "")
                    return
                if (forAdjuster)
                    shown = true
                else
                    legendDelay.restart()
            }

            Timer {
                id: legendDelay
                interval: 800
                onTriggered: legend.shown = true
            }

            // Pointer: a small square turned 45 degrees, half hidden behind the pill, on the side facing
            // the button.
            Rectangle {
                readonly property string side: legend.placement.side
                width: 8
                height: 8
                rotation: 45
                color: pill.color
                x: {
                    if (side === "right")
                        return -3
                    if (side === "left")
                        return pill.width - 5
                    return pill.width / 2 - 4
                }
                y: {
                    if (side === "below")
                        return -3
                    if (side === "above")
                        return pill.height - 5
                    return pill.height / 2 - 4
                }
            }

            Rectangle {
                id: pill
                width: legendText.implicitWidth + 20
                height: legendText.implicitHeight + 8
                radius: height / 2
                color: root.theme.surfaceRaised
                clip: true

                // Level fill behind the text, for adjusters.
                Rectangle {
                    visible: legend.forAdjuster && root.level >= 0 && !root.levelMuted
                    width: parent.width * Math.min(root.level, 100) / 100
                    height: parent.height
                    radius: parent.radius
                    color: root.theme.accent
                    opacity: 0.35
                    Behavior on width { NumberAnimation { duration: 90 } }
                }

                Text {
                    id: legendText
                    anchors.centerIn: parent
                    text: legend.label
                    font.pixelSize: 13 * root.sizeScale
                    color: root.theme.text
                }
            }
        }
    }

    // One linear clock for the whole 320 ms; the droplet and the children apply their own easing to it.
    NumberAnimation {
        id: expandAnimation
        target: root; property: "expansion"; from: 0; to: 1; duration: 320
    }

    // The first pointer position this overlay hears about becomes the hub. Most compositors report it
    // as soon as the surface appears under the pointer; some only once the mouse moves.
    function place(x, y) {
        hubX = x
        hubY = y
        isPlaced = true
        console.log("ring placed at", x, y, "on", screenName)
        launcher.placed(root)
        openAnimation.start()
    }

    ParallelAnimation {
        id: openAnimation
        NumberAnimation {
            target: ring; property: "scale"; from: 0; to: 1; duration: 125
            easing.type: Easing.BezierSpline; easing.bezierCurve: [0.075, 0, 0.5, 1, 1, 1]
        }
        SequentialAnimation {
            PauseAnimation { duration: 25 }
            NumberAnimation {
                target: ring; property: "opacity"; from: 0; to: 1; duration: 100
                easing.type: Easing.BezierSpline; easing.bezierCurve: [0.075, 0, 0.5, 1, 1, 1]
            }
        }
    }

    ParallelAnimation {
        id: closeAnimation
        NumberAnimation {
            target: ring; property: "scale"; to: 0; duration: 125
            easing.type: Easing.BezierSpline; easing.bezierCurve: [0.4, 0, 0.95, 1, 1, 1]
        }
        NumberAnimation {
            target: ring; property: "opacity"; to: 0; duration: 108
            easing.type: Easing.BezierSpline; easing.bezierCurve: [0.4, 0, 0.95, 1, 1, 1]
        }
        onFinished: launcher.finished()
    }
}
