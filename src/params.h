// Single source of truth for every user-adjustable setting.
// The UI, presets, settings file and DSP all refer to these keys.
#pragma once

namespace mf {

enum ParamId {
    InputChannel,   // 0 = left, 1 = right, 2 = both (mixed)
    InputGain,      // dB

    // "Your setup" profile (UI / setup only; the DSP ignores these).
    MicType,        // 0 dynamic, 1 condenser, 2 headset, 3 laptop/webcam built-in
    NeedOthers,     // other people talk nearby -> voice separation
    NeedNoise,      // background noise -> AI noise removal etc.
    NeedBroadcast,  // broadcast-quality sound -> de-esser, stronger compression

    NoiseAdded,     // optional effect shown in the chain (UI only)
    NoiseOn,
    NoiseAmount,    // % wet (RNNoise)

    LowCutAdded,
    LowCutOn,
    LowCutFreq,     // Hz
    LowCutSteep,    // 0 = 12 dB/oct, 1 = 24 dB/oct

    GateAdded,
    GateOn,
    GateMode,       // 0 = gate, 1 = expander
    GateThreshold,  // dB (detector level that opens the gate)
    GateRange,      // dB (max attenuation, negative)
    GateRatio,      // expander ratio (1:n below threshold)
    GateAttack,     // ms
    GateHold,       // ms
    GateRelease,    // ms
    GateHysteresis, // dB (close threshold = threshold - hysteresis)
    GateLookahead,  // ms (delays audio so word starts aren't clipped)
    GateFocus,      // detector filter on/off
    GateScLow,      // Hz, detector high-pass
    GateScHigh,     // Hz, detector low-pass
    GateCloseCheck, // closeness check on/off (open only for close, bass-heavy sound)
    GateTilt,       // dB: bass (100-300 Hz) minus treble (1-4 kHz) threshold for the closeness check
    GateTiltDir,    // 0 = your voice is bassier than others (open above GateTilt), 1 = brighter (open below)

    AutoLevelAdded,
    AutoLevelOn,
    AutoLevelTarget,  // dB RMS the speech is steered towards
    AutoLevelMaxGain, // dB, most it will boost or cut
    AutoLevelSpeed,   // 0 slow, 1 medium, 2 fast

    CompAdded,
    CompOn,
    CompThreshold,  // dB
    CompRatio,      // :1
    CompAttack,     // ms
    CompRelease,    // ms
    CompKnee,       // dB
    CompMakeup,     // dB

    DeessAdded,
    DeessOn,
    DeessFreq,      // Hz
    DeessThreshold, // dB
    DeessMax,       // dB max reduction

    VcAdded,        // voice changer
    VcOn,
    VcPitch,        // semitones
    VcCharacter,    // dB tilt: + brighter/smaller, - darker/bigger (formant-like)
    VcRobot,        // % ring modulation
    VcRobotFreq,    // Hz
    VcRadio,        // % band-limited "radio/phone" sound
    VcDrive,        // % distortion
    VcWobble,       // % vibrato depth
    VcWobbleRate,   // Hz
    VcMix,          // % wet

    EqAdded,
    EqOn,
    Eq1Freq, Eq1Gain, Eq1Q, // low shelf
    Eq2Freq, Eq2Gain, Eq2Q, // bell
    Eq3Freq, Eq3Gain, Eq3Q, // bell
    Eq4Freq, Eq4Gain, Eq4Q, // bell
    Eq5Freq, Eq5Gain, Eq5Q, // high shelf

    ToneAdded,
    ToneOn,
    ToneBass,       // dB, low shelf 120 Hz
    TonePresence,   // dB, bell 4.5 kHz
    ToneAir,        // dB, high shelf 10 kHz

    PluginsAdded,   // third-party VST3 / LV2 plugins stage
    PluginsOn,

    WarmthAdded,
    WarmthOn,
    WarmthAmount,   // % saturation drive

    ExciterAdded,
    ExciterOn,
    ExciterAmount,  // %
    ExciterFreq,    // Hz

    DoublerAdded,
    DoublerOn,
    DoublerWidth,   // % modulation depth / spread
    DoublerMix,     // %

    EchoAdded,
    EchoOn,
    EchoTime,       // ms
    EchoFeedback,   // %
    EchoMix,        // %
    EchoTone,       // Hz low-pass in the repeats

    ReverbAdded,
    ReverbOn,
    ReverbSize,     // %
    ReverbDamp,     // %
    ReverbMix,      // %

    LimiterOn,
    LimiterCeiling, // dB
    OutputGain,     // dB
    Bypass,         // 1 = only channel select + input gain
    Mute,           // 1 = send silence (fades out/in without clicks); never saved
    PARAM_COUNT
};

struct ParamDef {
    const char *key;
    float def, min, max;
};

// Sensible starting values for a voice; Setup tunes the important ones to each user.
inline constexpr ParamDef kParams[PARAM_COUNT] = {
    {"inputChannel",   0,     0,    2},
    {"inputGain",      15,   -12,   36},

    {"micType",        0,     0,    3},
    {"needOthers",     1,     0,    1},
    {"needNoise",      0,     0,    1},
    {"needBroadcast",  0,     0,    1},

    {"noiseAdded",     0,     0,    1},
    {"noiseOn",        1,     0,    1},
    {"noiseAmount",    100,   0,    100},

    {"lowCutAdded",    1,     0,    1},
    {"lowCutOn",       1,     0,    1},
    {"lowCutFreq",     80,    20,   300},
    {"lowCutSteep",    1,     0,    1},

    {"gateAdded",      1,     0,    1},
    {"gateOn",         1,     0,    1},
    {"gateMode",       0,     0,    1},
    {"gateThreshold", -20,   -80,   0},
    {"gateRange",     -30,   -80,   0},
    {"gateRatio",      3,     1.2f, 10},
    {"gateAttack",     2,     0.1f, 100},
    {"gateHold",       150,   0,    1000},
    {"gateRelease",    250,   10,   2000},
    {"gateHysteresis", 4,     0,    20},
    {"gateLookahead",  3,     0,    10},
    {"gateFocus",      1,     0,    1},
    {"gateScLow",      70,    20,   500},
    {"gateScHigh",     500,   150,  8000},
    {"gateCloseCheck", 0,     0,    1},
    {"gateTilt",       6,    -30,   40},
    {"gateTiltDir",    0,     0,    1},

    {"autoLevelAdded", 0,     0,    1},
    {"autoLevelOn",    1,     0,    1},
    {"autoLevelTarget",-18,  -36,  -6},
    {"autoLevelMaxGain",12,   0,    24},
    {"autoLevelSpeed", 1,     0,    2},

    {"compAdded",      1,     0,    1},
    {"compOn",         1,     0,    1},
    {"compThreshold", -24,   -60,   0},
    {"compRatio",      3,     1,    20},
    {"compAttack",     8,     0.1f, 100},
    {"compRelease",    150,   10,   1000},
    {"compKnee",       6,     0,    24},
    {"compMakeup",     6,     0,    30},

    {"deessAdded",     0,     0,    1},
    {"deessOn",        1,     0,    1},
    {"deessFreq",      6000,  2500, 12000},
    {"deessThreshold",-30,   -60,   0},
    {"deessMax",       8,     0,    24},

    {"vcAdded",        0,     0,    1},
    {"vcOn",           1,     0,    1},
    {"vcPitch",        0,    -12,   12},
    {"vcCharacter",    0,    -12,   12},
    {"vcRobot",        0,     0,    100},
    {"vcRobotFreq",    50,    10,   1000},
    {"vcRadio",        0,     0,    100},
    {"vcDrive",        0,     0,    100},
    {"vcWobble",       0,     0,    100},
    {"vcWobbleRate",   5,     0.5f, 12},
    {"vcMix",          100,   0,    100},

    {"eqAdded",        0,     0,    1},
    {"eqOn",           1,     0,    1},
    {"eq1Freq",        100,   20,   20000}, {"eq1Gain", 0, -18, 18}, {"eq1Q", 0.7f, 0.2f, 10},
    {"eq2Freq",        300,   20,   20000}, {"eq2Gain", 0, -18, 18}, {"eq2Q", 1.0f, 0.2f, 10},
    {"eq3Freq",        1000,  20,   20000}, {"eq3Gain", 0, -18, 18}, {"eq3Q", 1.0f, 0.2f, 10},
    {"eq4Freq",        3000,  20,   20000}, {"eq4Gain", 0, -18, 18}, {"eq4Q", 1.0f, 0.2f, 10},
    {"eq5Freq",        8000,  20,   20000}, {"eq5Gain", 0, -18, 18}, {"eq5Q", 0.7f, 0.2f, 10},

    {"toneAdded",      1,     0,    1},
    {"toneOn",         1,     0,    1},
    {"toneBass",       0,    -12,   12},
    {"tonePresence",   0,    -12,   12},
    {"toneAir",        0,    -12,   12},

    {"pluginsAdded",   0,     0,    1},
    {"pluginsOn",      1,     0,    1},

    {"warmthAdded",    0,     0,    1},
    {"warmthOn",       1,     0,    1},
    {"warmthAmount",   30,    0,    100},

    {"exciterAdded",   0,     0,    1},
    {"exciterOn",      1,     0,    1},
    {"exciterAmount",  30,    0,    100},
    {"exciterFreq",    4000,  1500, 10000},

    {"doublerAdded",   0,     0,    1},
    {"doublerOn",      1,     0,    1},
    {"doublerWidth",   50,    0,    100},
    {"doublerMix",     40,    0,    100},

    {"echoAdded",      0,     0,    1},
    {"echoOn",         1,     0,    1},
    {"echoTime",       320,   20,   1500},
    {"echoFeedback",   35,    0,    90},
    {"echoMix",        25,    0,    100},
    {"echoTone",       4000,  800,  16000},

    {"reverbAdded",    0,     0,    1},
    {"reverbOn",       1,     0,    1},
    {"reverbSize",     50,    0,    100},
    {"reverbDamp",     50,    0,    100},
    {"reverbMix",      20,    0,    100},

    {"limiterOn",      1,     0,    1},
    {"limiterCeiling", -1,   -12,   0},
    {"outputGain",     0,    -24,   24},
    {"bypass",         0,     0,    1},
    {"mute",           0,     0,    1},
};

} // namespace mf

namespace mf {
// Compile-time guard: every enum entry must line up with its row in kParams.
constexpr bool keyIs(int id, const char *k) {
    const char *a = kParams[id].key;
    while (*a && *a == *k) { ++a; ++k; }
    return *a == *k;
}
static_assert(keyIs(InputGain, "inputGain") && keyIs(GateAdded, "gateAdded") && keyIs(GateTiltDir, "gateTiltDir") &&
              keyIs(AutoLevelSpeed, "autoLevelSpeed") && keyIs(CompMakeup, "compMakeup") && keyIs(VcMix, "vcMix") &&
              keyIs(Eq5Q, "eq5Q") && keyIs(ToneAir, "toneAir") && keyIs(PluginsOn, "pluginsOn") && keyIs(ReverbMix, "reverbMix") &&
              keyIs(Bypass, "bypass") && keyIs(Mute, "mute"),
              "kParams rows are out of step with the ParamId enum");
} // namespace mf
