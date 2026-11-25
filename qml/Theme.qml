pragma Singleton
import QtQuick

QtObject {
    readonly property color bg: "#121419"
    readonly property color panel: "#1a1d24"
    readonly property color card: "#1f232b"
    readonly property color cardHi: "#262b35"
    readonly property color border: "#2d323d"
    readonly property color track: "#2b303b"
    readonly property color text: "#e8eaf0"
    readonly property color textDim: "#9aa2b3"
    readonly property color textFaint: "#6b7385"

    readonly property color input: "#5aa9ff"
    readonly property color lowcut: "#9b8cff"
    readonly property color gate: "#3fdc98"
    readonly property color comp: "#ffb347"
    readonly property color tone: "#ff7aa8"
    readonly property color output: "#56d4e8"
    readonly property color danger: "#ff5c5c"
    readonly property color warn: "#ffd24d"

    readonly property int radius: 12
    readonly property string mono: "monospace"

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
