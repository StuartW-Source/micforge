import QtQuick
import QtQuick.Layouts

StagePanel {
    id: root
    title: "Reverb"
    accent: "#b388ff"
    enableParam: "reverbOn"
    effectName: "reverb"
    visualWidth: 0
    summary: "Places your voice in a space, from a small room to a vast cathedral. A touch makes a dry mic sound natural; a lot is great for dramatic effects or singing."
    help: "Size = how big the space is (longer tail). Damping = how soft the walls are (more = darker, warmer). Mix = how much of the room you hear. For voice chat keep the mix low (10–20%) so you stay clear."

    ColumnLayout {
        Layout.fillWidth: true
        Layout.columnSpan: root.columns
        spacing: 8
        Text { text: "Presets"; color: Theme.textDim; font.pixelSize: 12; font.bold: true }
        PresetChips { Layout.fillWidth: true; presets: Theme.reverbPresets; accent: "#b388ff" }
    }
    ParamSlider {
        paramKey: "reverbSize"; label: "Size"; from: 0; to: 100; stepSize: 1
        accent: "#b388ff"
        format: (v) => Math.round(v) + " %"
    }
    ParamSlider {
        paramKey: "reverbDamp"; label: "Damping"; from: 0; to: 100; stepSize: 1
        accent: "#b388ff"
        format: (v) => Math.round(v) + " %"
    }
    ParamSlider {
        paramKey: "reverbMix"; label: "Mix"; from: 0; to: 100; stepSize: 1
        accent: "#b388ff"
        format: (v) => Math.round(v) + " %"
    }
}
