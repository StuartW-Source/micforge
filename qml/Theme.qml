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
    function vc(pitch, character, robot, robotFreq, radio, drive, wobble, wobbleRate, mix) {
        return { vcPitch: pitch, vcCharacter: character, vcRobot: robot, vcRobotFreq: robotFreq, vcRadio: radio,
                 vcDrive: drive, vcWobble: wobble, vcWobbleRate: wobbleRate, vcMix: mix }
    }
    readonly property var voicePresets: [
        { name: "Natural",       icon: "🙂", tip: "No changes: your own voice.",                       values: vc(0, 0, 0, 50, 0, 0, 0, 5, 100) },
        { name: "Chipmunk",      icon: "🐿", tip: "Squeaky and fast-sounding.",                        values: vc(9, 6, 0, 50, 0, 0, 0, 5, 100) },
        { name: "Helium",        icon: "🎈", tip: "Like you just breathed in a balloon.",              values: vc(5, 9, 0, 50, 0, 0, 0, 5, 100) },
        { name: "Deep Voice",    icon: "🗣", tip: "A lower, movie-trailer voice.",                     values: vc(-4, -4, 0, 50, 0, 0, 0, 5, 100) },
        { name: "Giant",         icon: "🗿", tip: "Huge and booming.",                                 values: vc(-8, -8, 0, 50, 0, 10, 0, 5, 100) },
        { name: "Monster",       icon: "👹", tip: "Growling, gravelly and menacing.",                  values: vc(-10, -10, 0, 50, 0, 45, 10, 3, 100) },
        { name: "Robot",         icon: "🤖", tip: "Metallic, buzzing machine voice.",                  values: vc(0, 0, 100, 60, 0, 15, 0, 5, 100) },
        { name: "Alien",         icon: "👽", tip: "Wobbly, otherworldly and ringing.",                 values: vc(4, 3, 50, 420, 0, 0, 35, 7, 100) },
        { name: "Walkie-talkie", icon: "📟", tip: "Crunchy two-way radio.",                            values: vc(0, 0, 0, 50, 100, 55, 0, 5, 100) },
        { name: "Telephone",     icon: "☎", tip: "Thin, old-fashioned phone line.",                    values: vc(0, 0, 0, 50, 100, 15, 0, 5, 100) },
        { name: "Old Radio",     icon: "📻", tip: "Warbly broadcast from the 1940s.",                   values: vc(0, -3, 0, 50, 80, 30, 8, 0.8, 100) },
        { name: "Underwater",    icon: "🌊", tip: "Muffled and bubbling.",                             values: vc(-1, -10, 0, 50, 0, 0, 70, 2.5, 100) },
        { name: "Ghost",         icon: "👻", tip: "Eerie and wavering. Add Reverb for extra spookiness.", values: vc(-3, -2, 0, 50, 0, 0, 40, 1.2, 70) }
    ]
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
