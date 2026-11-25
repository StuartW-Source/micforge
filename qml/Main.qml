import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ApplicationWindow {
    id: win
    width: 1500
    height: 900
    minimumWidth: 1000
    minimumHeight: 680
    visible: false
    title: "MicForge"
    color: Theme.bg

    palette {
        window: Theme.panel; windowText: Theme.text
        base: Theme.card; alternateBase: Theme.cardHi; text: Theme.text
        button: Theme.card; buttonText: Theme.text
        highlight: Theme.input; highlightedText: "#10131a"
        mid: Theme.border; dark: "#0e1015"; light: Theme.cardHi; midlight: Theme.cardHi
        placeholderText: Theme.textFaint; toolTipBase: "#0d0f13"; toolTipText: Theme.text
    }

    onClosing: (close) => {
        if (trayAvailable) { close.accepted = false; win.hide() }
    }

    readonly property var p: controller.params
    readonly property bool bypassed: p.bypass > 0.5

    // ------------------------------------------------------------------ effects chain
    // Chain order = processing order in the engine. Everything except Input and Output can be
    // removed and re-added from the Effect Library ("optional" = the parameter prefix of xxAdded / xxOn).
    readonly property var stageDefs: [
        { id: "input",     title: "Input",          accent: Theme.input,  enable: "",            optional: "",          file: "StageInput.qml" },
        { id: "noise",     title: "Noise Removal",  accent: "#7ee0c3",    enable: "noiseOn",     optional: "noise",     file: "StageNoise.qml",
          group: "Clean up", glyph: "≋", blurb: "AI removes fans, keyboard clicks and hum while you talk." },
        { id: "lowcut",    title: "Low Cut",        accent: Theme.lowcut, enable: "lowCutOn",    optional: "lowCut",    file: "StageLowCut.qml",
          group: "Clean up", glyph: "⌒", blurb: "Removes rumble, desk bumps and boom." },
        { id: "gate",      title: "Voice Gate",     accent: Theme.gate,   enable: "gateOn",      optional: "gate",      file: "StageGate.qml",
          group: "Clean up", glyph: "◐", blurb: "Mutes the mic between your words and keeps other voices out." },
        { id: "autolevel", title: "Auto Level",     accent: "#8fd16a",    enable: "autoLevelOn", optional: "autoLevel", file: "StageAutoLevel.qml",
          group: "Shape", glyph: "⇅", blurb: "Keeps your volume steady automatically, quiet or loud." },
        { id: "comp",      title: "Compressor",     accent: Theme.comp,   enable: "compOn",      optional: "comp",      file: "StageComp.qml",
          group: "Shape", glyph: "▤", blurb: "Evens out loud and quiet moments." },
        { id: "deess",     title: "De-esser",       accent: "#c9a0ff",    enable: "deessOn",     optional: "deess",     file: "StageDeess.qml",
          group: "Clean up", glyph: "ʃ", blurb: "Softens harsh S and T sounds." },
        { id: "vc",        title: "Voice Changer",  accent: "#ff9f43",    enable: "vcOn",        optional: "vc",        file: "StageVoice.qml",
          group: "Fun", glyph: "☺", blurb: "Chipmunk, robot, monster, radio and more, with one-click presets." },
        { id: "eq",        title: "Equaliser",      accent: "#ffcf5c",    enable: "eqOn",        optional: "eq",        file: "StageEq.qml",
          group: "Shape", glyph: "∿", blurb: "Five draggable points for precise tone shaping." },
        { id: "tone",      title: "Tone",           accent: Theme.tone,   enable: "toneOn",      optional: "tone",      file: "StageTone.qml",
          group: "Shape", glyph: "♪", blurb: "Simple Bass, Presence and Air controls." },
        { id: "plugins",   title: "Plugins",        accent: "#4dd0e1",    enable: "pluginsOn",   optional: "plugins",   file: "StagePlugins.qml",
          group: "Your plugins", glyph: "⧉", blurb: "Load your own VST3 and LV2 effects (LSP, Calf, x42 and more)." },
        { id: "warmth",    title: "Warmth",         accent: "#ff7b5c",    enable: "warmthOn",    optional: "warmth",    file: "StageWarmth.qml",
          group: "Shape", glyph: "◉", blurb: "Analogue-style saturation for a richer, rounder voice." },
        { id: "exciter",   title: "Exciter",        accent: "#ffe066",    enable: "exciterOn",   optional: "exciter",   file: "StageExciter.qml",
          group: "Shape", glyph: "✧", blurb: "Adds sparkle and clarity so you cut through." },
        { id: "doubler",   title: "Doubler",        accent: "#6ec6ff",    enable: "doublerOn",   optional: "doubler",   file: "StageDoubler.qml",
          group: "Space", glyph: "⧉", blurb: "Thicker, fuller voice, like two takes layered." },
        { id: "echo",      title: "Echo",           accent: "#a3a8ff",    enable: "echoOn",      optional: "echo",      file: "StageEcho.qml",
          group: "Space", glyph: "⟲", blurb: "Repeats that fade away. Subtle slapback to canyon." },
        { id: "reverb",    title: "Reverb",         accent: "#b388ff",    enable: "reverbOn",    optional: "reverb",    file: "StageReverb.qml",
          group: "Space", glyph: "◌", blurb: "Puts your voice in a room, hall or cathedral." },
        { id: "output",    title: "Output",         accent: Theme.output, enable: "",            optional: "",          file: "StageOutput.qml" }
    ]
    readonly property var visibleStages: stageDefs.filter(s => s.optional === "" || p[s.optional + "Added"] > 0.5)
    readonly property var addable: stageDefs.filter(s => s.optional !== "" && !(p[s.optional + "Added"] > 0.5))
    property string selected: "gate"
    onVisibleStagesChanged: if (!visibleStages.some(s => s.id === selected)) selected = visibleStages.some(s => s.id === "gate") ? "gate" : "input"
    readonly property var selectedDef: stageDefs.find(s => s.id === selected)

    function stageStatus(id) {
        switch (id) {
        case "input": return Theme.fmtDb(p.inputGain) + " · " + ["In 1", "In 2", "Both"][Math.round(p.inputChannel)]
        case "noise": return Math.round(p.noiseAmount) + "% · voice " + Math.round(controller.noiseVad * 100) + "%"
        case "lowcut": return Theme.fmtHz(p.lowCutFreq)
        case "gate": return (controller.gateOpen ? "OPEN" : "closed") + (p.gateMode > 0.5 ? " · soft" : "")
        case "comp": return "−" + controller.compReduction.toFixed(1) + " dB"
        case "deess": return "−" + controller.deessReduction.toFixed(1) + " dB"
        case "eq": {
            var n = 0
            for (var b = 1; b <= 5; ++b) if (Math.abs(p["eq" + b + "Gain"]) > 0.05) ++n
            return n === 0 ? "flat" : n + " point" + (n > 1 ? "s" : "") + " active"
        }
        case "tone": return (Math.abs(p.toneBass) + Math.abs(p.tonePresence) + Math.abs(p.toneAir)) < 0.1 ? "flat" : "shaped"
        case "autolevel": return (controller.autoLevelGain >= 0 ? "+" : "−") + Math.abs(controller.autoLevelGain).toFixed(1) + " dB"
        case "vc": return voicePresetName()
        case "warmth": return Math.round(p.warmthAmount) + "%"
        case "plugins": return controller.plugins.chain.length === 0 ? "empty" : controller.plugins.chain.length + " loaded"
        case "exciter": return Math.round(p.exciterAmount) + "%"
        case "doubler": return Math.round(p.doublerMix) + "% mix"
        case "echo": return Math.round(p.echoTime) + " ms"
        case "reverb": return Math.round(p.reverbMix) + "% · size " + Math.round(p.reverbSize)
        case "output": return controller.outPeak > -100 ? controller.outPeak.toFixed(0) + " dB" : "silent"
        }
        return ""
    }
    function stageActivity(id) {
        switch (id) {
        case "input": return (controller.inPeak + 60) / 60
        case "noise": return controller.noiseVad
        case "lowcut": return 0.3
        case "gate": return controller.gateOpen ? 1 : 0
        case "comp": return controller.compReduction / 12
        case "deess": return controller.deessReduction / 8
        case "eq": return 0.3
        case "tone": return 0.3
        case "autolevel": return 0.5 + controller.autoLevelGain / 24
        case "output": return (controller.outPeak + 60) / 60
        }
        return 0.3
    }
    // Name of the matching Voice Changer preset, or "custom".
    function voicePresetName() {
        for (var i = 0; i < Theme.voicePresets.length; ++i) {
            var pr = Theme.voicePresets[i], same = true
            for (var k in pr.values) if (Math.abs(p[k] - pr.values[k]) > 0.01) { same = false; break }
            if (same) return pr.name
        }
        return "custom"
    }

    // ------------------------------------------------------------------ header
    header: Rectangle {
        height: 68
        color: Theme.panel
        Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: Theme.border }

        RowLayout {
            anchors { fill: parent; leftMargin: 20; rightMargin: 16 }
            spacing: 12

            Rectangle {
                width: 38; height: 38; radius: 10
                gradient: Gradient {
                    orientation: Gradient.Horizontal
                    GradientStop { position: 0; color: Theme.input }
                    GradientStop { position: 1; color: Theme.gate }
                }
                Rectangle { x: 14; y: 7; width: 10; height: 16; radius: 5; color: "#10131a" }
                Rectangle { x: 18; y: 25; width: 2; height: 5; color: "#10131a" }
                Rectangle { x: 13; y: 30; width: 12; height: 2; radius: 1; color: "#10131a" }
            }
            ColumnLayout {
                spacing: 0
                Text { text: "MicForge"; color: Theme.text; font.pixelSize: 19; font.weight: Font.Bold }
                Text { text: "Only your voice, nobody else's"; color: Theme.textFaint; font.pixelSize: 11 }
            }

            Rectangle {
                id: pill
                Layout.leftMargin: 12
                height: 30
                implicitWidth: statusRow.implicitWidth + 24
                radius: 15
                readonly property color c: !controller.engineOk ? Theme.danger
                                           : !controller.captureRunning ? Theme.warn
                                           : controller.virtualMicInUse ? Theme.gate : Theme.input
                color: Qt.rgba(c.r, c.g, c.b, 0.12)
                border.color: Qt.rgba(c.r, c.g, c.b, 0.5)
                Row {
                    id: statusRow
                    anchors.centerIn: parent
                    spacing: 8
                    Rectangle {
                        width: 8; height: 8; radius: 4
                        anchors.verticalCenter: parent.verticalCenter
                        color: pill.c
                        SequentialAnimation on opacity {
                            running: controller.virtualMicInUse
                            loops: Animation.Infinite
                            NumberAnimation { to: 0.3; duration: 900 }
                            NumberAnimation { to: 1; duration: 900 }
                        }
                    }
                    Text {
                        color: Theme.text
                        font.pixelSize: 12
                        text: !controller.engineOk ? "Can't connect to PipeWire"
                            : !controller.captureRunning ? "Waiting for microphone…"
                            : controller.virtualMicInUse ? "Live · an app is using MicForge"
                            : "Ready · pick “MicForge” as your mic in Discord, OBS or any app"
                    }
                }
            }

            Item { Layout.fillWidth: true }

            Text { text: "Preset"; color: Theme.textFaint; font.pixelSize: 12 }
            ComboBox {
                Layout.preferredWidth: 210
                model: controller.presetNames
                displayText: controller.currentPreset === "" ? "Custom (edited)" : controller.currentPreset
                currentIndex: controller.presetNames.indexOf(controller.currentPreset)
                onActivated: (i) => controller.loadPreset(controller.presetNames[i])
            }
            PillButton {
                text: "Setup"
                icon: "✦"
                primary: true
                accent: Theme.gate
                onClicked: { controller.cancelCalibration(); voiceSetup.open() }
            }
            PillButton {
                text: "Listen"
                icon: "🎧"
                checkable: true
                checked: controller.monitor
                accent: Theme.output
                onClicked: controller.monitor = !controller.monitor
            }
            PillButton {
                text: win.bypassed ? "Raw mic" : "Compare"
                icon: "⇄"
                checkable: true
                checked: win.bypassed
                accent: Theme.warn
                onClicked: controller.setParam("bypass", win.bypassed ? 0 : 1)
            }
            PillButton {
                text: "⋯"
                onClicked: menu.popup()
                Menu {
                    id: menu
                    MenuItem { text: "Settings…"; onTriggered: settingsDialog.open() }
                    MenuSeparator {}
                    MenuItem { text: "Save settings as preset…"; onTriggered: savePopup.open() }
                    MenuItem {
                        text: "Delete preset “" + controller.currentPreset + "”"
                        enabled: controller.currentPreset !== "" && !controller.isBuiltinPreset(controller.currentPreset)
                        onTriggered: controller.deletePreset(controller.currentPreset)
                    }
                    MenuItem { text: "Reset sound to defaults"; onTriggered: controller.loadPreset(controller.presetNames[0]) }
                    MenuSeparator {}
                    MenuItem { text: "Show getting-started tips"; onTriggered: controller.showWelcome = true }
                }
            }
        }
    }

    // ------------------------------------------------------------------ body
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 18
        spacing: 14

        // Getting started banner.
        Rectangle {
            visible: controller.showWelcome
            Layout.fillWidth: true
            implicitHeight: welcomeRow.implicitHeight + 26
            radius: Theme.radius
            color: Qt.rgba(Theme.input.r, Theme.input.g, Theme.input.b, 0.08)
            border.color: Qt.rgba(Theme.input.r, Theme.input.g, Theme.input.b, 0.35)
            RowLayout {
                id: welcomeRow
                anchors { fill: parent; margins: 13; leftMargin: 18 }
                spacing: 28
                Repeater {
                    model: [
                        ["1", "Run Setup", "Click “Setup”, tell MicForge about your mic and room, and talk for a few seconds. It tunes everything to your voice."],
                        ["2", "Choose MicForge in your app", "In Discord, OBS or any other app pick “MicForge (processed mic)” as the input device, and turn OFF their noise suppression, echo cancellation and automatic gain."],
                        ["3", "Fine-tune by ear", "Click a block in the chain to adjust it. Listen = hear yourself, Compare = raw vs processed. Hover any ? for help."]
                    ]
                    RowLayout {
                        Layout.fillWidth: true
                        Layout.preferredWidth: 1
                        spacing: 12
                        Rectangle {
                            Layout.alignment: Qt.AlignTop
                            width: 26; height: 26; radius: 13
                            color: Theme.input
                            Text { anchors.centerIn: parent; text: modelData[0]; font.bold: true; color: "#10131a" }
                        }
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 3
                            Text { text: modelData[1]; color: Theme.text; font.pixelSize: 13; font.weight: Font.Bold }
                            Text { text: modelData[2]; color: Theme.textDim; font.pixelSize: 12; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                        }
                    }
                }
                Text {
                    Layout.alignment: Qt.AlignTop
                    text: "✕"
                    color: Theme.textDim
                    font.pixelSize: 14
                    MouseArea { anchors.fill: parent; anchors.margins: -6; cursorShape: Qt.PointingHandCursor; onClicked: controller.showWelcome = false }
                }
            }
        }

        // Bypass notice.
        Rectangle {
            visible: win.bypassed
            Layout.fillWidth: true
            implicitHeight: 36
            radius: 8
            color: Qt.rgba(Theme.warn.r, Theme.warn.g, Theme.warn.b, 0.12)
            border.color: Qt.rgba(Theme.warn.r, Theme.warn.g, Theme.warn.b, 0.5)
            Text {
                anchors.centerIn: parent
                text: "Comparing: you're hearing the RAW microphone (only input gain applied). Click “Raw mic” again to switch processing back on."
                color: Theme.warn
                font.pixelSize: 12
            }
        }

        // Chain strip.
        Flickable {
            Layout.fillWidth: true
            Layout.preferredHeight: 92
            contentWidth: strip.width
            contentHeight: height
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            flickableDirection: Flickable.HorizontalFlick
            ScrollBar.horizontal: ScrollBar { policy: ScrollBar.AsNeeded }

            Row {
                id: strip
                spacing: 0
                Repeater {
                    model: win.visibleStages
                    Row {
                        spacing: 0
                        ChainTile {
                            number: index + 1
                            title: modelData.title
                            accent: modelData.accent
                            enableParam: modelData.enable
                            selected: win.selected === modelData.id
                            status: win.stageStatus(modelData.id)
                            activity: win.stageActivity(modelData.id)
                            onClicked: win.selected = modelData.id
                        }
                        Item {
                            width: 18; height: 84
                            Text { anchors.centerIn: parent; text: "›"; color: Theme.textFaint; font.pixelSize: 20 }
                        }
                    }
                }
                // Add effect: opens the Effect Library.
                Rectangle {
                    width: 120; height: 84; radius: 12
                    color: addMouse.containsMouse ? Theme.cardHi : "transparent"
                    border.color: Theme.border
                    Column {
                        anchors.centerIn: parent
                        spacing: 2
                        Text { anchors.horizontalCenter: parent.horizontalCenter; text: "+"; color: Theme.textDim; font.pixelSize: 24 }
                        Text { anchors.horizontalCenter: parent.horizontalCenter; text: "Add effect"; color: Theme.textDim; font.pixelSize: 12 }
                    }
                    MouseArea {
                        id: addMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: library.open()
                    }
                }
            }
        }

        // Detail + always-visible monitor column.
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 14

            Loader {
                id: detail
                Layout.fillWidth: true
                Layout.fillHeight: true
                source: win.selectedDef ? win.selectedDef.file : ""
            }
            Connections {
                target: detail.item
                ignoreUnknownSignals: true
                function onSetupRequested() { controller.cancelCalibration(); voiceSetup.open() }
                function onLearnRequested() { voiceSetup.openLearn() }
            }

            Rectangle {
                Layout.preferredWidth: 230
                Layout.fillHeight: true
                radius: Theme.radius
                color: Theme.card
                border.color: Theme.border

                ColumnLayout {
                    anchors { fill: parent; margins: 16 }
                    spacing: 12
                    Text { text: "Live"; color: Theme.text; font.pixelSize: 15; font.weight: Font.Bold }
                    RowLayout {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        spacing: 6
                        LevelMeter {
                            Layout.fillHeight: true
                            Layout.fillWidth: true
                            level: controller.inPeak
                            label: "MIC IN"
                            accent: Theme.input
                        }
                        LevelMeter {
                            Layout.fillHeight: true
                            Layout.fillWidth: true
                            showScale: false
                            level: controller.outPeak
                            label: "TO APP"
                            accent: Theme.output
                        }
                    }
                    Rectangle {
                        id: miniGate
                        Layout.fillWidth: true
                        implicitHeight: 40
                        radius: 10
                        property bool sticky: false
                        Connections {
                            target: controller
                            function onMetersChanged() { if (controller.gateOpen) { miniGate.sticky = true; miniTimer.restart() } }
                        }
                        Timer { id: miniTimer; interval: 150; onTriggered: miniGate.sticky = false }
                        readonly property bool lit: (controller.gateOpen || sticky) && win.p.gateOn > 0.5 && !win.bypassed
                        color: lit ? Qt.rgba(Theme.gate.r, Theme.gate.g, Theme.gate.b, 0.2) : "#0e1015"
                        border.color: lit ? Theme.gate : Theme.border
                        Text {
                            anchors.centerIn: parent
                            text: win.p.gateOn < 0.5 || win.bypassed ? "Gate off" : miniGate.lit ? "● Your voice is passing" : "Gate closed"
                            color: miniGate.lit ? Theme.gate : Theme.textFaint
                            font.pixelSize: 12; font.weight: Font.Bold
                        }
                    }
                    PillButton {
                        visible: controller.hasRecordings && win.p.needOthers > 0.5
                        Layout.fillWidth: true
                        text: "Learn other voices"
                        icon: "+"
                        accent: Theme.tone
                        onClicked: voiceSetup.openLearn()
                    }
                    Text {
                        visible: controller.hasRecordings && win.p.needOthers > 0.5
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        color: Theme.textFaint; font.pixelSize: 10
                        text: "Someone talking nearby? Stay silent and click this. MicForge listens for 10 s and learns to ignore them."
                    }
                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: 40
                        radius: 10
                        color: controller.virtualMicInUse ? Qt.rgba(Theme.output.r, Theme.output.g, Theme.output.b, 0.15) : "#0e1015"
                        border.color: controller.virtualMicInUse ? Theme.output : Theme.border
                        Text {
                            anchors.centerIn: parent
                            text: controller.virtualMicInUse ? "ON AIR" : "No app connected"
                            color: controller.virtualMicInUse ? Theme.output : Theme.textFaint
                            font.pixelSize: 12; font.weight: Font.Bold
                        }
                    }
                }
            }
        }
    }

    VoiceSetup { id: voiceSetup }
    SettingsDialog { id: settingsDialog }
    EffectLibrary {
        id: library
        stages: win.stageDefs
        onPicked: (id) => win.selected = id
    }

    Component.onCompleted: {
        if (initialPage !== "") selected = initialPage
        if (initialSetup) voiceSetup.open()
        if (initialLibrary) library.open()
    }

    // ------------------------------------------------------------------ save preset dialog
    Popup {
        id: savePopup
        modal: true
        anchors.centerIn: Overlay.overlay
        width: 420
        padding: 24
        onOpened: { nameField.text = ""; nameField.forceActiveFocus() }
        background: Rectangle { color: Theme.panel; radius: 16; border.color: Theme.border }
        Overlay.modal: Rectangle { color: "#a0000000" }
        contentItem: ColumnLayout {
            spacing: 14
            Text { text: "Save preset"; color: Theme.text; font.pixelSize: 20; font.weight: Font.Bold }
            TextField {
                id: nameField
                Layout.fillWidth: true
                placeholderText: "e.g. Late night gaming"
                onAccepted: if (saveBtn.enabled) saveBtn.clicked()
            }
            RowLayout {
                Layout.fillWidth: true
                Item { Layout.fillWidth: true }
                PillButton { text: "Cancel"; onClicked: savePopup.close() }
                PillButton {
                    id: saveBtn
                    text: "Save"
                    primary: true
                    enabled: nameField.text.trim() !== "" && !controller.isBuiltinPreset(nameField.text.trim())
                    onClicked: { controller.savePreset(nameField.text); savePopup.close() }
                }
            }
        }
    }
}
