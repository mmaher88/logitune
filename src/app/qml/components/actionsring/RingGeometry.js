.pragma library

function rootAngle(index) { return index * 45 - 90 }
function childAngle(parent, index, count) { return rootAngle(parent) + (index - (count - 1) / 2) * 22.5 }
function point(angle, radius) {
    let radians = angle * Math.PI / 180
    return Qt.point(Math.cos(radians) * radius, Math.sin(radians) * radius)
}
