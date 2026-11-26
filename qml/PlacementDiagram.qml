import QtQuick

// Top-down sketch: a cardioid mic pointing at you, its cardioid pickup pattern, and the
// "deaf spot" behind it pointed at other people.
Canvas {
    id: root
    implicitHeight: 170
    onWidthChanged: requestPaint()
    Connections { target: Theme; function onDarkChanged() { requestPaint() } }
    onHeightChanged: requestPaint()

    onPaint: {
        var ctx = getContext("2d")
        ctx.reset()
        var W = width, H = height, cy = H / 2
        var micX = W * 0.40, youX = W * 0.13, othersX = W * 0.86
        var R = Math.min(W * 0.30, H * 0.95)

        // Cardioid pickup pattern, pointing left (towards you).
        ctx.beginPath()
        for (var i = 0; i <= 120; ++i) {
            var th = i / 120 * 2 * Math.PI
            var r = R * 0.5 * (1 + Math.cos(th - Math.PI))
            var x = micX + r * Math.cos(th), y = cy + r * Math.sin(th) * 0.8
            if (i === 0) ctx.moveTo(x, y); else ctx.lineTo(x, y)
        }
        ctx.closePath()
        ctx.fillStyle = Theme.alpha(Theme.gate, 0.14)
        ctx.fill()
        ctx.strokeStyle = Theme.alpha(Theme.gate, 0.7)
        ctx.setLineDash([4, 4])
        ctx.lineWidth = 1.5
        ctx.stroke()
        ctx.setLineDash([])

        // Microphone: grille on the left, body + cable to the right.
        ctx.fillStyle = Theme.micGrille
        ctx.beginPath(); ctx.arc(micX - 2, cy, 9, 0, 2 * Math.PI); ctx.fill()
        ctx.fillStyle = Theme.micBody
        ctx.fillRect(micX + 4, cy - 6, 44, 12)
        ctx.strokeStyle = Theme.textFaint; ctx.lineWidth = 2
        ctx.beginPath(); ctx.moveTo(micX + 48, cy); ctx.bezierCurveTo(micX + 70, cy, micX + 70, cy + 30, micX + 90, cy + 34); ctx.stroke()

        // You.
        ctx.fillStyle = Theme.gate
        ctx.beginPath(); ctx.arc(youX, cy, 20, 0, 2 * Math.PI); ctx.fill()
        ctx.fillStyle = Theme.onAccent; ctx.font = "bold 12px sans-serif"; ctx.textAlign = "center"
        ctx.fillText("YOU", youX, cy + 4)

        // Other people.
        ctx.fillStyle = Theme.tone
        ctx.beginPath(); ctx.arc(othersX, cy, 20, 0, 2 * Math.PI); ctx.fill()
        ctx.fillStyle = Theme.onAccent
        ctx.font = "bold 10px sans-serif"
        ctx.fillText("OTHERS", othersX, cy + 4)

        // Labels.
        ctx.fillStyle = Theme.textDim; ctx.font = "11px sans-serif"
        ctx.fillText("2–5 cm", (youX + micX) / 2, cy - 26)
        ctx.strokeStyle = Theme.textDim; ctx.lineWidth = 1
        ctx.beginPath(); ctx.moveTo(youX + 24, cy - 18); ctx.lineTo(micX - 14, cy - 18); ctx.stroke()
        ctx.fillStyle = Theme.tone
        ctx.fillText("back of mic = deaf spot", (micX + othersX) / 2 + 20, cy - 30)
        ctx.strokeStyle = Theme.tone
        ctx.beginPath(); ctx.moveTo(micX + 50, cy - 18); ctx.lineTo(othersX - 26, cy - 18); ctx.stroke()
        ctx.fillStyle = Theme.gate
        ctx.fillText("picks up here", micX - R * 0.55, cy + R * 0.48)
    }
}
