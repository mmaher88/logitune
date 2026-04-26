import QtQuick
import QtTest

Item {
    width: 800; height: 600

    property real imgX: 100
    property real imgY: 50
    property real imgW: 600
    property real imgH: 400

    function inverseDragMath(centroidX, centroidY) {
        var xPct = (centroidX - imgX) / imgW
        var yPct = (centroidY - imgY) / imgH
        return [Math.max(0, Math.min(1, xPct)),
                Math.max(0, Math.min(1, yPct))]
    }

    TestCase {
        name: "EasySwitchDragMath"

        function test_centerOfImageIsHalfHalf() {
            var r = inverseDragMath(imgX + imgW / 2, imgY + imgH / 2)
            verify(Math.abs(r[0] - 0.5) < 1e-6)
            verify(Math.abs(r[1] - 0.5) < 1e-6)
        }
        function test_topLeftCornerIsZeroZero() {
            var r = inverseDragMath(imgX, imgY)
            compare(r[0], 0)
            compare(r[1], 0)
        }
        function test_outsideClampsToRange() {
            var r = inverseDragMath(imgX - 999, imgY + imgH + 999)
            compare(r[0], 0)
            compare(r[1], 1)
        }
    }
}
