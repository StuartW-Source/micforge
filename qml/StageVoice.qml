import QtQuick
import QtQuick.Layouts

StagePanel {
    id: root
    title: "Voice Changer"
    accent: Theme.voice
    enableParam: "vcOn"
    effectName: "vc"
    visualWidth: 0
    summary: "Turn your voice into something else entirely. Pick a preset, then tweak it. Great for games, streams and making friends laugh. Click Listen at the top to hear yourself."
    help: "Pitch moves your voice up or down (in semitones: 12 = one octave). Character changes how big or small you sound. Robot adds a metallic buzz, Radio squeezes you into a small speaker, Drive adds grit, Wobble makes your voice waver.\n\nThe pitch shifter adds about 30 ms of delay, which is fine for chat. Switch the effect off when you want your normal voice back."

    ColumnLayout {
        Layout.fillWidth: true
        Layout.columnSpan: root.columns
        spacing: 8
        Text { text: "Presets"; color: Theme.textDim; font.pixelSize: 12; font.bold: true }
        PresetChips {
            Layout.fillWidth: true
            presets: Theme.voicePresets
            accent: Theme.voice
            big: true
        }
    }

    ParamSlider {
        paramKey: "vcPitch"; label: "Pitch"; from: -12; to: 12; stepSize: 0.5; bipolar: true
        accent: Theme.voice
        format: (v) => (v > 0 ? "+" : v < 0 ? "−" : "") + Math.abs(v).toFixed(1) + " st"
        help: "Semitones. +12 = one octave higher, −12 = one octave lower."
    }
    ParamSlider {
        paramKey: "vcCharacter"; label: "Character · big ↔ small"; from: -12; to: 12; stepSize: 0.5; bipolar: true
        accent: Theme.voice
        format: (v) => v < -0.05 ? "bigger " + Math.abs(v).toFixed(1) : v > 0.05 ? "smaller " + v.toFixed(1) : "neutral"
        help: "Negative sounds like a bigger, darker throat; positive sounds smaller and brighter. Combine with Pitch for cartoon voices."
    }
    ParamSlider {
        paramKey: "vcRobot"; label: "Robot"; from: 0; to: 100; stepSize: 1
        accent: Theme.voice
        format: (v) => Math.round(v) + " %"
    }
    ParamSlider {
        paramKey: "vcRadio"; label: "Radio / phone"; from: 0; to: 100; stepSize: 1
        accent: Theme.voice
        format: (v) => Math.round(v) + " %"
    }
    ParamSlider {
        paramKey: "vcDrive"; label: "Drive · grit"; from: 0; to: 100; stepSize: 1
        accent: Theme.voice
        format: (v) => Math.round(v) + " %"
    }
    ParamSlider {
        paramKey: "vcWobble"; label: "Wobble"; from: 0; to: 100; stepSize: 1
        accent: Theme.voice
        format: (v) => Math.round(v) + " %"
    }

    advanced: [
        ParamSlider {
            paramKey: "vcRobotFreq"; label: "Robot tone"; from: 10; to: 1000; logScale: true; stepSize: 1
            accent: Theme.voice
            format: (v) => Theme.fmtHz(v)
            help: "Low (30–80 Hz) = buzzy robot; high (300–1000 Hz) = ringing, bell-like alien."
        },
        ParamSlider {
            paramKey: "vcWobbleRate"; label: "Wobble speed"; from: 0.5; to: 12; logScale: true; stepSize: 0.1
            accent: Theme.voice
            format: (v) => v.toFixed(1) + " Hz"
        },
        ParamSlider {
            paramKey: "vcMix"; label: "Mix with your real voice"; from: 0; to: 100; stepSize: 1
            accent: Theme.voice
            format: (v) => Math.round(v) + " % effect"
            help: "100% = only the changed voice. Lower blends your real voice back in (sounds chorus-like with pitch changes)."
        }
    ]
}
