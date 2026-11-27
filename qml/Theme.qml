pragma Singleton
import QtQuick

QtObject {
    // ------------------------------------------------------------------ design tokens
    // Every colour in the UI comes from here; components never hard-code colours.
    // `mode` is "system" (follow the desktop's light/dark setting), "dark" or "light".
    property string mode: "system"
    readonly property bool dark: mode === "dark"
                                 || (mode === "system" && Qt.styleHints.colorScheme !== Qt.ColorScheme.Light)
    function pick(d, l) { return dark ? d : l }

    // Surfaces (back to front) and lines.
    readonly property color bg:        pick("#121419", "#eceef2")
    readonly property color panel:     pick("#1a1d24", "#f7f8fa")
    readonly property color card:      pick("#1f232b", "#ffffff")
    readonly property color cardHi:    pick("#262b35", "#eef1f6")
    readonly property color well:      pick("#0e1015", "#e4e7ed")    // inset areas: meters, graphs
    readonly property color border:    pick("#2d323d", "#d3d8e1")
    readonly property color borderHi:  pick("#3a4050", "#b9c0cc")
    readonly property color track:     pick("#2b303b", "#dde1e8")
    readonly property color grid:      pick("#252a34", "#e3e6ec")    // graph grid lines
    readonly property color tooltip:   pick("#0d0f13", "#ffffff")
    readonly property color scrim:     pick("#a0000000", "#66000000") // behind modal dialogs

    // Text.
    readonly property color text:      pick("#e8eaf0", "#181b22")
    readonly property color textDim:   pick("#9aa2b3", "#4a5263")
    readonly property color textFaint: pick("#6b7385", "#737b8c")
    readonly property color onAccent:  pick("#10131a", "#ffffff")    // text/icons drawn on an accent fill

    // Semantic + stage accents. Light mode uses deeper shades so accent-coloured text stays readable.
    readonly property color input:     pick("#5aa9ff", "#1f6fd1")
    readonly property color lowcut:    pick("#9b8cff", "#6450dc")
    readonly property color gate:      pick("#3fdc98", "#11895a")
    readonly property color comp:      pick("#ffb347", "#b86400")
    readonly property color tone:      pick("#ff7aa8", "#cc3a72")
    readonly property color output:    pick("#56d4e8", "#0d8197")
    readonly property color danger:    pick("#ff5c5c", "#cc2f2f")
    readonly property color warn:      pick("#ffd24d", "#9a6d00")
    readonly property color noise:     pick("#7ee0c3", "#128a72")
    readonly property color hum:       pick("#9fd8ff", "#2a6fa3")
    readonly property color autolevel: pick("#8fd16a", "#46861f")
    readonly property color deess:     pick("#c9a0ff", "#8048d8")
    readonly property color voice:     pick("#ff9f43", "#bd6110")
    readonly property color eq:        pick("#ffcf5c", "#9a7300")
    readonly property color plugins:   pick("#4dd0e1", "#0c8292")
    readonly property color warmth:    pick("#ff7b5c", "#c94425")
    readonly property color exciter:   pick("#ffe066", "#8f7400")
    readonly property color doubler:   pick("#6ec6ff", "#1c78ba")
    readonly property color echo:      pick("#a3a8ff", "#5158d0")
    readonly property color reverb:    pick("#b388ff", "#7444d6")
    readonly property color focus:     input                         // keyboard focus ring
    readonly property color knob:      pick("#e8eaf0", "#ffffff")    // slider handles
    readonly property color knobPressed: pick("#ffffff", "#eef1f6")
    readonly property color micGrille: pick("#c7ccd6", "#8e96a6")    // placement diagram
    readonly property color micBody:   pick("#8a90a0", "#5d6576")

    function alpha(c, a) { return Qt.rgba(c.r, c.g, c.b, a) }
    // Tinted background + border for notices/badges in an accent colour.
    function tint(c)   { return Qt.rgba(c.r, c.g, c.b, dark ? 0.12 : 0.10) }
    function tintBorder(c) { return Qt.rgba(c.r, c.g, c.b, dark ? 0.50 : 0.55) }

    // Spacing scale (px, before Qt's high-DPI scaling).
    readonly property int sp1: 4
    readonly property int sp2: 8
    readonly property int sp3: 12
    readonly property int sp4: 16
    readonly property int sp5: 24
    readonly property int sp6: 32

    // Corner radii.
    readonly property int radiusSm: 6
    readonly property int radius: 12
    readonly property int radiusLg: 16

    // Type scale.
    readonly property int fsCaption: 10
    readonly property int fsSmall: 11
    readonly property int fsBody: 12
    readonly property int fsBodyLg: 13
    readonly property int fsTitle: 15
    readonly property int fsHeading: 19
    readonly property int fsDisplay: 22
    readonly property string mono: "monospace"

    // Control sizes.
    readonly property int controlHeight: 34
    readonly property int controlHeightSm: 28

    // Voice Changer presets. Every preset sets every Voice Changer control, so the current preset can be recognised.
    // `formant` (voice size) defaults to the pitch: the classic, low-latency shifter where formants follow pitch.
    // `id` is stable (system-wide shortcuts are bound to it); `group` is everyday, fantasy or fun.
    function vc(pitch, character, robot, robotFreq, radio, drive, wobble, wobbleRate, mix, formant, growl, whisper) {
        return { vcPitch: pitch, vcCharacter: character, vcRobot: robot, vcRobotFreq: robotFreq, vcRadio: radio,
                 vcDrive: drive, vcWobble: wobble, vcWobbleRate: wobbleRate, vcMix: mix,
                 vcFormant: formant === undefined ? pitch : formant, vcGrowl: growl || 0, vcWhisper: whisper || 0 }
    }
    readonly property var voiceGroups: [
        { id: "everyday", title: "Everyday" },
        { id: "fantasy",  title: "Fantasy · tabletop and RPG" },
        { id: "fun",      title: "Fun and effects" }
    ]
    readonly property var voicePresets: [
        { id: "natural",   group: "everyday", name: "Natural",     icon: "🙂", tip: "No changes: your own voice.",                       values: vc(0, 0, 0, 50, 0, 0, 0, 5, 100) },
        { id: "child",     group: "everyday", name: "Child",       icon: "🧒", tip: "Higher and smaller, like a young child.",           values: vc(7, 2, 0, 50, 0, 0, 0, 5, 100, 4.5) },
        { id: "woman",     group: "everyday", name: "Woman",       icon: "👩", tip: "A higher, lighter adult voice. Works best starting from a lower voice.", values: vc(5, 1, 0, 50, 0, 0, 0, 5, 100, 2.5) },
        { id: "man",       group: "everyday", name: "Man",         icon: "👨", tip: "A lower, fuller adult voice. Works best starting from a higher voice.",  values: vc(-5, -1, 0, 50, 0, 0, 0, 5, 100, -2.5) },
        { id: "deep",      group: "everyday", name: "Deep Voice",  icon: "🗣", tip: "A lower, movie-trailer voice.",                     values: vc(-4, -4, 0, 50, 0, 0, 0, 5, 100) },

        { id: "dragon",    group: "fantasy",  name: "Dragon",      icon: "🐉", tip: "Vast, ancient and rumbling. Add Reverb (Hall) for a cave.", values: vc(-9, -4, 0, 50, 0, 18, 0, 5, 100, -7, 40) },
        { id: "orc",       group: "fantasy",  name: "Orc",         icon: "🪓", tip: "Brutish and gravelly.",                             values: vc(-5, -3, 0, 50, 0, 22, 0, 5, 100, -3.5, 35) },
        { id: "goblin",    group: "fantasy",  name: "Goblin",      icon: "👺", tip: "Small, nasal and sneaky.",                          values: vc(5, 4, 0, 50, 30, 15, 0, 5, 100, 6, 15) },
        { id: "dwarf",     group: "fantasy",  name: "Dwarf",       icon: "⛏", tip: "Short, stout and gruff.",                            values: vc(-3, -1, 0, 50, 0, 10, 0, 5, 100, 1.5, 18) },
        { id: "elf",       group: "fantasy",  name: "Elf",         icon: "🧝", tip: "Light, airy and graceful.",                         values: vc(2, 2, 0, 50, 0, 0, 6, 4.5, 100, 1, 0, 12) },
        { id: "fairy",     group: "fantasy",  name: "Fairy",       icon: "🧚", tip: "Tiny, bright and fluttering.",                      values: vc(10, 4, 0, 50, 0, 0, 15, 7, 100, 7, 0, 10) },
        { id: "giant",     group: "fantasy",  name: "Giant",       icon: "🗿", tip: "Huge and booming.",                                 values: vc(-8, -8, 0, 50, 0, 10, 0, 5, 100) },
        { id: "monster",   group: "fantasy",  name: "Monster",     icon: "👹", tip: "Growling, gravelly and menacing.",                  values: vc(-6, -4, 0, 50, 0, 12, 0, 5, 100, -5, 60) },
        { id: "demon",     group: "fantasy",  name: "Demon",       icon: "😈", tip: "A monstrous voice with your own underneath it.",   values: vc(-12, -4, 0, 50, 0, 20, 0, 5, 60, -7, 50) },
        { id: "lich",      group: "fantasy",  name: "Lich",        icon: "💀", tip: "A cold, hollow whisper from beyond the grave.",    values: vc(-3, -2, 0, 50, 0, 0, 15, 1.5, 100, -3, 0, 75) },
        { id: "golem",     group: "fantasy",  name: "Golem",       icon: "🪨", tip: "Stone and clockwork: deep and buzzing.",            values: vc(-6, -3, 55, 35, 0, 10, 0, 5, 100, -5) },
        { id: "wizard",    group: "fantasy",  name: "Old Wizard",  icon: "🧙", tip: "Aged, breathy and quavering.",                      values: vc(-1, -1, 0, 50, 0, 0, 30, 5.5, 100, 0, 0, 20) },

        { id: "chipmunk",  group: "fun",      name: "Chipmunk",    icon: "🐿", tip: "Squeaky and fast-sounding.",                        values: vc(9, 6, 0, 50, 0, 0, 0, 5, 100) },
        { id: "helium",    group: "fun",      name: "Helium",      icon: "🎈", tip: "Like you just breathed in a balloon.",              values: vc(5, 9, 0, 50, 0, 0, 0, 5, 100) },
        { id: "robot",     group: "fun",      name: "Robot",       icon: "🤖", tip: "Metallic, buzzing machine voice.",                  values: vc(0, 0, 100, 60, 0, 15, 0, 5, 100) },
        { id: "alien",     group: "fun",      name: "Alien",       icon: "👽", tip: "Wobbly, otherworldly and ringing.",                 values: vc(4, 3, 50, 420, 0, 0, 35, 7, 100) },
        { id: "walkie",    group: "fun",      name: "Walkie-talkie", icon: "📟", tip: "Crunchy two-way radio.",                          values: vc(0, 0, 0, 50, 100, 55, 0, 5, 100) },
        { id: "telephone", group: "fun",      name: "Telephone",   icon: "☎", tip: "Thin, old-fashioned phone line.",                    values: vc(0, 0, 0, 50, 100, 15, 0, 5, 100) },
        { id: "oldradio",  group: "fun",      name: "Old Radio",   icon: "📻", tip: "Warbly broadcast from the 1940s.",                   values: vc(0, -3, 0, 50, 80, 30, 8, 0.8, 100) },
        { id: "underwater", group: "fun",     name: "Underwater",  icon: "🌊", tip: "Muffled and bubbling.",                             values: vc(-1, -10, 0, 50, 0, 0, 70, 2.5, 100) },
        { id: "ghost",     group: "fun",      name: "Ghost",       icon: "👻", tip: "Eerie and wavering. Add Reverb for extra spookiness.", values: vc(-3, -2, 0, 50, 0, 0, 40, 1.2, 70) }
    ]
    function voicePresetById(id) {
        for (var i = 0; i < voicePresets.length; ++i) if (voicePresets[i].id === id) return voicePresets[i]
        return null
    }
    readonly property var reverbPresets: [
        { name: "Small room", values: { reverbSize: 30, reverbDamp: 60, reverbMix: 15 } },
        { name: "Studio",     values: { reverbSize: 45, reverbDamp: 50, reverbMix: 18 } },
        { name: "Hall",       values: { reverbSize: 75, reverbDamp: 35, reverbMix: 25 } },
        { name: "Cathedral",  values: { reverbSize: 95, reverbDamp: 20, reverbMix: 35 } }
    ]
    readonly property var echoPresets: [
        { name: "Slapback", values: { echoTime: 90,  echoFeedback: 10, echoMix: 25, echoTone: 6000 } },
        { name: "Stadium",  values: { echoTime: 380, echoFeedback: 40, echoMix: 30, echoTone: 4000 } },
        { name: "Canyon",   values: { echoTime: 650, echoFeedback: 55, echoMix: 35, echoTone: 3000 } }
    ]
    function applyPreset(ctl, values) { for (var k in values) ctl.setParam(k, values[k]) }
    function matches(params, values) {
        for (var k in values) if (Math.abs(params[k] - values[k]) > 0.01) return false
        return true
    }

    function fmtHz(v) { return v >= 1000 ? (v / 1000).toFixed(v >= 10000 ? 1 : 2) + " kHz" : Math.round(v) + " Hz" }
    function fmtMs(v) { return v >= 1000 ? (v / 1000).toFixed(2) + " s" : (v < 10 ? v.toFixed(1) : Math.round(v)) + " ms" }

    function fmtDb(v, decimals) {
        if (v <= -79.9) return "−∞ dB"
        var d = decimals === undefined ? 1 : decimals
        var s = Math.abs(v).toFixed(d)
        return (v < -0.05 ? "−" : (v > 0.05 ? "+" : "")) + s + " dB"
    }
}
