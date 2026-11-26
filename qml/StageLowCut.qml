import QtQuick
import QtQuick.Layouts

StagePanel {
    title: "Low Cut"
    accent: Theme.lowcut
    enableParam: "lowCutOn"
    effectName: "lowCut"
    visualWidth: 340
    summary: "Removes deep rumble your voice doesn't need: desk bumps, PC fans, traffic and the boomy sound when you get close to the mic."
    help: "Also called a high-pass filter. Everything below the frequency fades out. Adult voices start around 80–100 Hz, so 80 Hz removes rumble without thinning your voice."

    visual: ResponseCurve {
        anchors.fill: parent
        anchors.bottomMargin: 120
        includeEq: false
        includeTone: false
        accent: Theme.lowcut
    }

    ParamSlider {
        paramKey: "lowCutFreq"; label: "Cut below"; from: 20; to: 300; logScale: true; stepSize: 1
        accent: Theme.lowcut
        format: (v) => Theme.fmtHz(v)
        help: "80 Hz is a safe start for speech. 100–120 Hz for a noisy room or if you're very close to the mic."
    }
    RowLayout {
        Layout.fillWidth: true
        spacing: 10
        ToggleSwitch { paramKey: "lowCutPlosive"; accent: Theme.lowcut; label: "Tame pops" }
        Text { text: "Tame “p” and “b” pops"; color: Theme.text; font.pixelSize: Theme.fsBody }
        HelpButton { text: "When a “p” or “b” hits the mic with a burst of deep bass, the bass is turned down for a moment (a few hundredths of a second). Your normal voice, even a deep one close to the mic, isn't affected. A pop filter or foam windscreen on the mic still helps most." }
    }
    ColumnLayout {
        Layout.fillWidth: true
        spacing: 6
        Text { text: "Steepness"; color: Theme.textDim; font.pixelSize: 12 }
        Segmented {
            Layout.fillWidth: true
            paramKey: "lowCutSteep"
            options: ["Gentle 12 dB", "Steep 24 dB"]
            accent: Theme.lowcut
        }
    }
}
