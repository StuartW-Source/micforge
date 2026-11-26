import QtQuick
import QtQuick.Layouts

StagePanel {
    title: "Exciter"
    accent: Theme.exciter
    enableParam: "exciterOn"
    effectName: "exciter"
    visualWidth: 0
    summary: "Adds sparkle and clarity by generating fresh high harmonics from your voice, not just boosting treble. It helps you cut through game audio and makes duller mics sound more expensive."
    help: "Different from an EQ treble boost: an exciter creates new detail above the frequency you choose, so it adds clarity without making background hiss louder. Keep it subtle: 15–35%."

    ParamSlider {
        paramKey: "exciterAmount"; label: "Amount"; from: 0; to: 100; stepSize: 1
        accent: Theme.exciter
        format: (v) => Math.round(v) + " %"
    }
    ParamSlider {
        paramKey: "exciterFreq"; label: "Starting from"; from: 1500; to: 10000; logScale: true; stepSize: 50
        accent: Theme.exciter
        format: (v) => Theme.fmtHz(v)
        help: "Lower (2–3 kHz) = more presence and bite. Higher (6–10 kHz) = airy sparkle."
    }
}
