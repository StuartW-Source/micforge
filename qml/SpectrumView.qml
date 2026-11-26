import QtQuick

// What the Voice Gate hears, by pitch: the live sound, the gate's listening band, and the average
// spectra of the Setup recordings (your voice, other voices, room).
Rectangle {
    id: root
    readonly property var sp: controller.spectrum
    readonly property var p: controller.params
    property real minDb: -100
    property real maxDb: -10
    radius: Theme.radiusSm + 2
    color: Theme.well
    border.color: Theme.border
    implicitHeight: 180

    function xOf(hz) { return Math.log(hz / sp.lowHz) / Math.log(sp.highHz / sp.lowHz) * cv.width }
    function yOf(db) { return (1 - (Math.max(minDb, Math.min(maxDb, db)) - minDb) / (maxDb - minDb)) * cv.height }

    Canvas {
        id: cv
        anchors { fill: parent; margins: 8; bottomMargin: 22 }
        onWidthChanged: requestPaint()
        onHeightChanged: requestPaint()
        Connections { target: root.sp; function onLiveChanged() { cv.requestPaint() } function onRecordedChanged() { cv.requestPaint() } }
        Connections { target: Theme; function onDarkChanged() { cv.requestPaint() } }
        function curve(ctx, vals, color, dashed, fill) {
            if (!vals || vals.length === 0) return
            var n = vals.length
            ctx.beginPath()
            for (var i = 0; i < n; ++i) {
                var x = (i + 0.5) / n * width, y = root.yOf(vals[i])
                if (i === 0) ctx.moveTo(x, y); else ctx.lineTo(x, y)
            }
            if (fill) {
                ctx.lineTo(width, height); ctx.lineTo(0, height); ctx.closePath()
                ctx.fillStyle = Theme.alpha(color, 0.18); ctx.fill()
            } else {
                ctx.setLineDash(dashed ? [5, 4] : []); ctx.strokeStyle = color; ctx.lineWidth = 1.6; ctx.stroke(); ctx.setLineDash([])
            }
        }
        onPaint: {
            var ctx = getContext("2d")
            ctx.reset()
            // Grid: octaves and 20 dB steps.
            ctx.strokeStyle = Theme.grid; ctx.lineWidth = 1
            for (var hz = 100; hz <= 6400; hz *= 2) { var gx = root.xOf(hz); ctx.beginPath(); ctx.moveTo(gx, 0); ctx.lineTo(gx, height); ctx.stroke() }
            for (var db = -90; db <= -20; db += 20) { var gy = root.yOf(db); ctx.beginPath(); ctx.moveTo(0, gy); ctx.lineTo(width, gy); ctx.stroke() }
            // The gate's listening band.
            if (root.p.gateFocus > 0.5) {
                var x0 = root.p.gateScLow > 21 ? root.xOf(Math.max(root.sp.lowHz, root.p.gateScLow)) : 0
                var x1 = root.p.gateScHigh < 7900 ? root.xOf(root.p.gateScHigh) : width
                ctx.fillStyle = Theme.alpha(Theme.gate, 0.12); ctx.fillRect(x0, 0, x1 - x0, height)
            }
            var rec = root.sp.recorded
            curve(ctx, rec.room, Theme.textFaint, true, false)
            curve(ctx, rec.others, Theme.tone, true, false)
            curve(ctx, rec.you, Theme.gate, true, false)
            curve(ctx, root.sp.live, Theme.input, false, true)
            curve(ctx, root.sp.live, Theme.input, false, false)
        }
    }
    Repeater {
        model: [100, 200, 400, 800, 1600, 3200, 6400]
        Text {
            x: 8 + root.xOf(modelData) - width / 2
            y: root.height - 18
            text: modelData >= 1000 ? (modelData / 1000) + "k" : modelData
            color: Theme.textFaint; font.pixelSize: Theme.fsCaption; font.family: Theme.mono
        }
    }
}
