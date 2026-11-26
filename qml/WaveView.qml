import QtQuick

// Mirrored peak waveform (values 0..1) with an optional playhead (0..1, <0 hides it).
Rectangle {
    id: root
    property var values: []
    property color accent: Theme.input
    property real playhead: -1
    property bool dimmed: false
    radius: Theme.radiusSm + 2
    color: Theme.well
    border.color: Theme.border
    implicitHeight: 84

    Canvas {
        id: cv
        anchors { fill: parent; margins: 6 }
        onWidthChanged: requestPaint()
        onHeightChanged: requestPaint()
        Connections { target: root; function onValuesChanged() { cv.requestPaint() } function onDimmedChanged() { cv.requestPaint() } }
        Connections { target: Theme; function onDarkChanged() { cv.requestPaint() } }
        onPaint: {
            var ctx = getContext("2d")
            ctx.reset()
            var v = root.values, n = v.length
            ctx.strokeStyle = Theme.grid
            ctx.lineWidth = 1
            ctx.beginPath(); ctx.moveTo(0, height / 2); ctx.lineTo(width, height / 2); ctx.stroke()
            if (n === 0) return
            ctx.fillStyle = Theme.alpha(root.accent, root.dimmed ? 0.45 : 0.9)
            var bw = width / n, mid = height / 2
            for (var i = 0; i < n; ++i) {
                // dB-ish scaling so quiet speech is still visible.
                var a = v[i] <= 0 ? 0 : Math.max(0, 1 + Math.log(v[i]) / Math.log(10) / 3)
                var h = Math.max(1, a * (height / 2))
                ctx.fillRect(i * bw, mid - h, Math.max(1, bw - 0.4), 2 * h)
            }
        }
    }
    Rectangle {
        visible: root.playhead >= 0
        x: 6 + root.playhead * (root.width - 12) - 1
        y: 3; width: 2; height: root.height - 6
        color: Theme.text
    }
}
