import QtQuick
import QtQuick.Layouts

StagePanel {
    id: root
    title: "Echo"
    accent: "#a3a8ff"
    enableParam: "echoOn"
    effectName: "echo"
    visualWidth: 0
    summary: "Repeats of your voice that fade away, from a quick vintage slapback to a huge canyon. Each repeat gets a little darker, like a classic tape echo."
    help: "Time = gap between repeats. Repeats (feedback) = how many times it bounces. Mix = how loud the repeats are. Tip: make sure the people you're talking to won't find it distracting!"

    ColumnLayout {
        Layout.fillWidth: true
        Layout.columnSpan: root.columns
        spacing: 8
        Text { text: "Presets"; color: Theme.textDim; font.pixelSize: 12; font.bold: true }
        PresetChips { Layout.fillWidth: true; presets: Theme.echoPresets; accent: "#a3a8ff" }
    }
    ParamSlider {
        paramKey: "echoTime"; label: "Time"; from: 20; to: 1500; logScale: true; stepSize: 1
        accent: "#a3a8ff"
        format: (v) => Theme.fmtMs(v)
    }
    ParamSlider {
        paramKey: "echoFeedback"; label: "Repeats"; from: 0; to: 90; stepSize: 1
        accent: "#a3a8ff"
        format: (v) => Math.round(v) + " %"
    }
    ParamSlider {
        paramKey: "echoMix"; label: "Mix"; from: 0; to: 100; stepSize: 1
        accent: "#a3a8ff"
        format: (v) => Math.round(v) + " %"
    }
    ParamSlider {
        paramKey: "echoTone"; label: "Repeat brightness"; from: 800; to: 16000; logScale: true; stepSize: 50
        accent: "#a3a8ff"
        format: (v) => Theme.fmtHz(v)
        help: "Lower = darker, softer repeats that stay out of the way."
    }
}
