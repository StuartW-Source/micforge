// Voice-separation analysis. Given recordings of what the gate hears:
//   room   - nobody talking (may be near-silent if AI noise removal runs first)
//   others - other people talking (optional; any number of samples concatenated)
//   you    - the user talking
// find the detector band, level threshold and closeness threshold that let YOUR voice open the
// gate while room noise and other voices keep it closed.
#pragma once

#include "dsp.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace mf {

struct SeparationResult {
    bool heardYou = false;      // false = no clear speech in the "you" recording
    bool hadOthers = false;     // an "other voices" recording was available
    bool focus = true;          // use a detector band at all
    float scLow = 70, scHigh = 500;
    float roomDb = -90;         // detector level of room noise (95th percentile, floored at -90)
    float othersDb = -200;      // detector level of other voices' loud moments (98th percentile)
    float youDb = -120;         // detector level of your quieter syllables (20th percentile of speech)
    float separation = 0;       // youDb - max(othersDb, roomDb)
    float broadbandSeparation = 0;
    float threshold = -20, hysteresis = 4;
    float youPeakDb = -120;     // broadband speech peaks (for gain / compressor)
    int quality = 0;            // 0 poor, 1 ok, 2 good, 3 excellent

    // Closeness (bass 100-300 Hz vs treble 1-4 kHz). Level-independent.
    // The check only decides when the gate OPENS, so it compares your TYPICAL closeness (median:
    // your vowels) with the EXTREME of the other voices (their most "you-like" 5%).
    float youTilt = 0;          // median of your speech
    float othersTilt = 0;       // others' extreme in the direction that matters (95th or 5th percentile)
    float tiltGap = 0;          // how far your typical closeness is beyond the others' extreme (>0 = usable)
    bool tiltMeasured = false;  // others recorded and clearly different -> threshold placed in the gap
    bool tiltBrighter = false;  // your voice is BRIGHTER than others (open below the threshold)
    float tiltThreshold = 0;    // suggested closeness threshold (from the gap, or from your voice alone)

    // Broadband levels per recording, used to spot a recording that went wrong.
    float roomBroad95 = -120, roomBroad99 = -120; // typical and loudest moments of the "quiet" room
    float youBroad90 = -120;                      // your speech
    float othersBroad90 = -120;                   // other voices
};

// How each recording looks, so the user can redo just the one that went wrong.
struct StepCheck {
    enum Status { Ok, Warn, Bad, None };
    Status status = None;
    const char *note = "";
};

inline StepCheck checkRoom(const SeparationResult &r) {
    if (r.roomBroad99 > r.youBroad90 - 10)
        return {StepCheck::Warn, "There were loud sounds during the quiet step (talking, a bump or typing?). Redo it in silence."};
    return {StepCheck::Ok, "Nice and quiet."};
}

inline StepCheck checkYou(const SeparationResult &r) {
    if (!r.heardYou) return {StepCheck::Bad, "MicForge couldn't hear you clearly. Check the mic and input, then redo this step."};
    if (r.youBroad90 < r.roomBroad95 + 15)
        return {StepCheck::Warn, "Your voice was only a little louder than the room. Move closer to the mic and redo."};
    return {StepCheck::Ok, "Clear and strong."};
}

inline StepCheck checkOthers(const SeparationResult &r) {
    if (!r.hadOthers) return {StepCheck::None, "Not recorded. Optional: record it when someone is talking nearby."};
    if (r.othersBroad90 < r.roomBroad95 + 6)
        return {StepCheck::Warn, "Hardly any voices were heard. Redo it while someone is actually talking."};
    if (r.youTilt != 0 && r.othersTilt >= r.youTilt - 1 && r.othersBroad90 > r.youBroad90 - 6)
        return {StepCheck::Warn, "These voices sounded as close and loud as you. Was it you talking? Redo it while staying silent."};
    return {StepCheck::Ok, "Other voices captured."};
}

namespace detail {

inline float percentile(std::vector<float> v, float p) {
    if (v.empty()) return -120;
    std::sort(v.begin(), v.end());
    const size_t i = size_t(std::clamp(std::lround(p * (v.size() - 1)), 0L, long(v.size() - 1)));
    return v[i];
}

constexpr float kRoomFloor = -90; // AI noise removal can make the room digitally silent

// Detector level per 10 ms frame, in dB.
inline std::vector<float> frameLevels(const std::vector<float> &x, float fs, bool focus, float lo, float hi) {
    GateDetector d;
    d.configure(fs, focus, lo, hi);
    const size_t frame = size_t(fs / 100);
    std::vector<float> out;
    out.reserve(x.size() / frame + 1);
    float mx = 0;
    size_t k = 0;
    const size_t settle = std::min(x.size(), size_t(fs / 20)); // let filters settle (50 ms)
    for (size_t i = 0; i < settle; ++i) d.process(x[i]);
    for (size_t i = settle; i < x.size(); ++i) {
        mx = std::max(mx, d.process(x[i]));
        if (++k == frame) {
            out.push_back(linToDb(mx));
            mx = 0;
            k = 0;
        }
    }
    return out;
}

// Which frames contain speech: above the room AND within 25 dB of the recording's loud parts.
// The second rule keeps near-silent gaps out even when the room measures as digital silence.
inline float speechGate(const std::vector<float> &levels, float room) {
    return std::max(room + 6.0f, percentile(levels, 0.95f) - 25.0f);
}

struct BandScore {
    bool focus;
    float lo, hi, room, others, you, sep;
    bool heard;
};

inline BandScore scoreBand(const std::vector<float> &room, const std::vector<float> &others,
                           const std::vector<float> &you, float fs, bool focus, float lo, float hi) {
    BandScore s{focus, lo, hi, kRoomFloor, -200, -120, -200, false};
    const auto r = frameLevels(room, fs, focus, lo, hi);
    const auto y = frameLevels(you, fs, focus, lo, hi);
    s.room = std::max(kRoomFloor, percentile(r, 0.95f));
    if (!others.empty()) s.others = percentile(frameLevels(others, fs, focus, lo, hi), 0.98f);
    const float gate = speechGate(y, s.room);
    std::vector<float> active;
    for (float v : y)
        if (v > gate) active.push_back(v);
    s.heard = !y.empty() && active.size() > y.size() / 6 && percentile(y, 0.95f) > s.room + 12;
    s.you = s.heard ? percentile(active, 0.20f) : -120;
    s.sep = s.you - std::max(s.others, s.room);
    return s;
}

struct FrameInfo { float level, tilt; };

// Broadband level and closeness per 10 ms frame.
inline std::vector<FrameInfo> frameTilts(const std::vector<float> &x, float fs) {
    GateDetector d;
    d.configure(fs, false, 20, 8000);
    TiltDetector t;
    t.configure(fs);
    const size_t frame = size_t(fs / 100);
    std::vector<FrameInfo> out;
    float mx = 0;
    size_t k = 0;
    const size_t settle = std::min(x.size(), size_t(fs / 20));
    for (size_t i = 0; i < settle; ++i) { d.process(x[i]); t.process(x[i]); }
    for (size_t i = settle; i < x.size(); ++i) {
        mx = std::max(mx, d.process(x[i]));
        t.process(x[i]);
        if (++k == frame) {
            out.push_back({linToDb(mx), t.tiltDb()});
            mx = 0;
            k = 0;
        }
    }
    return out;
}

inline std::vector<float> speechTilts(const std::vector<float> &x, float fs, float roomBroad) {
    const auto f = frameTilts(x, fs);
    std::vector<float> lv;
    for (const auto &fi : f) lv.push_back(fi.level);
    const float gate = speechGate(lv, roomBroad);
    std::vector<float> v;
    for (const auto &fi : f)
        if (fi.level > gate) v.push_back(fi.tilt);
    return v;
}

} // namespace detail

inline SeparationResult analyseSeparation(const std::vector<float> &room, const std::vector<float> &others,
                                          const std::vector<float> &you, float fs = 48000) {
    using namespace detail;
    SeparationResult res;
    res.hadOthers = !others.empty();

    // Broadband levels of each recording (for the per-step checks).
    {
        const auto rl = frameLevels(room, fs, false, 20, 8000);
        res.roomBroad95 = std::max(kRoomFloor, percentile(rl, 0.95f));
        res.roomBroad99 = std::max(kRoomFloor, percentile(rl, 0.99f));
        res.youBroad90 = percentile(frameLevels(you, fs, false, 20, 8000), 0.90f);
        if (res.hadOthers) res.othersBroad90 = percentile(frameLevels(others, fs, false, 20, 8000), 0.90f);
    }

    // Broadband speech peak level (what the input meter shows).
    {
        std::vector<float> peaks;
        const size_t frame = size_t(fs / 100);
        for (size_t i = 0; i + frame <= you.size(); i += frame) {
            float m = 0;
            for (size_t k = 0; k < frame; ++k) m = std::max(m, std::fabs(you[i + k]));
            peaks.push_back(linToDb(m));
        }
        res.youPeakDb = percentile(peaks, 0.90f);
    }

    // Candidate detector bands. Low-pass bands favour a close, low voice (proximity effect).
    std::vector<BandScore> scores;
    scores.push_back(scoreBand(room, others, you, fs, false, 20, 8000));
    res.broadbandSeparation = scores.back().sep;
    for (float lo : {20.f, 60.f, 90.f, 120.f})
        for (float hi : {150.f, 200.f, 250.f, 300.f, 400.f, 500.f, 700.f, 1000.f, 2000.f, 8000.f})
            if (hi > lo * 1.8f) scores.push_back(scoreBand(room, others, you, fs, true, lo, hi));

    // Best separation wins. Within 0.5 dB, prefer the wider band (more robust for word onsets).
    const BandScore *best = nullptr;
    for (const auto &s : scores) {
        if (!s.heard) continue;
        if (!best || s.sep > best->sep + 0.5f ||
            (std::fabs(s.sep - best->sep) <= 0.5f && (s.hi - s.lo) > (best->hi - best->lo)))
            best = &s;
    }
    if (!best) return res;

    res.heardYou = true;
    res.focus = best->focus;
    res.scLow = best->lo;
    res.scHigh = best->hi;
    res.roomDb = best->room;
    res.othersDb = best->others;
    res.youDb = best->you;
    res.separation = best->sep;
    res.quality = best->sep >= 15 ? 3 : best->sep >= 9 ? 2 : best->sep >= 4 ? 1 : 0;

    // Closeness.
    {
        std::vector<float> lv;
        for (const auto &fi : frameTilts(room, fs)) lv.push_back(fi.level);
        const float roomBroad = std::max(kRoomFloor, percentile(lv, 0.95f));
        const auto yt = speechTilts(you, fs, roomBroad);
        if (yt.size() > 20) {
            res.youTilt = percentile(yt, 0.50f);
            // From your voice alone: a little below your typical vowels (others usually sound thinner).
            res.tiltThreshold = std::round((percentile(yt, 0.30f) - 2.0f) * 2) / 2;
            if (res.hadOthers) {
                const auto ot = speechTilts(others, fs, roomBroad);
                if (ot.size() > 20) {
                    const float oHi = percentile(ot, 0.95f), oLo = percentile(ot, 0.05f);
                    const float gapBass = res.youTilt - oHi;   // you bassier than their bassiest moments
                    const float gapBright = oLo - res.youTilt; // you brighter than their brightest moments
                    if (gapBass >= gapBright) { res.othersTilt = oHi; res.tiltGap = gapBass; }
                    else { res.othersTilt = oLo; res.tiltGap = gapBright; res.tiltBrighter = true; }
                    if (res.tiltGap >= 2) {
                        res.tiltMeasured = true;
                        const float step = std::min(3.0f, res.tiltGap * 0.4f);
                        res.tiltThreshold = std::round((res.tiltBrighter ? res.othersTilt - step : res.othersTilt + step) * 2) / 2;
                    } else {
                        res.tiltBrighter = false;
                    }
                }
            }
        }
    }

    // Level threshold.
    const float top = std::max(best->others, best->room);
    if (best->sep > 3) {
        res.threshold = top + best->sep * 0.5f;
        res.hysteresis = std::clamp(best->sep * 0.35f, 2.0f, 6.0f);
        res.hysteresis = std::min(res.hysteresis, best->sep * 0.5f - 1.0f);
    } else if (res.tiltMeasured) {
        // Others are as loud as you, but the closeness check can tell you apart:
        // keep the level threshold just above the room and below your voice.
        res.threshold = std::min(best->you - 4.0f, std::max(best->room + 6.0f, best->you - 10.0f));
        res.hysteresis = 3.0f;
    } else {
        // No clean gap either way: favour keeping others out, accept some clipped quiet words.
        res.threshold = std::min(top + 1.5f, best->you - 1.0f);
        res.hysteresis = 1.0f;
    }
    res.threshold = std::round(std::clamp(res.threshold, -80.0f, 0.0f) * 2) / 2;
    res.hysteresis = std::round(std::max(res.hysteresis, 0.5f) * 2) / 2;
    return res;
}

} // namespace mf
