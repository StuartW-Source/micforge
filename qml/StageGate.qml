import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

StagePanel {
    id: root
    signal setupRequested()
    signal learnRequested()

    title: "Voice Gate"
    accent: Theme.gate
    enableParam: "gateOn"
    effectName: "gate"
    visualWidth: 200
    summary: "Opens only for YOUR voice. It listens to the frequencies where your close-up voice is strongest, checks the sound is as close to the mic as you are, and stays closed for other people in the room, keyboards and fans."
    help: "The meter shows what the gate hears in its listening band. Above the yellow line the gate OPENS. Below the dashed line it waits (hold), then fades out (close time).\n\nShaded zones come from Setup: grey = room noise, pink = other voices, green = your voice. A good setup has the yellow line sitting in the gap between pink and green."

    readonly property var p: controller.params
    readonly property var zones: controller.zones
    readonly property bool expander: p.gateMode > 0.5

    visual: RowLayout {
        anchors.fill: parent
        spacing: 8
        LevelMeter {
            Layout.fillHeight: true
            Layout.fillWidth: true
            level: controller.gateLevel
            label: "GATE HEARS"
            accent: Theme.gate
            minDb: -80
            hasThreshold: true
            threshold: root.p.gateThreshold
            secondaryThreshold: root.p.gateThreshold - root.p.gateHysteresis
            thresholdParam: "gateThreshold"
            zones: root.zones
            zoneLabels: true
        }
        ReductionMeter {
            Layout.fillHeight: true
            reduction: controller.gateReduction
            maxDb: 60
            label: "CUT"
            accent: Qt.darker(Theme.gate, 1.5)
        }
    }

    // Live status + voice-setup summary.
    RowLayout {
        Layout.fillWidth: true
        Layout.columnSpan: root.columns
        spacing: 14
        Rectangle {
            id: gateLight
            Layout.preferredWidth: root.width < 900 ? 130 : 170
            implicitHeight: 64
            radius: 12
            property bool open: controller.gateOpen && root.active
            property bool sticky: false
            Timer { id: stickyTimer; interval: 120; onTriggered: gateLight.sticky = false }
            onOpenChanged: { if (open) gateLight.sticky = true; stickyTimer.restart() }
            readonly property bool lit: open || sticky
            color: lit ? Theme.alpha(Theme.gate, 0.22) : Theme.well
            border.color: lit ? Theme.gate : Theme.border
            Behavior on color { ColorAnimation { duration: 80 } }
            Column {
                anchors.centerIn: parent
                spacing: 2
                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: !root.active ? "OFF" : gateLight.lit ? "OPEN" : "CLOSED"
                    color: gateLight.lit ? Theme.gate : Theme.textFaint
                    font.pixelSize: 20; font.weight: Font.Black
                }
                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: !root.active ? "gate bypassed"
                        : gateLight.lit ? (root.p.gateCloseCheck > 0.5 ? "loud + close enough" : "loud enough")
                        : controller.gateReason === "far" ? "not close enough"
                        : controller.gateReason === "notvoice" ? "not a voice"
                        : controller.gateReason === "noise" ? "background noise"
                        : "too quiet"
                    color: Theme.textFaint
                    font.pixelSize: 11
                }
            }
        }
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: 64
            radius: 12
            color: Theme.alpha(Theme.gate, 0.06)
            border.color: Theme.border
            RowLayout {
                anchors { fill: parent; leftMargin: 14; rightMargin: 10 }
                spacing: 10
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2
                    Text {
                        text: root.zones.valid ? (root.zones.others > root.zones.room ? "Tuned to your voice vs. other voices" : "Tuned to your voice")
                                               : "Not tuned yet"
                        color: Theme.text; font.pixelSize: 13; font.weight: Font.Bold
                    }
                    Text {
                        Layout.fillWidth: true
                        elide: Text.ElideRight
                        color: Theme.textDim; font.pixelSize: 11
                        text: !root.zones.valid ? "Run Setup so the gate learns your voice."
                            : "Listening " + (root.p.gateFocus > 0.5 ? (root.p.gateScLow > 21 ? Theme.fmtHz(root.p.gateScLow) + " – " : "below ") + Theme.fmtHz(root.p.gateScHigh) : "to the full range")
                              + (root.zones.others > root.zones.room ? " · gap to other voices " + (root.zones.you - root.zones.others).toFixed(0) + " dB" : "")
                    }
                }
                PillButton {
                    visible: controller.hasRecordings && root.p.needOthers > 0.5
                    text: "Learn other voices"
                    icon: "+"
                    accent: Theme.tone
                    onClicked: root.learnRequested()
                }
                PillButton {
                    text: root.zones.valid ? "Re-run" : "Setup"
                    icon: "✦"
                    primary: !root.zones.valid
                    accent: Theme.gate
                    onClicked: root.setupRequested()
                }
            }
        }
    }

    // Protection against loud other voices: loudness margin + closeness check.
    RowLayout {
        Layout.fillWidth: true
        Layout.fillHeight: false
        Layout.preferredHeight: 112
        Layout.maximumHeight: 112
        Layout.columnSpan: root.columns
        spacing: 14

        Rectangle {
            id: margin
            Layout.preferredWidth: root.width < 900 ? 200 : 260
            Layout.fillHeight: true
            implicitHeight: 110
            radius: 12
            color: Theme.well
            border.color: Theme.border
            readonly property bool known: root.zones.valid === true && root.zones.others > root.zones.room
            readonly property real db: known ? root.p.gateThreshold - root.zones.others : 0
            readonly property color c: db >= 12 ? Theme.gate : db >= 6 ? Theme.warn : Theme.danger
            ColumnLayout {
                anchors { fill: parent; margins: 12 }
                spacing: 2
                Text { text: "Loudness margin"; color: Theme.textDim; font.pixelSize: 11 }
                Text {
                    text: margin.known ? (margin.db >= 0 ? "+" : "") + margin.db.toFixed(0) + " dB" : "—"
                    color: margin.known ? margin.c : Theme.textFaint
                    font.pixelSize: 24; font.weight: Font.Bold; font.family: Theme.mono
                }
                Text {
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    maximumLineCount: 3
                    elide: Text.ElideRight
                    color: Theme.textFaint; font.pixelSize: 11
                    text: !margin.known ? "Use “Learn other voices” while someone is talking nearby to measure this."
                        : root.p.gateCloseCheck > 0.5 ? "How much louder other voices can get on volume alone. The closeness check blocks them beyond that."
                        : "How much louder than normal other voices can get before they open your gate."
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            implicitHeight: closeCol.implicitHeight + 24
            radius: 12
            color: Theme.well
            border.color: root.p.gateCloseCheck > 0.5 ? Theme.alpha(Theme.gate, 0.5) : Theme.border
            ColumnLayout {
                id: closeCol
                anchors { left: parent.left; right: parent.right; top: parent.top; margins: 12 }
                spacing: 8
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8
                    Text { text: "Closeness check"; color: Theme.text; font.pixelSize: 13; font.weight: Font.Bold }
                    HelpButton { text: "A voice close to the mic gets a big bass boost (the proximity effect). Voices from across the room don't, and getting louder doesn't change that. So the gate also checks that a sound is as close and bass-heavy as YOU before opening. This keeps other people out even when they laugh or shout.\n\nSetup measures your voice (and other voices, if recorded) and sets this for you. The white dot shows the sound happening right now." }
                    Item { Layout.fillWidth: true }
                    Text {
                        text: root.p.gateCloseCheck > 0.5 ? "ON" : "OFF"
                        color: root.p.gateCloseCheck > 0.5 ? Theme.gate : Theme.textFaint
                        font.pixelSize: 11; font.bold: true
                    }
                    ToggleSwitch { paramKey: "gateCloseCheck"; accent: Theme.gate }
                }
                ClosenessBar {
                    Layout.fillWidth: true
                    live: controller.gateTilt
                    threshold: root.p.gateTilt
                    enabledCheck: root.p.gateCloseCheck > 0.5
                    hasMarks: root.zones.valid === true
                    hasOthersMark: root.zones.valid === true && root.zones.othersTilt > -100
                    youMark: root.zones.youTilt || 0
                    othersMark: root.zones.othersTilt || 0
                }
            }
        }
    }

    ColumnLayout {
        Layout.fillWidth: true
        Layout.columnSpan: root.columns
        spacing: 6
        // Why the gate is doing what it does, in one plain sentence.
        Text {
            Layout.fillWidth: true
            Layout.bottomMargin: 6
            visible: root.active
            wrapMode: Text.WordWrap
            color: controller.gateReason === "far" || controller.gateReason === "noise" ? Theme.text : Theme.textDim
            font.pixelSize: 12
            text: {
                var thr = Theme.fmtDb(controller.gateOpenAt > -119 ? controller.gateOpenAt : root.p.gateThreshold)
                // Open/closed comes from the same light as the box above; the reason explains a closed gate.
                switch (gateLight.lit ? "open" : controller.gateReason) {
                case "open": return "Why it's open: the sound is above " + thr + (root.p.gateCloseCheck > 0.5 ? " and sounds close to the mic." : ".")
                case "far": return "Why it's closed: loud enough, but it doesn't sound close to the mic (another voice, or a speaker)."
                case "notvoice": return "Why it's closed: loud enough, but the AI voice detector doesn't hear speech (typing, clicks, music…)."
                case "noise": return "Why it's closed: steady background noise at about " + Theme.fmtDb(controller.gateFloor) + ", so the gate now opens above " + thr + " instead of " + Theme.fmtDb(root.p.gateThreshold) + "."
                default: return "Why it's closed: nothing is above " + thr + "."
                    + (controller.gateFloor > -119 && root.p.gateAdaptive > 0.5 ? " Background noise: about " + Theme.fmtDb(controller.gateFloor) + "." : "")
                }
            }
        }
        RowLayout {
            Layout.fillWidth: true
            Layout.bottomMargin: 6
            spacing: 10
            ToggleSwitch { paramKey: "gateAdaptive"; accent: Theme.gate; label: "Ignore steady background noise" }
            Text { text: "Ignore steady background noise"; color: Theme.text; font.pixelSize: 12 }
            HelpButton { text: "If a fan, air conditioning or other steady noise gets louder than the gate's threshold, the gate raises its threshold just above it, so the noise stays out and your voice still opens it. It only reacts to sound that stays steady in the gaps between your words for a few seconds, so your voice is never mistaken for noise." }
        }
        RowLayout {
            Layout.fillWidth: true
            Layout.bottomMargin: 6
            spacing: 10
            opacity: controller.noiseRemovalAvailable ? 1 : 0.5
            ToggleSwitch { paramKey: "gateVad"; accent: Theme.gate; enabled: controller.noiseRemovalAvailable; label: "Only open for speech" }
            Text { text: "Only open for speech (AI voice detector)"; color: Theme.text; font.pixelSize: 12 }
            HelpButton { text: "The gate also asks the AI voice detector (the one Noise Removal uses) whether it hears speech. Typing, clicks, music and other non-speech sounds stay out even when they're loud. It doesn't tell your voice from other people's (that's the closeness check's job).\n\nIt adds 20 ms of delay, and for about a second after you stop talking it still counts as speech, so a keystroke right after a sentence can slip through." }
            Text { visible: !controller.noiseRemovalAvailable; text: "needs rnnoise"; color: Theme.textFaint; font.pixelSize: 11 }
        }
        ParamSlider {
            visible: root.p.gateVad > 0.5 && controller.noiseRemovalAvailable
            Layout.fillWidth: true
            Layout.maximumWidth: 460
            Layout.bottomMargin: 6
            paramKey: "gateVadThreshold"; label: "Speech certainty needed"; from: 10; to: 99; stepSize: 1
            accent: Theme.gate
            help: "How sure the detector must be that it hears speech. Higher keeps out more typing and noise; if the start of your words gets cut, lower it."
            format: function(v) { return Math.round(v) + "%" }
        }
        RowLayout {
            Layout.fillWidth: true
            Layout.bottomMargin: 6
            spacing: 10
            ToggleSwitch { paramKey: "breathOn"; accent: Theme.gate; label: "Reduce breaths" }
            Text { text: "Reduce breaths (experimental)"; color: Theme.text; font.pixelSize: 12 }
            HelpButton { text: "Turns down breaths between your sentences. It waits until a sound has been breath-like (no pitch, no hiss, well below your voice) for about a sixth of a second, so short sounds inside words (h, f, t) are never touched, but the start of each breath still passes.\n\nWhispering has no pitch either: leave this off if you whisper." }
        }
        ParamSlider {
            visible: root.p.breathOn > 0.5
            Layout.fillWidth: true
            Layout.maximumWidth: 460
            Layout.bottomMargin: 6
            paramKey: "breathAmount"; label: "Turn breaths down by"; from: 3; to: 30; stepSize: 1
            accent: Theme.gate
            format: function(v) { return Math.round(v) + " dB" }
        }
        Text { text: "When you're not talking"; color: Theme.textDim; font.pixelSize: 12 }
        Segmented {
            Layout.fillWidth: true
            Layout.maximumWidth: 460
            paramKey: "gateMode"
            options: ["Mute (gate)", "Turn down softly (expander)"]
            accent: Theme.gate
        }
    }
    ParamSlider {
        paramKey: "gateThreshold"; label: "Opens above"; from: -80; to: 0; stepSize: 0.5
        accent: Theme.gate
        format: (v) => Theme.fmtDb(v)
        help: "The level your voice must reach (in the listening band) to open the gate. You can also drag the yellow line on the meter. Setup puts it in the gap between other sounds and you."
    }
    ParamSlider {
        paramKey: "gateRange"; label: "Noise reduction"; from: 0; to: -80; stepSize: 1
        accent: Theme.gate
        format: (v) => v <= -79.5 ? "Full mute" : Theme.fmtDb(v, 0)
        help: "The most it will turn things down when you're not talking. −30 to −40 dB keeps other people effectively silent but sounds natural. Full mute is total silence."
    }
    ParamSlider {
        paramKey: "gateRelease"; label: "Close time"; from: 10; to: 2000; logScale: true; stepSize: 1
        accent: Theme.gate
        format: (v) => Theme.fmtMs(v)
        help: "How slowly it fades out after you stop. Too short sounds choppy and cuts word endings; 150–400 ms is natural."
    }
    ParamSlider {
        visible: root.expander
        paramKey: "gateRatio"; label: "Softness"; from: 1.2; to: 10; logScale: true; stepSize: 0.1
        accent: Theme.gate
        format: (v) => "1 : " + v.toFixed(1)
        help: "Expander mode: for every 1 dB below the line, the sound drops by this many dB. Low = gentle and natural, high = close to a gate."
    }

    advanced: [
        ParamSlider {
            enabled: root.p.gateCloseCheck > 0.5
            paramKey: "gateTilt"; label: "Closeness needed"; from: -15; to: 40; stepSize: 0.5
            accent: Theme.gate
            format: (v) => v.toFixed(1) + " dB"
            help: "How bass-heavy (close) a sound must be to open the gate: the yellow line on the closeness bar. Raise it if other voices still get through when loud. Lower it if your own quieter words get cut."
        },
        Item { visible: root.columns > 1; implicitHeight: 1 },
        ParamSlider {
            paramKey: "gateAttack"; label: "Open time"; from: 0.1; to: 100; logScale: true; stepSize: 0.1
            accent: Theme.gate
            format: (v) => Theme.fmtMs(v)
            help: "How fast the gate opens. Keep it short (0.5–5 ms) with look-ahead so word starts aren't lost."
        },
        ParamSlider {
            paramKey: "gateLookahead"; label: "Look-ahead"; from: 0; to: 10; stepSize: 0.5
            accent: Theme.gate
            format: (v) => v.toFixed(1) + " ms"
            help: "Delays your audio by this much so the gate can open just before a word starts. 3–5 ms stops the first syllable being clipped and nobody will notice the delay."
        },
        ParamSlider {
            paramKey: "gateHold"; label: "Hold"; from: 0; to: 1000; stepSize: 5
            accent: Theme.gate
            format: (v) => Theme.fmtMs(v)
            help: "How long it stays open after your voice drops, so it doesn't flutter in short pauses between words."
        },
        ParamSlider {
            paramKey: "gateHysteresis"; label: "Close margin"; from: 0; to: 20; stepSize: 0.5
            accent: Theme.gate
            format: (v) => v.toFixed(1) + " dB"
            help: "The gate closes this far BELOW the open line (the dashed line). Keeps it from flickering. Keep the dashed line above the pink 'Others' zone."
        },
        RowLayout {
            Layout.fillWidth: true
            Text { text: "Listen only to a band"; color: Theme.textDim; font.pixelSize: 12; Layout.fillWidth: true }
            HelpButton { text: "The gate decides using only this frequency range. A low band (e.g. below 150–300 Hz) favours a close-up voice over voices further away. Setup picks the best band automatically. This doesn't change how you sound." }
            ToggleSwitch { paramKey: "gateFocus"; accent: Theme.gate }
        },
        Item { visible: root.columns > 1; implicitHeight: 1 },
        ParamSlider {
            enabled: root.p.gateFocus > 0.5
            paramKey: "gateScLow"; label: "Listen from"; from: 20; to: 500; logScale: true; stepSize: 1
            accent: Theme.gate
            format: (v) => v <= 21 ? "lowest" : Theme.fmtHz(v)
        },
        ParamSlider {
            enabled: root.p.gateFocus > 0.5
            paramKey: "gateScHigh"; label: "Listen up to"; from: 150; to: 8000; logScale: true; stepSize: 5
            accent: Theme.gate
            format: (v) => v >= 7900 ? "highest" : Theme.fmtHz(v)
        }
    ]
}
