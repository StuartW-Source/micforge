import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ApplicationWindow {
    id: win
    width: 1400
    height: 880
    minimumWidth: 860
    minimumHeight: 600
    visible: false
    title: "MicForge"
    color: Theme.bg

    palette {
        window: Theme.panel; windowText: Theme.text
        base: Theme.card; alternateBase: Theme.cardHi; text: Theme.text
        button: Theme.card; buttonText: Theme.text
        highlight: Theme.focus; highlightedText: Theme.onAccent
        mid: Theme.border; dark: Theme.well; light: Theme.cardHi; midlight: Theme.cardHi
        placeholderText: Theme.textFaint; toolTipBase: Theme.tooltip; toolTipText: Theme.text
    }

    onClosing: (close) => {
        if (trayAvailable) { close.accepted = false; win.hide() }
    }

    // Theme: the saved preference, or MICFORGE_THEME for screenshots.
    Binding { target: Theme; property: "mode"; value: initialTheme !== "" ? initialTheme : controller.themeMode }

    readonly property var p: controller.params
    readonly property bool bypassed: p.bypass > 0.5

    // ------------------------------------------------------------------ effects chain
    // Chain order = processing order in the engine. Everything except Input and Output can be
    // removed and re-added from the Effect Library ("optional" = the parameter prefix of xxAdded / xxOn).
    readonly property var stageDefs: [
        { id: "input",     title: "Input",          accent: Theme.input,  enable: "",            optional: "",          file: "StageInput.qml" },
        { id: "noise",     title: "Noise Removal",  accent: Theme.noise,    enable: "noiseOn",     optional: "noise",     file: "StageNoise.qml",
          group: "Clean up", glyph: "≋", blurb: "AI removes fans, keyboard clicks and hum while you talk." },
        { id: "hum",       title: "Hum Removal",    accent: Theme.hum,      enable: "humOn",       optional: "hum",       file: "StageHum.qml",
          group: "Clean up", glyph: "ϟ", blurb: "Removes electrical hum (50/60 Hz buzz) from cables, chargers and ground loops." },
        { id: "lowcut",    title: "Low Cut",        accent: Theme.lowcut, enable: "lowCutOn",    optional: "lowCut",    file: "StageLowCut.qml",
          group: "Clean up", glyph: "⌒", blurb: "Removes rumble, desk bumps and boom." },
        { id: "gate",      title: "Voice Gate",     accent: Theme.gate,   enable: "gateOn",      optional: "gate",      file: "StageGate.qml",
          group: "Clean up", glyph: "◐", blurb: "Mutes the mic between your words and keeps other voices out." },
        { id: "autolevel", title: "Auto Level",     accent: Theme.autolevel,    enable: "autoLevelOn", optional: "autoLevel", file: "StageAutoLevel.qml",
          group: "Shape", glyph: "⇅", blurb: "Keeps your volume steady automatically, quiet or loud." },
        { id: "comp",      title: "Compressor",     accent: Theme.comp,   enable: "compOn",      optional: "comp",      file: "StageComp.qml",
          group: "Shape", glyph: "▤", blurb: "Evens out loud and quiet moments." },
        { id: "deess",     title: "De-esser",       accent: Theme.deess,    enable: "deessOn",     optional: "deess",     file: "StageDeess.qml",
          group: "Clean up", glyph: "ʃ", blurb: "Softens harsh S and T sounds." },
        { id: "vc",        title: "Voice Changer",  accent: Theme.voice,    enable: "vcOn",        optional: "vc",        file: "StageVoice.qml",
          group: "Fun", glyph: "☺", blurb: "Chipmunk, robot, monster, radio and more, with one-click presets." },
        { id: "eq",        title: "Equaliser",      accent: Theme.eq,    enable: "eqOn",        optional: "eq",        file: "StageEq.qml",
          group: "Shape", glyph: "∿", blurb: "Five draggable points for precise tone shaping." },
        { id: "tone",      title: "Tone",           accent: Theme.tone,   enable: "toneOn",      optional: "tone",      file: "StageTone.qml",
          group: "Shape", glyph: "♪", blurb: "Simple Bass, Presence and Air controls." },
        { id: "plugins",   title: "Plugins",        accent: Theme.plugins,    enable: "pluginsOn",   optional: "plugins",   file: "StagePlugins.qml",
          group: "Your plugins", glyph: "⧉", blurb: "Load your own VST3 and LV2 effects (LSP, Calf, x42 and more)." },
        { id: "warmth",    title: "Warmth",         accent: Theme.warmth,    enable: "warmthOn",    optional: "warmth",    file: "StageWarmth.qml",
          group: "Shape", glyph: "◉", blurb: "Analogue-style saturation for a richer, rounder voice." },
        { id: "exciter",   title: "Exciter",        accent: Theme.exciter,    enable: "exciterOn",   optional: "exciter",   file: "StageExciter.qml",
          group: "Shape", glyph: "✧", blurb: "Adds sparkle and clarity so you cut through." },
        { id: "doubler",   title: "Doubler",        accent: Theme.doubler,    enable: "doublerOn",   optional: "doubler",   file: "StageDoubler.qml",
          group: "Space", glyph: "⧉", blurb: "Thicker, fuller voice, like two takes layered." },
        { id: "echo",      title: "Echo",           accent: Theme.echo,    enable: "echoOn",      optional: "echo",      file: "StageEcho.qml",
          group: "Space", glyph: "⟲", blurb: "Repeats that fade away. Subtle slapback to canyon." },
        { id: "reverb",    title: "Reverb",         accent: Theme.reverb,    enable: "reverbOn",    optional: "reverb",    file: "StageReverb.qml",
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
        case "hum": return controller.humBase > 0 ? controller.humBase + " Hz × " + Math.round(p.humHarmonics) : "listening…"
        case "lowcut": return Theme.fmtHz(p.lowCutFreq) + (p.lowCutPlosive > 0.5 ? " · pops" : "")
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

    // ------------------------------------------------------------------ navigation
    readonly property var pages: [
        { id: "overview",  title: "Overview",        glyph: "◉", file: "OverviewPage.qml" },
        { id: "isolation", title: "Voice Isolation", glyph: "◐", file: "IsolationPage.qml" },
        { id: "effects",   title: "Effects",         glyph: "⧉", file: "EffectsPage.qml" },
        { id: "devices",   title: "Audio Devices",   glyph: "⇌", file: "DevicesPage.qml" },
        { id: "mictest",   title: "Mic Test",        glyph: "◎", file: "MicTestPage.qml" },
        { id: "presets",   title: "Presets",         glyph: "★", file: "PresetsPage.qml" },
        { id: "diagnostics", title: "Diagnostics",   glyph: "✚", file: "DiagnosticsPage.qml" },
        { id: "settings",  title: "Settings",        glyph: "⚙", file: "SettingsPage.qml" }
    ]
    property string page: "overview"
    readonly property bool compactNav: width < 1100

    function navigate(id) { page = id }
    function openStage(id) { selected = id; page = "effects" }
    function openSetup() { if (controller.safeMode) return; controller.cancelCalibration(); voiceSetup.open() }
    function openLearn() { voiceSetup.openLearn() }
    function openLibrary() { library.open() }

    Shortcut { sequence: "Ctrl+M"; onActivated: controller.toggleMute() }
    Shortcut { sequences: [StandardKey.Undo]; onActivated: controller.undo() }
    Shortcut { sequences: [StandardKey.Redo, "Ctrl+Shift+Z"]; onActivated: controller.redo() }
    Repeater {
        model: win.pages
        Item { Shortcut { sequence: "Ctrl+" + (index + 1); onActivated: win.page = modelData.id } }
    }

    // ------------------------------------------------------------------ header
    header: Rectangle {
        height: 60
        color: Theme.panel
        Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: Theme.border }

        RowLayout {
            anchors { fill: parent; leftMargin: Theme.sp4; rightMargin: Theme.sp4 }
            spacing: Theme.sp3

            Image { source: "qrc:/icons/micforge.svg"; sourceSize.width: 34; sourceSize.height: 34; Layout.preferredWidth: 34; Layout.preferredHeight: 34 }
            ColumnLayout {
                spacing: 0
                Text { text: "MicForge"; color: Theme.text; font.pixelSize: Theme.fsHeading - 1; font.weight: Font.Bold }
                Text { visible: !win.compactNav; text: "Only your voice, nobody else's"; color: Theme.textFaint; font.pixelSize: Theme.fsSmall }
            }

            // Status: what apps are hearing right now.
            Rectangle {
                id: pill
                Layout.leftMargin: Theme.sp2
                Layout.fillWidth: true
                Layout.minimumWidth: 40
                Layout.maximumWidth: statusText.implicitWidth + 44
                Layout.preferredWidth: statusText.implicitWidth + 44
                height: 30
                radius: 15
                readonly property color c: {
                    switch (controller.appState) {
                    case "processing": return controller.virtualMicInUse ? Theme.gate : Theme.input
                    case "muted": return Theme.danger
                    case "bypassed": case "connecting": case "no-input": return Theme.warn
                    }
                    return Theme.danger
                }
                color: Theme.tint(c)
                border.color: Theme.tintBorder(c)
                Accessible.role: Accessible.StaticText
                Accessible.name: controller.appStateText
                Item {
                    anchors { fill: parent; leftMargin: 12; rightMargin: 12 }
                    Rectangle {
                        id: dot
                        width: 8; height: 8; radius: 4
                        anchors.verticalCenter: parent.verticalCenter
                        color: pill.c
                        SequentialAnimation on opacity {
                            running: controller.appState === "processing" && controller.virtualMicInUse
                            loops: Animation.Infinite
                            NumberAnimation { to: 0.3; duration: 900 }
                            NumberAnimation { to: 1; duration: 900 }
                            onStopped: dot.opacity = 1
                        }
                    }
                    Text {
                        id: statusText
                        x: 16
                        width: parent.width - 16
                        anchors.verticalCenter: parent.verticalCenter
                        color: Theme.text
                        font.pixelSize: Theme.fsBody
                        elide: Text.ElideRight
                        text: controller.appStateText
                    }
                }
            }

            Item { Layout.fillWidth: true; Layout.minimumWidth: 0 }

            PillButton {
                text: "↶"
                implicitWidth: Theme.controlHeight
                enabled: controller.undoText !== ""
                onClicked: controller.undo()
                Accessible.name: "Undo " + controller.undoText
                ToolTipLite { visible: uh.hovered && parent.enabled; text: "Undo: " + controller.undoText + " (Ctrl+Z)" }
                HoverHandler { id: uh }
            }
            PillButton {
                text: "↷"
                implicitWidth: Theme.controlHeight
                enabled: controller.redoText !== ""
                onClicked: controller.redo()
                Accessible.name: "Redo " + controller.redoText
                ToolTipLite { visible: rh.hovered && parent.enabled; text: "Redo: " + controller.redoText + " (Ctrl+Shift+Z)" }
                HoverHandler { id: rh }
            }

            PillButton {
                text: controller.muted ? "Muted" : "Mute"
                icon: controller.muted ? "⊘" : "●"
                checkable: true
                checked: controller.muted
                accent: Theme.danger
                onClicked: controller.toggleMute()
                ToolTipLite { visible: parent.activeFocus || hh.hovered; text: "Send silence to your apps (Ctrl+M)" }
                HoverHandler { id: hh }
            }
            PillButton {
                text: "Listen"
                icon: "🎧"
                checkable: true
                checked: controller.monitor
                accent: Theme.output
                onClicked: controller.monitor = !controller.monitor
                ToolTipLite { visible: lh.hovered; text: "Hear yourself through your speakers or headphones" }
                HoverHandler { id: lh }
            }
            PillButton {
                text: win.bypassed ? "Raw mic" : "Compare"
                icon: "⇄"
                checkable: true
                checked: win.bypassed
                accent: Theme.warn
                onClicked: controller.setParam("bypass", win.bypassed ? 0 : 1)
                ToolTipLite { visible: ch.hovered; text: "Switch between processed and raw microphone" }
                HoverHandler { id: ch }
            }
        }
    }

    // ------------------------------------------------------------------ body
    RowLayout {
        anchors.fill: parent
        spacing: 0

        // Sidebar.
        Rectangle {
            Layout.fillHeight: true
            Layout.preferredWidth: win.compactNav ? 64 : 208
            color: Theme.panel
            Rectangle { anchors.right: parent.right; width: 1; height: parent.height; color: Theme.border }
            ColumnLayout {
                anchors { fill: parent; margins: Theme.sp2; topMargin: Theme.sp3 }
                spacing: 2
                Repeater {
                    model: win.pages
                    NavItem {
                        Layout.fillWidth: true
                        text: modelData.title
                        glyph: modelData.glyph
                        compact: win.compactNav
                        selected: win.page === modelData.id
                        shortcutHint: "Ctrl+" + (index + 1)
                        onClicked: win.page = modelData.id
                    }
                }
                Item { Layout.fillHeight: true }
                Text {
                    visible: !win.compactNav
                    Layout.leftMargin: Theme.sp3
                    Layout.bottomMargin: Theme.sp2
                    text: "Version " + controller.version
                    color: Theme.textFaint
                    font.pixelSize: Theme.fsCaption
                }
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.margins: Theme.sp4
            spacing: Theme.sp3

            // Notices: safe mode, damaged settings, engine problems, missing optional libraries, mute, bypass.
            Repeater {
                model: {
                    var n = []
                    if (controller.safeMode)
                        n.push({ kind: "warn", text: controller.safeModeReason + " Your plugins and saved sound settings aren't loaded, and nothing you change now is saved. Your configuration is untouched.", action: "Restart normally" })
                    if (controller.damagedSettingsCopy !== "")
                        n.push({ kind: "danger", text: "Your settings file couldn't be read, so defaults are in use. A copy of the damaged file was kept at " + controller.damagedSettingsCopy + ".", action: "" })
                    if (controller.engineState === "reconnecting" && controller.engineError !== "")
                        n.push({ kind: "warn", text: "Waiting for the audio system: " + controller.engineError + ". MicForge will connect automatically.", action: "Retry now" })
                    if (controller.muted)
                        n.push({ kind: "danger", text: "Muted: your apps are receiving silence.", action: "Unmute" })
                    else if (win.bypassed)
                        n.push({ kind: "warn", text: "Comparing: apps hear your RAW microphone (only input gain applied).", action: "Turn processing back on" })
                    if (controller.presetNotice !== "")
                        n.push({ kind: "warn", text: controller.presetNotice, action: "Dismiss" })
                    if (controller.profileNotice !== "")
                        n.push({ kind: "info", text: controller.profileNotice, action: "OK" })
                    for (var i = 0; i < controller.unavailableFeatures.length; ++i)
                        n.push({ kind: "info", text: controller.unavailableFeatures[i], action: "" })
                    return n
                }
                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: Math.max(40, noticeRow.implicitHeight + 14)
                    radius: Theme.radiusSm + 2
                    readonly property color c: modelData.kind === "danger" ? Theme.danger : modelData.kind === "warn" ? Theme.warn : Theme.input
                    color: Theme.tint(c)
                    border.color: Theme.tintBorder(c)
                    Accessible.role: Accessible.AlertMessage
                    Accessible.name: modelData.text
                    RowLayout {
                        id: noticeRow
                        anchors { left: parent.left; right: parent.right; verticalCenter: parent.verticalCenter; leftMargin: 14; rightMargin: 6 }
                        spacing: Theme.sp3
                        Text {
                            Layout.fillWidth: true
                            text: modelData.text
                            color: Theme.text
                            font.pixelSize: Theme.fsBody
                            wrapMode: Text.WordWrap
                        }
                        PillButton {
                            visible: modelData.action !== ""
                            text: modelData.action
                            implicitHeight: Theme.controlHeightSm
                            onClicked: {
                                switch (modelData.action) {
                                case "Retry now": controller.restartEngine(); break
                                case "Unmute": controller.muted = false; break
                                case "Turn processing back on": controller.setParam("bypass", 0); break
                                case "OK": controller.dismissProfileNotice(); break
                                case "Dismiss": controller.dismissPresetNotice(); break
                                default: controller.requestRestart(false)
                                }
                            }
                        }
                    }
                }
            }

            // Pages. Each is created on first visit and then kept, so switching is instant.
            StackLayout {
                id: stack
                Layout.fillWidth: true
                Layout.fillHeight: true
                currentIndex: win.pages.findIndex(pg => pg.id === win.page)
                Repeater {
                    model: win.pages
                    Loader {
                        active: win.page === modelData.id || item !== null
                        source: modelData.file
                        onLoaded: item.app = win
                    }
                }
            }
        }
    }

    VoiceSetup { id: voiceSetup }
    EffectLibrary {
        id: library
        stages: win.stageDefs
        onPicked: (id) => win.openStage(id)
    }

    Component.onCompleted: {
        if (initialPage !== "") {
            if (pages.some(pg => pg.id === initialPage)) page = initialPage
            else openStage(initialPage)
        }
        if (initialSetup) voiceSetup.open()
        if (initialLibrary) library.open()
    }
}
