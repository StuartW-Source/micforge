import QtQuick
import QtQuick.Layouts

StagePanel {
    id: root
    title: "De-esser"
    accent: Theme.deess
    enableParam: "deessOn"
    effectName: "deess"
    visualWidth: 160
    summary: "Softens harsh, hissy “S”, “T” and “SH” sounds, only at the moment they happen. The rest of your voice stays untouched."
    help: "It watches the high frequencies. When they jump above the threshold (as they do on 'S' sounds), it briefly turns down just the top end. Compression and presence boosts often make esses harsher, which is why this sits after the compressor."

    property real held: 0
    Connections {
        target: controller
        function onMetersChanged() { root.held = Math.max(controller.deessReduction, root.held - 0.3) }
    }

    visual: RowLayout {
        anchors.fill: parent
        spacing: 14
        ReductionMeter {
            Layout.fillHeight: true
            Layout.preferredWidth: 36
            reduction: controller.deessReduction
            maxDb: 24
            label: "TAMED"
            accent: Theme.deess
        }
        ColumnLayout {
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignTop
            spacing: 2
            Text { text: "Taming by"; color: Theme.textFaint; font.pixelSize: 11 }
            Text { text: root.held.toFixed(1) + " dB"; color: Theme.deess; font.pixelSize: 22; font.weight: Font.Bold; font.family: Theme.mono }
            Text {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                color: Theme.textFaint; font.pixelSize: 11
                text: "Say “Sally sells sea shells” and watch it react to each S."
            }
        }
    }

    ParamSlider {
        paramKey: "deessThreshold"; label: "Sensitivity (threshold)"; from: -60; to: 0; stepSize: 0.5
        accent: Theme.deess
        format: (v) => Theme.fmtDb(v)
        help: "Lower = reacts to quieter S sounds. Set it so it only moves on S/T sounds, not on normal vowels."
    }
    ParamSlider {
        paramKey: "deessMax"; label: "Max reduction"; from: 0; to: 24; stepSize: 0.5
        accent: Theme.deess
        format: (v) => v.toFixed(1) + " dB"
        help: "The most it will turn the esses down. 4–8 dB is usually plenty; more can make you sound lispy."
    }
    ParamSlider {
        paramKey: "deessFreq"; label: "Frequency"; from: 2500; to: 12000; logScale: true; stepSize: 50
        accent: Theme.deess
        format: (v) => Theme.fmtHz(v)
        help: "Where the harshness lives. Most voices: 5–8 kHz. Lower voices often sit nearer 5 kHz."
    }
}
