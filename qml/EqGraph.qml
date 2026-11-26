import QtQuick
import QtQuick.Controls

// Parametric EQ graph: drag a numbered point left/right for frequency, up/down for boost/cut,
// scroll on it to make it wider/narrower. Double-click a point to reset it to 0 dB.
Item {
    id: root
    property color accent: Theme.tone
    property int selected: 3
    readonly property var p: controller.params
    readonly property var bandColors: [Theme.comp, Theme.tone, Theme.reverb, Theme.input, Theme.output]

    implicitHeight: 260

    ResponseCurve {
        id: curve
        anchors.fill: parent
        accent: root.accent
        includeLowCut: false
        includeTone: false
        minDb: -18
        maxDb: 18
        showDbScale: true
    }

    Repeater {
        model: 5
        Item {
            id: handle
            readonly property int band: index + 1
            readonly property real freq: root.p["eq" + band + "Freq"]
            readonly property real gain: root.p["eq" + band + "Gain"]
            width: 26; height: 26
            x: curve.xOf(freq) - width / 2
            y: curve.yOf(gain) - height / 2

            Rectangle {
                anchors.centerIn: parent
                width: root.selected === handle.band ? 24 : 20
                height: width; radius: width / 2
                color: root.bandColors[index]
                border.color: Theme.onAccent
                border.width: 2
                Text { anchors.centerIn: parent; text: handle.band; color: Theme.onAccent; font.pixelSize: 11; font.bold: true }
            }
            MouseArea {
                anchors.fill: parent
                anchors.margins: -4
                cursorShape: Qt.SizeAllCursor
                preventStealing: true
                hoverEnabled: true
                onPressed: root.selected = handle.band
                onPositionChanged: (mouse) => {
                    if (!pressed) return
                    var pt = mapToItem(curve, mouse.x, mouse.y)
                    var f = Math.max(20, Math.min(20000, curve.fAt(Math.max(0, Math.min(curve.width, pt.x)))))
                    var g = Math.max(-18, Math.min(18, curve.dbAt(pt.y)))
                    controller.setParam("eq" + handle.band + "Freq", Math.round(f))
                    controller.setParam("eq" + handle.band + "Gain", Math.round(g * 2) / 2)
                }
                onDoubleClicked: controller.setParam("eq" + handle.band + "Gain", 0)
                onWheel: (wheel) => {
                    var key = "eq" + handle.band + "Q"
                    var q = root.p[key] * (wheel.angleDelta.y > 0 ? 1.12 : 1 / 1.12)
                    controller.setParam(key, Math.round(q * 100) / 100)
                }
                ToolTip.visible: containsMouse && !pressed
                ToolTip.delay: 600
                ToolTip.text: "Drag to move · scroll to change width · double-click to flatten"
            }
        }
    }
}
