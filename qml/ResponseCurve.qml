import QtQuick

// Draws the combined frequency response of the low cut, equaliser and tone controls,
// using the same filter formulas as the audio engine.
Canvas {
    id: root
    property color accent: Theme.tone
    property bool includeLowCut: true
    property bool includeEq: true
    property bool includeTone: true
    property real minDb: -18
    property real maxDb: 12
    property bool showDbScale: false

    readonly property var p: controller.params
    readonly property var deps: [p.lowCutOn, p.lowCutFreq, p.lowCutSteep, p.toneOn, p.toneBass, p.tonePresence, p.toneAir,
                                 p.bypass, p.eqAdded, p.eqOn,
                                 p.eq1Freq, p.eq1Gain, p.eq1Q, p.eq2Freq, p.eq2Gain, p.eq2Q, p.eq3Freq, p.eq3Gain, p.eq3Q,
                                 p.eq4Freq, p.eq4Gain, p.eq4Q, p.eq5Freq, p.eq5Gain, p.eq5Q]
    onDepsChanged: requestPaint()
    onWidthChanged: requestPaint()
    Connections { target: Theme; function onDarkChanged() { requestPaint() } }
    onHeightChanged: requestPaint()

    implicitHeight: 90

    function xOf(f) { return Math.log(f / 20) / Math.log(1000) * width }
    function fAt(x) { return 20 * Math.pow(1000, x / width) }
    function yOf(db) { return (1 - (db - minDb) / (maxDb - minDb)) * height }
    function dbAt(y) { return minDb + (1 - y / height) * (maxDb - minDb) }

    function coefs(type, f0, q, gain) {
        var fs = 48000, A = Math.pow(10, gain / 40), w = 2 * Math.PI * Math.min(f0, 21000) / fs
        var cw = Math.cos(w), sw = Math.sin(w), al = sw / (2 * q), s = 2 * Math.sqrt(A) * al
        var b0, b1, b2, a0, a1, a2
        if (type === "hp") { b0 = (1 + cw) / 2; b1 = -(1 + cw); b2 = b0; a0 = 1 + al; a1 = -2 * cw; a2 = 1 - al }
        else if (type === "peak") { b0 = 1 + al * A; b1 = -2 * cw; b2 = 1 - al * A; a0 = 1 + al / A; a1 = -2 * cw; a2 = 1 - al / A }
        else if (type === "ls") {
            b0 = A * ((A + 1) - (A - 1) * cw + s); b1 = 2 * A * ((A - 1) - (A + 1) * cw); b2 = A * ((A + 1) - (A - 1) * cw - s)
            a0 = (A + 1) + (A - 1) * cw + s; a1 = -2 * ((A - 1) + (A + 1) * cw); a2 = (A + 1) + (A - 1) * cw - s
        } else {
            b0 = A * ((A + 1) + (A - 1) * cw + s); b1 = -2 * A * ((A - 1) + (A + 1) * cw); b2 = A * ((A + 1) + (A - 1) * cw - s)
            a0 = (A + 1) - (A - 1) * cw + s; a1 = 2 * ((A - 1) - (A + 1) * cw); a2 = (A + 1) - (A - 1) * cw - s
        }
        return [b0 / a0, b1 / a0, b2 / a0, a1 / a0, a2 / a0]
    }
    function magDb(c, f) {
        var w = 2 * Math.PI * f / 48000, c1 = Math.cos(w), s1 = Math.sin(w), c2 = Math.cos(2 * w), s2 = Math.sin(2 * w)
        var nr = c[0] + c[1] * c1 + c[2] * c2, ni = -(c[1] * s1 + c[2] * s2)
        var dr = 1 + c[3] * c1 + c[4] * c2, di = -(c[3] * s1 + c[4] * s2)
        return 10 * Math.log((nr * nr + ni * ni) / (dr * dr + di * di)) / Math.LN10
    }

    onPaint: {
        var ctx = getContext("2d")
        ctx.reset()
        var W = width, H = height

        ctx.fillStyle = Theme.well
        ctx.fillRect(0, 0, W, H)
        ctx.strokeStyle = Theme.grid
        ctx.lineWidth = 1
        ;[50, 100, 200, 500, 1000, 2000, 5000, 10000].forEach(function(f) {
            ctx.beginPath(); ctx.moveTo(xOf(f), 0); ctx.lineTo(xOf(f), H); ctx.stroke()
        })
        if (showDbScale) {
            for (var g = Math.ceil(minDb / 6) * 6; g <= maxDb; g += 6) {
                ctx.beginPath(); ctx.moveTo(0, yOf(g)); ctx.lineTo(W, yOf(g)); ctx.stroke()
            }
        }
        ctx.strokeStyle = Theme.borderHi
        ctx.beginPath(); ctx.moveTo(0, yOf(0)); ctx.lineTo(W, yOf(0)); ctx.stroke()

        ctx.fillStyle = Theme.textFaint
        ctx.font = "9px sans-serif"
        ;[[100, "100"], [1000, "1k"], [10000, "10k"]].forEach(function(t) { ctx.fillText(t[1], xOf(t[0]) + 3, H - 4) })
        if (showDbScale) {
            for (var d = Math.ceil(minDb / 6) * 6; d <= maxDb; d += 6)
                if (d !== minDb) ctx.fillText((d > 0 ? "+" : "") + d, 3, yOf(d) - 2)
        }

        var filters = []
        if (p.bypass < 0.5) {
            if (includeLowCut && p.lowCutOn > 0.5) {
                if (p.lowCutSteep > 0.5) { filters.push(coefs("hp", p.lowCutFreq, 0.5412, 0)); filters.push(coefs("hp", p.lowCutFreq, 1.3066, 0)) }
                else filters.push(coefs("hp", p.lowCutFreq, 0.7071, 0))
            }
            if (includeEq && p.eqAdded > 0.5 && p.eqOn > 0.5) {
                var types = ["ls", "peak", "peak", "peak", "hs"]
                for (var b = 1; b <= 5; ++b)
                    filters.push(coefs(types[b - 1], p["eq" + b + "Freq"], p["eq" + b + "Q"], p["eq" + b + "Gain"]))
            }
            if (includeTone && p.toneOn > 0.5) {
                filters.push(coefs("ls", 120, 0.7071, p.toneBass))
                filters.push(coefs("peak", 4500, 0.9, p.tonePresence))
                filters.push(coefs("hs", 10000, 0.7071, p.toneAir))
            }
        }

        ctx.beginPath()
        for (var x = 0; x <= W; x += 2) {
            var f = fAt(x), db = 0
            for (var i = 0; i < filters.length; ++i) db += magDb(filters[i], f)
            var y = Math.max(0, Math.min(H, yOf(db)))
            if (x === 0) ctx.moveTo(x, y); else ctx.lineTo(x, y)
        }
        ctx.strokeStyle = accent
        ctx.lineWidth = 2
        ctx.stroke()
        ctx.lineTo(W, yOf(0)); ctx.lineTo(0, yOf(0)); ctx.closePath()
        ctx.fillStyle = Theme.alpha(accent, 0.15)
        ctx.fill()
    }
}
