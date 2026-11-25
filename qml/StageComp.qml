import QtQuick
import QtQuick.Layouts

StagePanel {
    id: root
    title: "Compressor"
    accent: Theme.comp
    enableParam: "compOn"
    effectName: "comp"
    visualWidth: 200
    summary: "Evens out your volume. Loud moments get turned down so everything can be turned up: quiet words become clear and shouting won't blast anyone."
    help: "When your level goes above the threshold (yellow line), the compressor turns it down. Strength (ratio) decides how hard: at 3:1, going 9 dB over the line comes out only 3 dB over.\n\nThe SQUASH meter shows how much it's turning you down. 3–8 dB while talking is typical for voice chat."

    property real held: 0
    Connections {
        target: controller
        function onMetersChanged() { root.held = Math.max(controller.compReduction, root.held - 0.25) }
    }

    visual: RowLayout {
        anchors.fill: parent
        spacing: 10
        LevelMeter {
            Layout.fillHeight: true
            Layout.fillWidth: true
            level: controller.compLevel
            label: "LEVEL"
            accent: Theme.comp
            hasThreshold: true
            threshold: controller.params.compThreshold
            thresholdParam: "compThreshold"
        }
        ReductionMeter {
            Layout.fillHeight: true
            reduction: controller.compReduction
            maxDb: 24
            label: "SQUASH"
            accent: Theme.comp
        }
    }

    ColumnLayout {
        Layout.fillWidth: true
        Layout.columnSpan: root.columns
        spacing: 2
        Text { text: "Turning you down by"; color: Theme.textFaint; font.pixelSize: 11 }
        Text {
            text: root.held.toFixed(1) + " dB"
            color: Theme.comp
            font.pixelSize: 26; font.weight: Font.Bold; font.family: Theme.mono
        }
        Text {
            font.pixelSize: 12
            color: Theme.textDim
            text: root.held > 12 ? "Working very hard. Raise the threshold or lower the strength."
                : root.held > 2 ? "Working nicely."
                : "Barely working right now. Talk to see it move."
        }
    }
    ParamSlider {
        paramKey: "compThreshold"; label: "Threshold"; from: -60; to: 0; stepSize: 0.5
        accent: Theme.comp
        format: (v) => Theme.fmtDb(v)
        help: "Your voice is turned down only when it gets louder than this. Lower = it works more of the time."
    }
    ParamSlider {
        paramKey: "compRatio"; label: "Strength"; from: 1; to: 20; logScale: true; stepSize: 0.1
        accent: Theme.comp
        format: (v) => v.toFixed(1) + " : 1"
        help: "2:1 gentle · 3–4:1 typical voice · 6:1+ strong, radio-style."
    }
    ParamSlider {
        paramKey: "compMakeup"; label: "Volume boost"; from: 0; to: 30; stepSize: 0.5
        accent: Theme.comp
        format: (v) => Theme.fmtDb(v)
        help: "Turns everything back up after compression. Set it so the OUT meter peaks around −12 to −6 dB when you talk."
    }

    advanced: [
        ParamSlider {
            paramKey: "compAttack"; label: "Reaction time"; from: 0.1; to: 100; logScale: true; stepSize: 0.1
            accent: Theme.comp
            format: (v) => Theme.fmtMs(v)
            help: "How quickly it starts turning down (attack). 5–15 ms keeps speech punchy."
        },
        ParamSlider {
            paramKey: "compRelease"; label: "Recovery time"; from: 10; to: 1000; logScale: true; stepSize: 1
            accent: Theme.comp
            format: (v) => Theme.fmtMs(v)
            help: "How quickly it lets go after a loud moment (release). 100–250 ms is natural for speech."
        },
        ParamSlider {
            paramKey: "compKnee"; label: "Softness"; from: 0; to: 24; stepSize: 0.5
            accent: Theme.comp
            format: (v) => v.toFixed(1) + " dB"
            help: "Soft knee: eases in gradually around the threshold instead of switching on abruptly."
        }
    ]
}
