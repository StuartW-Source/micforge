import QtQuick
import QtQuick.Layouts

StagePanel {
    title: "Warmth"
    accent: Theme.warmth
    enableParam: "warmthOn"
    effectName: "warmth"
    visualWidth: 0
    summary: "Analogue-style saturation, like a vintage tube preamp or tape machine. It rounds off sharp peaks and adds rich harmonics, so your voice sounds fuller and more 'radio'."
    help: "Low amounts (10–30%) add subtle richness most people feel rather than hear. High amounts audibly colour and thicken the sound. Quiet sounds pass almost untouched; only louder moments get saturated."

    ParamSlider {
        paramKey: "warmthAmount"; label: "Amount"; from: 0; to: 100; stepSize: 1
        accent: Theme.warmth
        format: (v) => Math.round(v) + " %"
    }
}
