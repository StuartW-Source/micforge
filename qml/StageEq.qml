import QtQuick
import QtQuick.Layouts

StagePanel {
    id: root
    title: "Equaliser"
    accent: Theme.eq
    enableParam: "eqOn"
    effectName: "eq"
    visualWidth: 0
    summary: "Precise tone shaping with five adjustable points. Drag a point up to boost or down to cut that frequency, left/right to move it, and scroll on it to make it wider or narrower."
    help: "Point 1 is a low shelf (everything below it), point 5 is a high shelf (everything above it), points 2–4 are bells.\n\nUseful moves for voice:\n• Boomy/muddy: cut 2–4 dB around 200–400 Hz\n• Nasal/honky: cut a little around 800 Hz–1.2 kHz\n• Dull: boost 2–3 dB around 3–5 kHz\n\nSmall cuts usually sound better than big boosts."

    property int band: graph.selected
    readonly property var names: ["Low shelf", "Bell", "Bell", "Bell", "High shelf"]

    EqGraph {
        id: graph
        Layout.fillWidth: true
        Layout.columnSpan: root.columns
        Layout.preferredHeight: 260
        accent: Theme.eq
    }

    // Band selector chips.
    RowLayout {
        Layout.columnSpan: root.columns
        spacing: 8
        Text { text: "Point"; color: Theme.textDim; font.pixelSize: 12 }
        Repeater {
            model: 5
            Rectangle {
                width: 34; height: 28; radius: 14
                color: graph.selected === index + 1 ? graph.bandColors[index] : Theme.track
                Text {
                    anchors.centerIn: parent
                    text: index + 1
                    font.bold: true
                    color: graph.selected === index + 1 ? Theme.onAccent : Theme.textDim
                }
                MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: graph.selected = index + 1 }
            }
        }
        Text { text: "· " + root.names[graph.selected - 1]; color: Theme.textFaint; font.pixelSize: 12 }
        Item { Layout.fillWidth: true }
        PillButton {
            text: "Flatten all"
            implicitHeight: 28
            onClicked: { for (var b = 1; b <= 5; ++b) controller.setParam("eq" + b + "Gain", 0) }
        }
    }

    ParamSlider {
        paramKey: "eq" + root.band + "Gain"; label: "Boost / cut"; from: -18; to: 18; stepSize: 0.5; bipolar: true
        accent: graph.bandColors[root.band - 1]
        format: (v) => Theme.fmtDb(v)
    }
    ParamSlider {
        paramKey: "eq" + root.band + "Freq"; label: "Frequency"; from: 20; to: 20000; logScale: true; stepSize: 1
        accent: graph.bandColors[root.band - 1]
        format: (v) => Theme.fmtHz(v)
    }
    ParamSlider {
        paramKey: "eq" + root.band + "Q"; label: "Width (Q)"; from: 0.2; to: 10; logScale: true; stepSize: 0.01
        accent: graph.bandColors[root.band - 1]
        format: (v) => v.toFixed(2) + (v < 0.8 ? " · wide" : v > 3 ? " · narrow" : "")
        help: "Low Q = wide, gentle change over many frequencies. High Q = narrow, surgical change."
    }
}
