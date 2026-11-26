import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Guided setup:  Your setup -> Placement -> Quiet room -> [Other voices] -> You talk -> Result
// Also hosts "Learn other voices" (learnPrep -> learn -> Result), which refines a saved setup.
Popup {
    id: root
    modal: true
    anchors.centerIn: Overlay.overlay
    width: Math.min(860, parent ? parent.width - 40 : 860)
    padding: 30
    readonly property string phase: controller.calibPhase
    readonly property var res: controller.calibResult
    readonly property var p: controller.params
    readonly property bool running: phase !== "" && phase !== "done"
    readonly property bool learning: phase === "learnPrep" || phase === "learn" || (res.learning === true && (phase === "analysing" || phase === "done"))
    readonly property bool needOthers: p.needOthers > 0.5
    readonly property int mic: Math.round(p.micType)
    property string page: "profile"     // before recording starts: "profile" or "placement"

    closePolicy: running ? Popup.NoAutoClose : Popup.CloseOnEscape
    onOpened: if (phase === "") page = "profile"
    onClosed: controller.cancelCalibration()
    background: Rectangle { color: Theme.panel; radius: 18; border.color: Theme.border }
    Overlay.modal: Rectangle { color: Theme.scrim }

    function openLearn() { controller.startLearnOthers(); open() }

    readonly property var steps: needOthers ? ["Your setup", "Placement", "Quiet room", "Other voices", "You talk", "Result"]
                                            : ["Your setup", "Placement", "Quiet room", "You talk", "Result"]
    readonly property int step: {
        if (phase === "") return page === "profile" ? 0 : 1
        if (phase === "prepRoom" || phase === "room") return 2
        if (phase === "askOthers" || phase === "prepOthers" || phase === "others") return 3
        if (phase === "prepYou" || phase === "you") return needOthers ? 4 : 3
        return steps.length - 1
    }

    component Choice: Rectangle {
        id: choice
        property string title: ""
        property string sub: ""
        property bool selected: false
        property bool multi: false
        property color accent: Theme.gate
        signal picked()
        Layout.fillWidth: true
        implicitHeight: col.implicitHeight + 24
        radius: 12
        color: selected ? Theme.alpha(accent, 0.14) : (m.containsMouse ? Theme.cardHi : Theme.card)
        border.color: selected ? accent : Theme.border
        border.width: selected ? 2 : 1
        ColumnLayout {
            id: col
            anchors { left: parent.left; right: parent.right; top: parent.top; margins: 12 }
            spacing: 3
            RowLayout {
                spacing: 8
                Rectangle {
                    width: 16; height: 16; radius: choice.multi ? 4 : 8
                    color: choice.selected ? choice.accent : "transparent"
                    border.color: choice.selected ? choice.accent : Theme.textFaint
                    Text { anchors.centerIn: parent; visible: choice.selected; text: "✓"; color: Theme.onAccent; font.pixelSize: 10; font.bold: true }
                }
                Text { text: choice.title; color: Theme.text; font.pixelSize: 14; font.bold: true }
            }
            Text { Layout.fillWidth: true; text: choice.sub; color: Theme.textDim; font.pixelSize: 12; wrapMode: Text.WordWrap }
        }
        MouseArea { id: m; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: choice.picked() }
    }

    contentItem: ColumnLayout {
        spacing: 18

        // Step indicator (not shown for "Learn other voices").
        RowLayout {
            visible: !root.learning
            Layout.fillWidth: true
            spacing: 6
            Repeater {
                model: root.steps
                RowLayout {
                    spacing: 6
                    Rectangle {
                        width: 22; height: 22; radius: 11
                        color: index < root.step ? Theme.gate : index === root.step ? Theme.alpha(Theme.gate, 0.25) : Theme.track
                        border.color: index === root.step ? Theme.gate : "transparent"
                        Text {
                            anchors.centerIn: parent
                            text: index < root.step ? "✓" : index + 1
                            color: index < root.step ? Theme.onAccent : Theme.text
                            font.pixelSize: 11; font.bold: true
                        }
                    }
                    Text {
                        text: modelData
                        color: index === root.step ? Theme.text : Theme.textFaint
                        font.pixelSize: 12
                        font.weight: index === root.step ? Font.Bold : Font.Normal
                    }
                    Rectangle { visible: index < root.steps.length - 1; width: 14; height: 1; color: Theme.border }
                }
            }
        }

        Text {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            color: Theme.text
            font.pixelSize: 24
            font.weight: Font.Bold
            text: root.phase === "" ? (root.page === "profile" ? "Tell MicForge about your setup" : "Set up your mic")
                : root.phase === "prepRoom" || root.phase === "room" ? "Stay quiet for a moment"
                : root.phase === "askOthers" ? "Is anyone else talking nearby?"
                : root.phase === "prepOthers" || root.phase === "others" ? "Recording other voices. You stay silent"
                : root.phase === "prepYou" || root.phase === "you" ? "Your turn: talk normally"
                : root.phase === "learnPrep" || root.phase === "learn" ? "Learning other voices. You stay silent"
                : root.phase === "analysing" ? "Working out how to tell your voice apart…"
                : !root.res.ok ? "Couldn't finish"
                : root.res.learning ? "Updated with the new voices"
                : root.res.redo ? "Updated with the new recording"
                : root.needOthers && root.res.hadOthers ? ["Weak separation", "Usable separation", "Good separation", "Excellent separation"][root.res.quality]
                : "All set"
        }

        // ---- Your setup (profile) -------------------------------------------------
        ColumnLayout {
            visible: root.phase === "" && root.page === "profile"
            Layout.fillWidth: true
            spacing: 12
            Text { text: "Which microphone are you using?"; color: Theme.textDim; font.pixelSize: 13; font.bold: true }
            GridLayout {
                Layout.fillWidth: true
                columns: 4
                columnSpacing: 10
                Repeater {
                    model: [
                        ["Dynamic mic", "SM57, SM58, PodMic, Q2U. Needs to be close; great at ignoring the room."],
                        ["Condenser mic", "Blue Yeti, AT2020, QuadCast. Detailed, but hears more of the room."],
                        ["Headset", "Gaming headset or earbuds with a boom mic near your mouth."],
                        ["Laptop / webcam", "Built-in mic. Hears the whole room."]
                    ]
                    Choice {
                        Layout.preferredWidth: 1
                        Layout.fillHeight: true
                        title: modelData[0]; sub: modelData[1]
                        selected: root.mic === index
                        accent: Theme.input
                        onPicked: controller.setParam("micType", index)
                    }
                }
            }
            Text { Layout.topMargin: 6; text: "What do you need? Pick any that apply."; color: Theme.textDim; font.pixelSize: 13; font.bold: true }
            Choice {
                multi: true
                title: "Other people talk nearby"
                sub: "A partner, housemates or colleagues in the same room. Keep their voices off your mic, even when they're loud. Nobody else has to take part in setup."
                selected: root.p.needOthers > 0.5
                onPicked: controller.setParam("needOthers", selected ? 0 : 1)
            }
            Choice {
                multi: true
                title: "Background noise"
                sub: "PC fans, air conditioning, a mechanical keyboard, traffic. Adds AI noise removal."
                selected: root.p.needNoise > 0.5
                accent: Theme.noise
                onPicked: controller.setParam("needNoise", selected ? 0 : 1)
            }
            Choice {
                multi: true
                title: "Broadcast-quality sound"
                sub: "Streaming, podcasts, recording. Smoother gate, firmer compression, de-esser and a touch more clarity."
                selected: root.p.needBroadcast > 0.5
                accent: Theme.deess
                onPicked: controller.setParam("needBroadcast", selected ? 0 : 1)
            }
            Text {
                visible: root.p.needOthers < 0.5 && root.p.needNoise < 0.5 && root.p.needBroadcast < 0.5
                text: "Nothing picked: MicForge will set up a simple noise gate and level for a quiet room."
                color: Theme.textFaint; font.pixelSize: 12
            }
        }

        // ---- Placement ------------------------------------------------------------
        ColumnLayout {
            visible: root.phase === "" && root.page === "placement"
            Layout.fillWidth: true
            spacing: 14
            Text {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                color: Theme.textDim; font.pixelSize: 13; lineHeight: 1.25
                text: "Setup takes about 20 seconds" + (root.needOthers ? " (plus 10 seconds of other voices if anyone is around)" : "")
                      + ". MicForge listens to the quiet room, then to you, and tunes everything to your voice. Placement makes a bigger difference than any setting:"
            }
            PlacementDiagram {
                visible: root.mic <= 1 && root.needOthers
                Layout.fillWidth: true
                Layout.preferredHeight: 170
            }
            Repeater {
                model: root.mic === 0 ? [
                        "Keep the mic 2–5 cm from your mouth. Close up, your voice gets a deep bass boost (the proximity effect) that MicForge uses to recognise you.",
                        root.needOthers ? "Point the back of the mic (the cable end) towards the other people. That's where a dynamic mic is deaf." : "Point the front of the mic straight at your mouth, not at your keyboard.",
                        "Sit the way you normally would, with headphones on."
                    ] : root.mic === 1 ? [
                        "Get the mic 10–15 cm from your mouth, closer than feels natural. Condensers hear a lot of the room.",
                        "Speak into the correct side. Many USB condensers (like the Blue Yeti) are side-address: talk into the side with the logo, not the top.",
                        root.needOthers ? "Turn the mic so its back faces the other people." : "Keep it away from your keyboard and PC fans."
                    ] : root.mic === 2 ? [
                        "Move the boom so the mic sits at the corner of your mouth, about 2–3 cm away.",
                        "If your headset has a mute switch or its own noise cancelling, leave mute off; MicForge does the cleaning.",
                        "Sit the way you normally would."
                    ] : [
                        "Sit centred in front of the laptop/webcam, as close as is comfortable.",
                        "Built-in mics hear the whole room, so separation from other voices will be limited. A headset is a cheap, big upgrade.",
                        "Keep fans and keyboards as far from the mic as you can."
                    ]
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10
                    Text { text: index + 1; color: Theme.gate; font.bold: true; Layout.alignment: Qt.AlignTop }
                    Text { Layout.fillWidth: true; wrapMode: Text.WordWrap; color: Theme.text; font.pixelSize: 13; text: modelData }
                }
            }
        }

        // ---- Recording -----------------------------------------------------------------
        ColumnLayout {
            visible: root.running && root.phase !== "analysing"
            Layout.fillWidth: true
            spacing: 14
            Text {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                color: Theme.textDim; font.pixelSize: 14; lineHeight: 1.25
                text: root.phase === "prepRoom" ? "Get ready… no talking, typing or moving the mic."
                    : root.phase === "room" ? "Measuring the room's background sound…"
                    : root.phase === "askOthers" ? "If someone is talking nearby right now, for example on their own call, MicForge can record 10 seconds of them while you stay silent. They don't need to do anything special or even know.\n\nNobody around? Skip this. MicForge sets things up from your voice alone, and you can teach it other voices later with “Learn other voices” whenever someone is talking."
                    : root.phase === "prepOthers" || root.phase === "learnPrep" ? "Get ready… stay silent. Recording starts in a moment."
                    : root.phase === "others" || root.phase === "learn" ? "Recording other voices as your mic hears them. Stay silent… The more natural and varied (laughing, loud moments), the better."
                    : root.phase === "prepYou" ? "Get ready to talk at your normal volume…"
                    : "Read this out loud at your normal volume:"
            }
            Rectangle {
                visible: root.phase === "you" || root.phase === "prepYou"
                Layout.fillWidth: true
                implicitHeight: quote.implicitHeight + 26
                radius: 12
                color: Theme.card
                border.color: Theme.alpha(Theme.gate, 0.4)
                Text {
                    id: quote
                    anchors { fill: parent; margins: 13 }
                    wrapMode: Text.WordWrap
                    color: Theme.text
                    font.pixelSize: 15
                    font.italic: true
                    text: "“Hey everyone, can you hear me okay? I'm testing my microphone setup. One, two, three, four, five. This should sound clear, and you shouldn't hear anyone else in the room.”"
                }
            }
            ColumnLayout {
                visible: root.phase !== "askOthers"
                Layout.fillWidth: true
                spacing: 6
                RowLayout {
                    Layout.fillWidth: true
                    Text { text: "What the gate hears"; color: Theme.textFaint; font.pixelSize: 11 }
                    Item { Layout.fillWidth: true }
                    Text {
                        readonly property int secs: root.phase === "room" ? 4 : (root.phase === "others" || root.phase === "learn") ? 10 : 6
                        text: root.phase.indexOf("rep") >= 0 ? "starting…" : Math.ceil((1 - controller.calibProgress) * secs) + " s left"
                        color: Theme.textDim; font.pixelSize: 11
                    }
                }
                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: 12
                    radius: 6
                    color: Theme.track
                    Rectangle {
                        height: parent.height; radius: 6
                        width: parent.width * Math.max(0, Math.min(1, (controller.gateLevel + 80) / 80))
                        color: root.phase === "you" ? Theme.gate : (root.phase === "others" || root.phase === "learn") ? Theme.tone : Theme.textDim
                        Behavior on width { NumberAnimation { duration: 60 } }
                    }
                }
                ProgressBar {
                    Layout.fillWidth: true
                    Layout.topMargin: 4
                    readonly property bool prep: root.phase.indexOf("rep") >= 0
                    value: prep ? 0 : controller.calibProgress
                    indeterminate: prep
                }
            }
        }

        BusyIndicator { visible: root.phase === "analysing"; running: visible; Layout.alignment: Qt.AlignHCenter }

        // ---- Result ----------------------------------------------------------------
        ColumnLayout {
            visible: root.phase === "done"
            Layout.fillWidth: true
            spacing: 14

            // Each recording, with an automatic check and its own Redo button.
            Rectangle {
                visible: !root.res.learning && (root.res.steps || []).length > 0
                Layout.fillWidth: true
                implicitHeight: stepsCol.implicitHeight + 20
                radius: 12
                color: Theme.card
                border.color: Theme.border
                ColumnLayout {
                    id: stepsCol
                    anchors { left: parent.left; right: parent.right; top: parent.top; margins: 10 }
                    spacing: 6
                    Text { text: "Your recordings"; color: Theme.textDim; font.pixelSize: 12; font.bold: true; Layout.leftMargin: 4 }
                    Repeater {
                        model: root.res.steps || []
                        Rectangle {
                            Layout.fillWidth: true
                            implicitHeight: 44
                            radius: 8
                            readonly property color c: modelData.status === "ok" ? Theme.gate
                                                     : modelData.status === "warn" ? Theme.warn
                                                     : modelData.status === "bad" ? Theme.danger : Theme.textFaint
                            color: modelData.status === "ok" || modelData.status === "none" ? "transparent" : Theme.alpha(c, 0.08)
                            RowLayout {
                                anchors { fill: parent; leftMargin: 8; rightMargin: 6 }
                                spacing: 10
                                Rectangle {
                                    width: 22; height: 22; radius: 11
                                    color: Theme.alpha(parent.parent.c, 0.2)
                                    Text {
                                        anchors.centerIn: parent
                                        text: modelData.status === "ok" ? "✓" : modelData.status === "none" ? "–" : "!"
                                        color: parent.parent.parent.c; font.bold: true; font.pixelSize: 12
                                    }
                                }
                                Text { text: modelData.label; color: Theme.text; font.pixelSize: 13; font.bold: true; Layout.preferredWidth: 110 }
                                Text {
                                    Layout.fillWidth: true
                                    text: modelData.note
                                    color: modelData.status === "ok" ? Theme.textDim : Theme.text
                                    font.pixelSize: 12
                                    elide: Text.ElideRight
                                    wrapMode: Text.WordWrap
                                    maximumLineCount: 2
                                }
                                PillButton {
                                    text: modelData.status === "none" ? "Record" : "Redo"
                                    implicitHeight: 28
                                    primary: modelData.status === "warn" || modelData.status === "bad"
                                    accent: modelData.status === "bad" ? Theme.danger : Theme.warn
                                    onClicked: controller.redoStep(modelData.id)
                                }
                            }
                        }
                    }
                }
            }

            Text {
                visible: !root.res.ok
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                color: Theme.textDim; font.pixelSize: 14
                text: root.res.message || ""
            }

            ColumnLayout {
                visible: root.res.ok === true
                Layout.fillWidth: true
                spacing: 14
                Text {
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    color: Theme.textDim; font.pixelSize: 14; lineHeight: 1.25
                    textFormat: Text.StyledText
                    text: !root.res.ok ? ""
                        : root.res.hadOthers
                          ? "In the best listening band (<b>" + root.res.band + "</b>) your voice is <b>" + root.res.separation.toFixed(0) + " dB</b> louder than the other voices"
                            + (root.res.separation - root.res.broadbandSeparation > 1 ? " (vs. " + root.res.broadbandSeparation.toFixed(0) + " dB without the band filter)." : ".")
                          : "Your voice is <b>" + root.res.separation.toFixed(0) + " dB</b> above the room's background sound in the <b>" + root.res.band + "</b> band."
                }
                ZoneBar {
                    Layout.fillWidth: true
                    room: root.res.room || -70
                    others: root.res.others || -200
                    you: root.res.you || -20
                    threshold: root.res.threshold || -30
                    closeAt: root.res.closeAt || -34
                    live: controller.gateLevel
                }

                // Closeness summary.
                Rectangle {
                    visible: root.res.ok === true && root.needOthers
                    Layout.fillWidth: true
                    implicitHeight: protCol.implicitHeight + 24
                    radius: 12
                    color: Theme.card
                    border.color: root.res.closeCheck ? Theme.alpha(Theme.gate, 0.5) : Theme.border
                    ColumnLayout {
                        id: protCol
                        anchors { left: parent.left; right: parent.right; top: parent.top; margins: 12 }
                        spacing: 4
                        Text {
                            text: root.res.closeCheck ? "✓ Closeness check ON: other voices stay out even when loud" : "Closeness check OFF"
                            color: root.res.closeCheck ? Theme.gate : Theme.warn
                            font.pixelSize: 14; font.bold: true
                        }
                        Text {
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                            color: Theme.textDim; font.pixelSize: 13
                            textFormat: Text.StyledText
                            text: !root.res.ok ? ""
                                : !root.res.closeCheck ? "Not used with a built-in laptop/webcam mic."
                                : root.res.tiltMeasured
                                  ? (controller.params.gateTiltDir > 0.5
                                     ? "On your mic, your voice is clearly brighter than the other voices. The gate only opens for sound that bright, so their voices are blocked at any volume."
                                     : "Your close-up voice is clearly bassier than the other voices, even at their most bass-heavy moments. The gate only opens for that, so their voices are blocked at any volume.")
                                    + " On volume alone they could get <b>" + (root.res.headroom || 0).toFixed(0) + " dB</b> louder than their normal speech before opening it."
                                  : "Set from your voice alone: the gate only opens for sound at least as close and bass-heavy as you. Use “Learn other voices” when someone is talking to fine-tune it."
                        }
                    }
                }

                // Live test.
                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: 70
                    radius: 12
                    color: Theme.card
                    border.color: Theme.border
                    RowLayout {
                        anchors { fill: parent; margins: 12 }
                        spacing: 14
                        Rectangle {
                            id: light
                            Layout.preferredWidth: 110
                            Layout.fillHeight: true
                            radius: 10
                            property bool sticky: false
                            readonly property bool lit: controller.gateOpen || sticky
                            Connections {
                                target: controller
                                function onMetersChanged() { if (controller.gateOpen) { light.sticky = true; lightTimer.restart() } }
                            }
                            Timer { id: lightTimer; interval: 150; onTriggered: light.sticky = false }
                            color: lit ? Theme.alpha(Theme.gate, 0.25) : Theme.well
                            border.color: lit ? Theme.gate : Theme.border
                            Text {
                                anchors.centerIn: parent
                                text: light.lit ? "OPEN" : "CLOSED"
                                color: light.lit ? Theme.gate : Theme.textFaint
                                font.pixelSize: 16; font.weight: Font.Black
                            }
                        }
                        Text {
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                            color: Theme.text; font.pixelSize: 13
                            textFormat: Text.StyledText
                            text: root.needOthers
                                  ? "<b>Try it:</b> when someone else talks, the light should stay CLOSED. When you talk it should go OPEN."
                                  : "<b>Try it:</b> stay silent and type. The light should stay CLOSED. Talk and it should go OPEN."
                        }
                    }
                }

                GridLayout {
                    columns: 4
                    columnSpacing: 18; rowSpacing: 4
                    Text { text: "Listening band"; color: Theme.textFaint; font.pixelSize: 12 }
                    Text { text: root.res.band || ""; color: Theme.text; font.pixelSize: 12; font.bold: true }
                    Text { text: "Gate opens above"; color: Theme.textFaint; font.pixelSize: 12 }
                    Text { text: root.res.ok ? Theme.fmtDb(root.res.threshold) : ""; color: Theme.text; font.pixelSize: 12; font.bold: true }
                    Text { visible: !root.res.learning; text: "Input gain"; color: Theme.textFaint; font.pixelSize: 12 }
                    Text { visible: !root.res.learning; text: root.res.ok ? Theme.fmtDb(root.res.gain) : ""; color: Theme.text; font.pixelSize: 12; font.bold: true }
                    Text { visible: !root.res.learning; text: "Compressor"; color: Theme.textFaint; font.pixelSize: 12 }
                    Text { visible: !root.res.learning; text: root.res.ok && !root.res.learning ? Theme.fmtDb(root.res.compThreshold) + ", boost " + Theme.fmtDb(root.res.makeup) : ""; color: Theme.text; font.pixelSize: 12; font.bold: true }
                    readonly property var lv: root.res.levels || ({})
                    Text { visible: !!parent.lv.valid; text: "Voice above room"; color: Theme.textFaint; font.pixelSize: 12 }
                    Text {
                        visible: !!parent.lv.valid
                        text: parent.lv.valid ? Math.round(parent.lv.range) + " dB" + (parent.lv.range >= 25 ? " (good)" : parent.lv.range >= 15 ? " (fair)" : " (low)") : ""
                        color: parent.lv.range >= 15 ? Theme.text : Theme.warn; font.pixelSize: 12; font.bold: true
                    }
                    Text { visible: parent.lv.clips !== undefined; text: "Clipping"; color: Theme.textFaint; font.pixelSize: 12 }
                    Text {
                        visible: parent.lv.clips !== undefined
                        text: parent.lv.clips > 0 ? parent.lv.clips + " samples: lower the gain on your interface" : "none"
                        color: parent.lv.clips > 0 ? Theme.danger : Theme.text; font.pixelSize: 12; font.bold: true
                    }
                }

                Repeater {
                    model: root.res.tips || []
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8
                        Text { text: "💡"; font.pixelSize: 13; Layout.alignment: Qt.AlignTop }
                        Text { Layout.fillWidth: true; wrapMode: Text.WordWrap; color: Theme.textDim; font.pixelSize: 13; text: modelData }
                    }
                }
            }
        }

        // ---- Buttons --------------------------------------------------------------
        RowLayout {
            Layout.fillWidth: true
            Layout.topMargin: 4
            spacing: 10
            PillButton {
                visible: root.phase === "askOthers"
                text: "Skip: nobody's around"
                onClicked: controller.skipOthers()
            }
            PillButton {
                visible: root.phase === "" && root.page === "placement"
                text: "Back"
                onClicked: root.page = "profile"
            }
            Item { Layout.fillWidth: true }
            PillButton {
                visible: root.running && root.phase !== "askOthers" && root.phase !== "analysing"
                text: "↺ Restart this step"
                onClicked: controller.restartStep()
            }
            PillButton {
                visible: root.running
                text: "Cancel"
                onClicked: { controller.cancelCalibration(); root.close() }
            }
            PillButton {
                visible: root.phase === "" && root.page === "profile"
                text: "Not now"
                onClicked: root.close()
            }
            PillButton {
                visible: root.phase === "done" && !root.res.learning
                text: "Start over"
                onClicked: { controller.cancelCalibration(); root.page = "profile" }
            }
            PillButton {
                visible: root.phase === "" || root.phase === "askOthers" || root.phase === "done"
                text: root.phase === "" ? (root.page === "profile" ? "Next" : "Start")
                    : root.phase === "askOthers" ? "Record 10 s now" : "Done"
                primary: true
                accent: root.phase === "askOthers" ? Theme.tone : Theme.gate
                onClicked: {
                    if (root.phase === "" && root.page === "profile") { controller.applyNeeds(); root.page = "placement" }
                    else if (root.phase === "") controller.startCalibration()
                    else if (root.phase === "askOthers") controller.continueCalibration()
                    else root.close()
                }
            }
        }
    }
}
