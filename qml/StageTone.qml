import QtQuick
import QtQuick.Layouts

StagePanel {
    title: "Tone"
    accent: Theme.tone
    enableParam: "toneOn"
    effectName: "tone"
    visualWidth: 340
    summary: "Three simple controls for the character of your voice. Small moves of ±3 dB go a long way."
    help: "Bass (120 Hz) = warmth and body. Presence (4.5 kHz) = clarity, cuts through game audio. Air (10 kHz) = crispness.\n\nThe curve shows the combined effect of Low Cut, Equaliser and Tone. Double-click a value to reset it."

    visual: ResponseCurve {
        anchors.fill: parent
        anchors.bottomMargin: 120
        accent: Theme.tone
    }

    ParamSlider {
        paramKey: "toneBass"; label: "Bass · warmth"; from: -12; to: 12; stepSize: 0.5; bipolar: true
        accent: Theme.tone
        format: (v) => Theme.fmtDb(v)
    }
    ParamSlider {
        paramKey: "tonePresence"; label: "Presence · clarity"; from: -12; to: 12; stepSize: 0.5; bipolar: true
        accent: Theme.tone
        format: (v) => Theme.fmtDb(v)
    }
    ParamSlider {
        paramKey: "toneAir"; label: "Air · crispness"; from: -12; to: 12; stepSize: 0.5; bipolar: true
        accent: Theme.tone
        format: (v) => Theme.fmtDb(v)
    }
}
