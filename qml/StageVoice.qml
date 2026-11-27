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
    help: "Pitch moves your voice up or down (in semitones: 12 = one octave). Voice size changes how big or small the speaker sounds, separately from pitch. Growl adds a rough rattle, Whisper makes it breathy, Tone tilts it darker or brighter. Robot adds a metallic buzz, Radio squeezes you into a small speaker, Drive adds grit, Wobble makes your voice waver.\n\nThe voice changer adds about 15 ms of delay, or about 37 ms when Voice size differs from Pitch or Whisper is on. Both are fine for chat. Switch the effect off when you want your normal voice back."

    ColumnLayout {
        Layout.fillWidth: true
        Layout.columnSpan: root.columns
        spacing: 8
        Repeater {
            model: Theme.voiceGroups
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 6
                Text { text: modelData.title; color: Theme.textDim; font.pixelSize: Theme.fsBody; font.bold: true }
                PresetChips {
                    Layout.fillWidth: true
                    presets: Theme.voicePresets
                    group: modelData.id
                    keys: controller.voiceKeys
                    accent: Theme.voice
                    big: true
                }
            }
        }
        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.sp2
            Text {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                color: Theme.textFaint; font.pixelSize: Theme.fsSmall
                text: !controller.shortcutsAvailable ? "Keys for voices need a desktop with system-wide shortcuts."
                    : !(controller.shortcutsEnabled && controller.voiceShortcutsEnabled)
                      ? "Switch voices with keys, even in a game: turn on “Shortcuts for voices” in Settings."
                      : "Press a voice's key to switch to it; press it again for your own voice."
                      + (controller.voiceKeys["off"] ? " Your own voice: " + controller.voiceKeys["off"] + "." : "")
            }
            PillButton {
                visible: controller.shortcutsEnabled && controller.voiceShortcutsEnabled
                text: "Set keys for voices…"
                implicitHeight: Theme.controlHeightSm
                onClicked: controller.configureShortcuts()
            }
        }
    }

    ParamSlider {
        paramKey: "vcPitch"; label: "Pitch"; from: -12; to: 12; stepSize: 0.5; bipolar: true
        accent: Theme.voice
        format: (v) => (v > 0 ? "+" : v < 0 ? "−" : "") + Math.abs(v).toFixed(1) + " st"
        help: "Semitones. +12 = one octave higher, −12 = one octave lower."
    }
    ParamSlider {
        paramKey: "vcFormant"; label: "Voice size · big ↔ small"; from: -12; to: 12; stepSize: 0.5; bipolar: true
        accent: Theme.voice
        format: (v) => Math.abs(v - controller.params.vcPitch) < 0.05 ? "follows pitch"
                     : (v < -0.05 ? "bigger " + Math.abs(v).toFixed(1) : v > 0.05 ? "smaller " + v.toFixed(1) : "your own")
        help: "How big the throat and head sound (the formants), separately from pitch. Higher pitch with a little smaller size sounds like a child or a woman rather than a chipmunk; lower and bigger sounds like an orc or a dragon.\n\nWhen it equals Pitch, size follows pitch (the classic effect, about 15 ms of delay). Otherwise the voice goes through a formant shifter, which adds about 37 ms."
    }
    ParamSlider {
        paramKey: "vcGrowl"; label: "Growl"; from: 0; to: 100; stepSize: 1
        accent: Theme.voice
        format: (v) => Math.round(v) + " %"
        help: "A rough, irregular rattle in the throat: monsters, orcs, dragons."
    }
    ParamSlider {
        paramKey: "vcWhisper"; label: "Whisper"; from: 0; to: 100; stepSize: 1
        accent: Theme.voice
        format: (v) => Math.round(v) + " %"
        help: "Breathy to fully whispered: ghosts, liches, secrets. Uses the formant shifter (about 37 ms of delay)."
    }
    ParamSlider {
        paramKey: "vcCharacter"; label: "Tone · darker ↔ brighter"; from: -12; to: 12; stepSize: 0.5; bipolar: true
        accent: Theme.voice
        format: (v) => v < -0.05 ? "darker " + Math.abs(v).toFixed(1) : v > 0.05 ? "brighter " + v.toFixed(1) : "neutral"
        help: "Tilts the sound darker (more bass) or brighter (more treble)."
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
