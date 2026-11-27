// Real-time voice processing chain. Runs on the PipeWire audio thread:
// no allocation, no locks, no syscalls inside process().
#pragma once

#include "params.h"

#include "optional/rnnoise_api.h" // loaded at run time (optional dependency)

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <vector>

#if defined(__x86_64__) || defined(__i386__)
#include <xmmintrin.h>
#endif

namespace mf {

inline float dbToLin(float db) { return std::pow(10.0f, db / 20.0f); }
inline float linToDb(float l) { return 20.0f * std::log10(std::max(l, 1e-9f)); }

// Shared settings: written by the UI thread, read by the audio thread.
struct Params {
    std::array<std::atomic<float>, PARAM_COUNT> v;
    std::atomic<uint32_t> version{1};

    Params() {
        for (int i = 0; i < PARAM_COUNT; ++i) v[i].store(kParams[i].def);
    }
    void set(int id, float value) {
        value = std::clamp(value, kParams[id].min, kParams[id].max);
        v[id].store(value, std::memory_order_relaxed);
        version.fetch_add(1, std::memory_order_release);
    }
    float get(int id) const { return v[id].load(std::memory_order_relaxed); }
};

// Meter values: the audio thread keeps the maximum since the UI last read it.
struct Meters {
    std::atomic<float> inPeak{-120};       // dBFS after input gain
    std::atomic<float> noiseVad{0};        // RNNoise voice probability 0..1
    std::atomic<float> gateLevel{-120};    // gate detector level, dB
    std::atomic<float> gateReduction{0};   // dB of attenuation (positive)
    std::atomic<float> gateOpen{0};        // 1 if open at any point
    std::atomic<float> gateTilt{-120};
    // Why the gate is open or closed (highest during the block): 0 = gate off, 1 = closed: too quiet,
    // 2 = closed: steady background noise raised the threshold, 3 = closed: not a voice (AI voice
    // detector), 4 = closed: loud enough but not close enough, 5 = open.
    std::atomic<int> gateReason{0};
    std::atomic<float> gateFloor{-120};    // steady background level the gate measured, dB (-120 = none)
    std::atomic<float> gateOpenAt{-120};
    std::atomic<float> humBase{0};         // mains frequency Hum Removal is working on (0 = none yet)   // threshold in use (raised above the setting by background noise)     // closeness (bass minus treble, dB) of current sound; -120 = silence
    std::atomic<float> compLevel{-120};    // compressor detector input, dB
    std::atomic<float> compReduction{0};   // dB (positive)
    std::atomic<float> deessReduction{0};  // dB (positive)
    std::atomic<float> outPeak{-120};      // dBFS final output
    std::atomic<float> limReduction{0};    // dB (positive)
    std::atomic<float> autoLevelGain{0};   // dB currently applied by Auto Level (+ boost / - cut)

    static void bump(std::atomic<float> &a, float v) {
        if (v > a.load(std::memory_order_relaxed)) a.store(v, std::memory_order_relaxed);
    }
};

// Records the signal the gate sees, for the voice-separation setup.
// The UI thread arms it; the audio thread appends until full.
struct Recorder {
    std::vector<float> buf;
    std::atomic<size_t> pos{0};
    std::atomic<bool> active{false};
    explicit Recorder(size_t seconds = 30) : buf(seconds * 48000) {}
    inline void push(float x) {
        if (!active.load(std::memory_order_relaxed)) return;
        const size_t p = pos.load(std::memory_order_relaxed);
        if (p < buf.size()) {
            buf[p] = x;
            pos.store(p + 1, std::memory_order_release);
        }
    }
};

struct Biquad {
    enum Type { LP, HP, Peak, LowShelf, HighShelf, Notch };
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    double z1 = 0, z2 = 0;

    inline float process(float x) {
        double y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return float(y);
    }
    void reset() { z1 = z2 = 0; }

    // RBJ audio-EQ cookbook formulas.
    void set(Type t, double fs, double f, double q, double gainDb = 0) {
        f = std::clamp(f, 5.0, fs * 0.45);
        const double A = std::pow(10.0, gainDb / 40.0);
        const double w = 2.0 * M_PI * f / fs;
        const double cw = std::cos(w), sw = std::sin(w), al = sw / (2.0 * q);
        double B0, B1, B2, A0, A1, A2;
        switch (t) {
        case LP:
            B0 = (1 - cw) / 2; B1 = 1 - cw; B2 = (1 - cw) / 2;
            A0 = 1 + al; A1 = -2 * cw; A2 = 1 - al;
            break;
        case HP:
            B0 = (1 + cw) / 2; B1 = -(1 + cw); B2 = (1 + cw) / 2;
            A0 = 1 + al; A1 = -2 * cw; A2 = 1 - al;
            break;
        case Peak:
            B0 = 1 + al * A; B1 = -2 * cw; B2 = 1 - al * A;
            A0 = 1 + al / A; A1 = -2 * cw; A2 = 1 - al / A;
            break;
        case Notch:
            B0 = 1; B1 = -2 * cw; B2 = 1;
            A0 = 1 + al; A1 = -2 * cw; A2 = 1 - al;
            break;
        case LowShelf: {
            const double s = 2 * std::sqrt(A) * al;
            B0 = A * ((A + 1) - (A - 1) * cw + s);
            B1 = 2 * A * ((A - 1) - (A + 1) * cw);
            B2 = A * ((A + 1) - (A - 1) * cw - s);
            A0 = (A + 1) + (A - 1) * cw + s;
            A1 = -2 * ((A - 1) + (A + 1) * cw);
            A2 = (A + 1) + (A - 1) * cw - s;
            break;
        }
        case HighShelf:
        default: {
            const double s = 2 * std::sqrt(A) * al;
            B0 = A * ((A + 1) + (A - 1) * cw + s);
            B1 = -2 * A * ((A - 1) + (A + 1) * cw);
            B2 = A * ((A + 1) + (A - 1) * cw - s);
            A0 = (A + 1) - (A - 1) * cw + s;
            A1 = 2 * ((A - 1) - (A + 1) * cw);
            A2 = (A + 1) - (A - 1) * cw - s;
            break;
        }
        }
        b0 = B0 / A0; b1 = B1 / A0; b2 = B2 / A0; a1 = A1 / A0; a2 = A2 / A0;
    }
};

// One-pole smoothing coefficient for a time constant in milliseconds.
inline float onePole(float ms, float fs) {
    return ms <= 0 ? 0.0f : std::exp(-1.0f / (0.001f * ms * fs));
}

// ---------------------------------------------------------------- smoothing (no clicks on changes)

// A value that glides to its target (one-pole, ~10 ms) instead of jumping.
struct Smoothed {
    float cur = 0, target = 0, k = 0.0021f; // k = 1 - onePole(10 ms, 48 kHz)
    void configure(float fs, float ms = 10.0f) { k = 1.0f - onePole(ms, fs); }
    void set(float v, bool jump) { target = v; if (jump) cur = v; }
    inline float next() {
        if (cur != target) {
            cur += k * (target - cur);
            if (std::fabs(target - cur) < 1e-6f * (1.0f + std::fabs(target))) cur = target;
        }
        return cur;
    }
};

// A biquad whose frequency, Q and gain glide to new settings (coefficients updated every 16 samples).
struct SmoothBiquad {
    Biquad bq;
    Biquad::Type type = Biquad::Peak;
    double f = 1000, q = 0.7071, g = 0, tf = 1000, tq = 0.7071, tg = 0, k = 0.022, fs = 48000;
    int tick = 0;
    bool init = false;
    void set(Biquad::Type t, double sampleRate, double freq, double Q, double gainDb, bool jump) {
        fs = sampleRate;
        k = 1.0 - std::exp(-16.0 / (0.015 * fs)); // ~15 ms glide
        tf = freq; tq = Q; tg = gainDb;
        if (jump || !init || t != type) {
            type = t; f = tf; q = tq; g = tg; init = true;
            bq.set(type, fs, f, q, g);
        }
    }
    bool moving() const { return f != tf || q != tq || g != tg; }
    // True if the filter currently does anything (or is still gliding back to flat).
    bool audible() const { return moving() || std::fabs(g) > 0.05 || (type != Biquad::Peak && type != Biquad::LowShelf && type != Biquad::HighShelf); }
    void reset() { bq.reset(); }
    inline float process(float x) {
        if (moving() && (++tick & 15) == 0) step();
        return bq.process(x);
    }
    void step() {
        f *= std::pow(tf / f, k);
        q *= std::pow(tq / q, k);
        g += k * (tg - g);
        if (std::fabs(f / tf - 1) < 1e-4) f = tf;
        if (std::fabs(q / tq - 1) < 1e-4) q = tq;
        if (std::fabs(g - tg) < 0.005) g = tg;
        bq.set(type, fs, f, q, g);
    }
};

// Estimates the peak of the continuous waveform between samples (4x oversampling with a windowed-sinc
// interpolator), so the limiter can catch "inter-sample" overs that would clip after conversion or
// encoding. The estimate is delayed by kHalf samples.
struct TruePeakDetector {
    static constexpr int kHalf = 8, kTaps = 2 * kHalf;
    float coef[3][kTaps] = {};
    std::array<float, kTaps> hist{};
    int pos = 0;
    TruePeakDetector() {
        for (int ph = 0; ph < 3; ++ph) {
            const double frac = (ph + 1) / 4.0;
            double sum = 0;
            for (int k = 0; k < kTaps; ++k) {
                const double d = (k - (kHalf - 1)) - frac; // distance from the interpolated point
                const double sinc = std::fabs(d) < 1e-12 ? 1.0 : std::sin(M_PI * d) / (M_PI * d);
                const double w = 0.42 + 0.5 * std::cos(M_PI * d / kHalf) + 0.08 * std::cos(2 * M_PI * d / kHalf);
                coef[ph][k] = float(sinc * w);
                sum += coef[ph][k];
            }
            for (int k = 0; k < kTaps; ++k) coef[ph][k] = float(coef[ph][k] / sum);
        }
    }
    void reset() { hist.fill(0); pos = 0; }
    // Push one sample; returns the largest |value| at and just after sample (now - kHalf + 1).
    inline float process(float x) {
        hist[size_t(pos)] = x;
        pos = (pos + 1) % kTaps;
        float pk = std::fabs(hist[size_t((pos + kHalf - 1) % kTaps)]); // the sample itself
        for (int ph = 0; ph < 3; ++ph) {
            float acc = 0;
            for (int k = 0; k < kTaps; ++k) acc += coef[ph][k] * hist[size_t((pos + k) % kTaps)];
            pk = std::max(pk, std::fabs(acc));
        }
        return pk;
    }
};

// Removes mains hum: narrow notches (3 Hz wide) on 50 or 60 Hz and its harmonics. In automatic mode it
// only commits to 50 or 60 Hz when those frequencies are narrow, steady peaks (hum), checked over two
// consecutive seconds, so a voice's own harmonics (which wander in pitch) can't trigger it.
struct HumRemover {
    static constexpr int kMax = 10;
    SmoothBiquad notch[kMax];
    float fs = 48000, base = 0;     // 0 = not detected yet (automatic mode): passes audio through
    int harmonics = 6, mode = 0;    // mode: 0 auto, 1 = 50 Hz, 2 = 60 Hz
    // Detection: Goertzel at k*f and k*f +- 8 Hz for k = 1..3, f = 50/60, over 1 s blocks.
    struct G { double c = 0, s1 = 0, s2 = 0; void set(double hz, double fs) { c = 2 * std::cos(2 * M_PI * hz / fs); } inline void push(float x) { const double s0 = x + c * s1 - s2; s2 = s1; s1 = s0; } double power() const { return s1 * s1 + s2 * s2 - c * s1 * s2; } void clear() { s1 = s2 = 0; } };
    G g[2][3][3]; // [50/60][harmonic][centre, below, above]
    int count = 0, lastWinner = 0;

    void configure(float sampleRate, int freqMode, int harm, bool jump) {
        fs = sampleRate; harmonics = std::clamp(harm, 1, kMax); mode = freqMode;
        for (int b = 0; b < 2; ++b)
            for (int k = 0; k < 3; ++k) {
                const double f = (b ? 60.0 : 50.0) * (k + 1);
                g[b][k][0].set(f, fs); g[b][k][1].set(f - 8, fs); g[b][k][2].set(f + 8, fs);
            }
        if (mode == 1) setBase(50, jump);
        else if (mode == 2) setBase(60, jump);
    }
    void setBase(float f, bool jump) {
        const bool first = base == 0;
        base = f;
        for (int k = 0; k < kMax; ++k) {
            const double hz = f * (k + 1);
            notch[k].set(Biquad::Notch, fs, hz, hz / 3.0, 0, jump || first);
        }
    }
    void reset() { for (auto &n : notch) n.reset(); for (auto &a : g) for (auto &b : a) for (auto &c : b) c.clear(); count = 0; }
    void detect(float x) {
        for (auto &a : g) for (auto &b : a) for (auto &c : b) c.push(x);
        if (++count < int(fs)) return;
        count = 0;
        int winner = 0;
        double best = 0;
        for (int b = 0; b < 2; ++b) {
            int peaky = 0;
            double e = 0;
            for (int k = 0; k < 3; ++k) {
                const double c = g[b][k][0].power(), side = 0.5 * (g[b][k][1].power() + g[b][k][2].power());
                if (c > 10.0 * side && c > 1e-3) ++peaky; // 10 dB above its neighbours, and audible (about -70 dBFS)
                e += c;
            }
            if (peaky >= 2 && e > best) { best = e; winner = b ? 60 : 50; }
        }
        for (auto &a : g) for (auto &b : a) for (auto &c : b) c.clear();
        if (winner && winner == lastWinner && winner != int(base)) setBase(float(winner), false);
        lastWinner = winner;
    }
    inline float process(float x) {
        if (mode == 0) detect(x);
        if (base == 0) return x;
        for (int k = 0; k < harmonics; ++k) x = notch[k].process(x);
        return x;
    }
};

// Plosive guard: a "p" or "b" close to the mic hits it with a burst of very low frequencies. When the
// energy below ~120 Hz jumps 6+ dB above the voice band (300 Hz-3 kHz; a real close voice measured at
// most +1 dB), a low shelf at 200 Hz
// dips quickly (2 ms) and recovers (80 ms). A bass-heavy voice never has that much more sub-bass than
// voice-band energy, so it isn't touched.
struct PlosiveGuard {
    Biquad lfLp, midHp, midLp, shelf;
    float lfEnv = 0, midEnv = 0, red = 0, shelfDb = 0, aEnv = 0, rEnv = 0, aRed = 0, rRed = 0, fs = 48000;
    int tick = 0;
    void configure(float sampleRate) {
        fs = sampleRate;
        lfLp.set(Biquad::LP, fs, 120, 0.7071);
        midHp.set(Biquad::HP, fs, 300, 0.7071);
        midLp.set(Biquad::LP, fs, 3000, 0.7071);
        aEnv = onePole(1.0f, fs); rEnv = onePole(50.0f, fs);
        aRed = onePole(2.0f, fs); rRed = onePole(80.0f, fs);
        shelf.set(Biquad::LowShelf, fs, 200, 0.7071, -shelfDb);
    }
    void reset() { lfLp.reset(); midHp.reset(); midLp.reset(); shelf.reset(); lfEnv = midEnv = red = shelfDb = 0; }
    static inline void follow(float &env, float a, float att, float rel) { env = a > env ? att * env + (1 - att) * a : rel * env + (1 - rel) * a; }
    inline float process(float x) {
        follow(lfEnv, std::fabs(lfLp.process(x)), aEnv, rEnv);
        follow(midEnv, std::fabs(midLp.process(midHp.process(x))), aEnv, rEnv);
        const float ratio = linToDb(lfEnv) - linToDb(midEnv);
        const float want = linToDb(lfEnv) > -35.0f ? std::clamp((ratio - 6.0f) * 4.0f, 0.0f, 24.0f) : 0.0f;
        red = want > red ? want + (red - want) * aRed : want + (red - want) * rRed;
        if ((++tick & 15) == 0 && std::fabs(red - shelfDb) > 0.1f) {
            shelfDb = red;
            shelf.set(Biquad::LowShelf, fs, 200, 0.7071, -shelfDb);
        }
        return shelfDb > 0.05f ? shelf.process(x) : x;
    }
};

// Breath reduction. Every 20 ms it checks the sound for pitch (autocorrelation at 70-400 Hz on a 12 kHz
// copy), for hiss ("s": energy above 4.5 kHz) and for level against your recent speech. A breath is
// unpitched, not hissy, at least 10 dB below your speech, and lasts: after 160 ms of that in a row the
// sound is turned down (20 ms fade), and any pitched sound brings it straight back. Short unpitched sounds
// inside words ("h", "f", "t", under 160 ms) are never touched. Whispers are unpitched too, so this is
// opt-in. The first 160 ms of a breath pass (no extra delay is added to detect it).
struct BreathReducer {
    static constexpr int kFrame = 960, kDec = 4, kDecFrame = kFrame / kDec, kRunFrames = 8;
    Biquad aa, hp; // anti-alias for the 12 kHz copy; 4.5 kHz high-pass for hiss
    std::array<float, kDecFrame> dec{};
    int pos = 0, decPos = 0, run = 0;
    double eAll = 0, eHi = 0;
    float speechDb = -120, gain = 1, target = 1, depth = 0.25f, kGain = 0.001f, fs = 48000;
    void configure(float sampleRate, float amountDb) {
        fs = sampleRate;
        aa.set(Biquad::LP, fs, 3000, 0.7071);
        hp.set(Biquad::HP, fs, 4500, 0.7071);
        depth = dbToLin(-amountDb);
        kGain = 1.0f - onePole(20.0f, fs);
    }
    void reset() { aa.reset(); hp.reset(); pos = decPos = run = 0; eAll = eHi = 0; gain = target = 1; speechDb = -120; }
    // Normalised autocorrelation peak over pitch lags (0 = noise, 1 = perfectly periodic).
    float periodicity() const {
        double e0 = 0;
        for (float v : dec) e0 += double(v) * v;
        if (e0 < 1e-12) return 0;
        float best = 0;
        for (int lag = 12000 / 400; lag <= 12000 / 70; ++lag) {
            double c = 0, e1 = 0, e2 = 0;
            for (int k = 0; k + lag < kDecFrame; ++k) { c += double(dec[size_t(k)]) * dec[size_t(k + lag)]; e1 += double(dec[size_t(k)]) * dec[size_t(k)]; e2 += double(dec[size_t(k + lag)]) * dec[size_t(k + lag)]; }
            if (e1 > 0 && e2 > 0) best = std::max(best, float(c / std::sqrt(e1 * e2)));
        }
        return best;
    }
    void endFrame() {
        const float eDb = float(10.0 * std::log10(eAll / kFrame + 1e-14));
        const float hiShare = float(eHi / (eAll + 1e-14));
        const float r = periodicity();
        const bool voiced = r > 0.6f && eDb > -70.0f;
        if (voiced) speechDb = std::max(speechDb, eDb);
        else speechDb -= 0.1f; // the reference slowly forgets (5 dB per second without speech)
        const bool breathy = !voiced && r < 0.5f && hiShare < 0.5f && eDb > -75.0f && eDb < speechDb - 10.0f;
        run = breathy ? run + 1 : 0;
        target = run >= kRunFrames ? depth : 1.0f;
        if (voiced) { target = 1.0f; gain = std::max(gain, 0.5f); } // pitched sound returns quickly
        eAll = eHi = 0;
    }
    inline float process(float x) {
        const float h = hp.process(x);
        eAll += double(x) * x; eHi += double(h) * h;
        const float a = aa.process(x);
        if ((pos & (kDec - 1)) == 0) dec[size_t(decPos++)] = a;
        if (++pos == kFrame) { pos = 0; decPos = 0; endFrame(); }
        gain += kGain * (target - gain);
        return x * gain;
    }
};

// Crossfades a stage in and out (10 ms) when it's switched on or off, instead of cutting.
struct StageFade {
    float mix = 0, stepSize = 1.0f / 480.0f;
    bool on = false;
    void set(bool target, bool jump, float fs) {
        stepSize = 1.0f / (0.010f * fs);
        on = target;
        if (jump) mix = on ? 1.0f : 0.0f;
    }
    bool running() const { return on || mix > 0.0f; }
    bool starting() const { return on && mix == 0.0f; } // the stage's state should be cleared first
    inline float apply(float dry, float wet) {
        if (on) { if (mix < 1.0f) mix = std::min(1.0f, mix + stepSize); }
        else if (mix > 0.0f) mix = std::max(0.0f, mix - stepSize);
        return mix >= 1.0f ? wet : dry + mix * (wet - dry);
    }
};

// What the gate "hears": optional band filter + fast envelope follower.
// Shared by the live gate and the voice-separation analysis so they agree exactly.
struct GateDetector {
    Biquad hp[2], lp[2];
    bool focus = true, useHp = true, useLp = true;
    float env = 0, atk = 0, rel = 0;

    void configure(float fs, bool focusOn, float lowHz, float highHz) {
        focus = focusOn;
        useHp = lowHz > 21;
        useLp = highHz < 7900;
        for (auto &b : hp) b.set(Biquad::HP, fs, lowHz, 0.7071);
        for (auto &b : lp) b.set(Biquad::LP, fs, highHz, 0.7071);
        atk = onePole(0.3f, fs);
        rel = onePole(10.0f, fs);
    }
    void reset() {
        for (auto &b : hp) b.reset();
        for (auto &b : lp) b.reset();
        env = 0;
    }
    inline float process(float x) {
        if (focus) {
            if (useHp) x = hp[1].process(hp[0].process(x));
            if (useLp) x = lp[1].process(lp[0].process(x));
        }
        const float a = std::fabs(x);
        env = a > env ? atk * env + (1 - atk) * a : rel * env + (1 - rel) * a;
        return env;
    }
};

} // namespace mf
#include "effects.h"
#include "spectrum.h"
namespace mf {

// "Closeness" detector: compares bass (100-300 Hz) with treble (1-4 kHz) energy.
// A voice a few cm from a dynamic mic gets a big bass boost (proximity effect); a voice from
// across the room doesn't. The ratio barely changes with loudness, so a loud voice from across the room still
// reads as "far". Shared by the live gate and the voice-separation analysis.
struct TiltDetector {
    Biquad lowHp, lowLp, hiHp, hiLp;
    float lowP = 0, hiP = 0, coef = 0;

    void configure(float fs) {
        lowHp.set(Biquad::HP, fs, 100, 0.7071);
        lowLp.set(Biquad::LP, fs, 300, 0.7071);
        hiHp.set(Biquad::HP, fs, 1000, 0.7071);
        hiLp.set(Biquad::LP, fs, 4000, 0.7071);
        coef = onePole(25.0f, fs);
    }
    void reset() {
        lowHp.reset(); lowLp.reset(); hiHp.reset(); hiLp.reset();
        lowP = hiP = 0;
    }
    inline void process(float x) {
        const float l = lowLp.process(lowHp.process(x));
        const float h = hiLp.process(hiHp.process(x));
        lowP = coef * lowP + (1 - coef) * l * l;
        hiP = coef * hiP + (1 - coef) * h * h;
    }
    // True if bass exceeds treble by more than the given power ratio.
    inline bool above(float ratioPow) const { return lowP > hiP * ratioPow + 1e-14f; }
    inline bool below(float ratioPow) const { return lowP < hiP * ratioPow; }
    float tiltDb() const { return 10.0f * std::log10((lowP + 1e-14f) / (hiP + 1e-14f)); }
};

class VoiceChain {
public:
    explicit VoiceChain(float sampleRate = 48000) : fs(sampleRate), rev(sampleRate) {
        denoise = mf::rnnoise::create(); // nullptr if rnnoise isn't installed: the stage passes audio through
        vadState = mf::rnnoise::create(); // voice detector for the gate when Noise Removal isn't running
    }
    ~VoiceChain() {
        mf::rnnoise::destroy(denoise);
        mf::rnnoise::destroy(vadState);
    }
    VoiceChain(const VoiceChain &) = delete;
    VoiceChain &operator=(const VoiceChain &) = delete;

    Recorder *recorder = nullptr; // optional tap at the gate input
    ScopeTap *scope = nullptr;    // optional display tap at the gate input (spectrum view)
    // Optional block-processing hook for third-party plugins (kept as a plain callback so this
    // header stays free of plugin/Qt dependencies).
    void (*blockHook)(void *ctx, float *io, int n) = nullptr;
    void *blockCtx = nullptr;

    void reset() {
        for (auto &b : lowCut) b.reset();
        for (auto &b : listenCut) b.reset();
        for (auto &b : tone) b.reset();
        for (auto &b : eq) b.reset();
        deHp.reset();
        resetGate();
        compGrDb = 0; limGain = 1; limPos = 0;
        limDelay.fill(0); limAbs.fill(0); tpDet.reset();
        deEnv = 0; deRed = 0;
        nIn.fill(0); nOut.fill(0); nDry.fill(0); nPos = 0;
        al.reset(); warm.reset(); vc.reset(); dbl.reset(); echo.reset(); rev.reset();
        exHp.reset(); exHp2.reset(); vcLo.reset(); vcHi.reset(); vcBpHp.reset(); vcBpLp.reset();
    }

    // in: interleaved frames with inChannels channels. out: mono.
    void process(const float *in, int inChannels, float *out, int n, const Params &p, Meters &m) {
        while (n > kMaxBlock) { // the bypass crossfade keeps one block of raw input
            processBlock(in, inChannels, out, kMaxBlock, p, m);
            in += size_t(kMaxBlock) * size_t(inChannels);
            out += kMaxBlock;
            n -= kMaxBlock;
        }
        processBlock(in, inChannels, out, n, p, m);
    }

private:
    void resetGate() {
        det.reset();
        tilt.reset();
        gateIsOpen = false; gateHoldLeft = 0; gateGainDb = 0;
        gDelay.fill(0); gPos = 0;
        vIn.fill(0); vOut.fill(0); vPos = 0; vadHold = 0;
        frameMax = 0; frameCount = 0; swSum = swSq = 0; swFrames = 0; swHead = swCount = 0; floorValid = false;
        applyFloor();
    }

    // ---- Adaptive noise floor. Every 20 ms the detector's peak level is noted; every 0.5 s the spread of
    // those peaks is checked. Steady sound (spread < 2.5 dB) may be background: fans, hum, hiss. The floor
    // is the quietest steady half-second of the last 4 s, and it only RISES when that steadiness is
    // confirmed: either in at least two separate gaps (background that persists between phrases), or for
    // 3 s without a break. Speech is never steady; a sustained vowel is one stretch, and in a quiet room the
    // pauses around it keep the floor low. The floor FALLS at once when a quieter steady half-second appears.
    // The threshold in use is raised only when that background would otherwise reach the gate's close level.
    inline void trackFloor(float env) {
        frameMax = std::max(frameMax, env);
        if (++frameCount < kFloorFrame) return;
        const float f = linToDb(frameMax);
        frameMax = 0; frameCount = 0;
        swSum += f; swSq += f * f;
        if (++swFrames < kFloorFrames) return;
        const float mu = swSum / swFrames, sigma = std::sqrt(std::max(0.0f, swSq / swFrames - mu * mu));
        swSum = swSq = 0; swFrames = 0;

        const bool steady = sigma < 2.5f && mu > -110.0f;
        swHist[size_t(swHead)] = steady ? mu + 2.0f * sigma : kNotSteady;
        swHead = (swHead + 1) % kFloorHistory;
        if (swCount < kFloorHistory) ++swCount;

        float lowest = kNotSteady;
        int runs = 0, run = 0, longest = 0;
        for (int k = 0; k < swCount; ++k) { // oldest to newest
            const float c = swHist[size_t((swHead - swCount + k + kFloorHistory) % kFloorHistory)];
            if (c != kNotSteady) {
                lowest = std::min(lowest, c);
                if (run++ == 0) ++runs;
                longest = std::max(longest, run);
            } else {
                run = 0;
            }
        }
        if (lowest != kNotSteady) {
            if (floorValid && lowest < floorDb) floorDb = lowest;                   // quieter: follow at once
            else if ((runs >= 2 || longest >= kFloorRiseRun) && (!floorValid || lowest > floorDb)) {
                floorDb = lowest;
                floorValid = true;
            }
        }
        applyFloor();
    }
    void applyFloor() {
        float closeDb = gateThrDb - gateHystDb, openDb = gateThrDb;
        // Noise peaks occasionally exceed the measured floor: close 3 dB above it, open 5 dB above it.
        if (adaptive && floorValid && floorDb + 3.0f > closeDb) {
            closeDb = floorDb + 3.0f;
            openDb = std::max(gateThrDb, closeDb + 2.0f);
        }
        effOpenLin = dbToLin(openDb);
        effCloseLin = dbToLin(closeDb);
    }

    // A stage that is about to fade in starts from a clean state (no stale filter memory or tails).
    void resetStartingStages() {
        if (fLowCut.starting()) { for (auto &b : lowCut) b.reset(); plosive.reset(); }
        if (fHum.starting()) hum.reset();
        if (fGate.starting()) resetGate();
        if (fAl.starting()) al.reset();
        if (fBreath.starting()) breath.reset();
        if (fComp.starting()) compGrDb = 0;
        if (fDeess.starting()) { deHp.reset(); deShelf.reset(); deEnv = deRed = 0; }
        if (fVc.starting()) { vc.reset(); vcLo.reset(); vcHi.reset(); vcBpHp.reset(); vcBpLp.reset(); }
        if (fEq.starting()) for (auto &b : eq) b.reset();
        if (fTone.starting()) for (auto &b : tone) b.reset();
        if (fWarm.starting()) warm.reset();
        if (fEx.starting()) { exHp.reset(); exHp2.reset(); }
        if (fDbl.starting()) dbl.reset();
        if (fEcho.starting()) echo.reset();
        if (fRev.starting()) rev.reset();
        if (fLim.starting()) { limGain = 1; limDelay.fill(0); limAbs.fill(0); tpDet.reset(); }
        if (fNoise.starting()) { nIn.fill(0); nOut.fill(0); nDry.fill(0); nPos = 0; }
    }

    void processBlock(const float *in, int inChannels, float *out, int n, const Params &p, Meters &m) {
#if defined(__x86_64__) || defined(__i386__)
        // Flush denormals to zero for this thread (cheap, avoids CPU spikes on silence).
        _mm_setcsr(_mm_getcsr() | 0x8040);
#endif
        uint32_t ver = p.version.load(std::memory_order_acquire);
        if (ver != seenVersion) {
            seenVersion = ver;
            update(p);
        }

        float inPk = 0, outPk = 0, gateLvl = 0, compLvlDb = -120, gateRed = 0, compRed = 0, limRed = 0,
              deRedMax = 0, vadMax = 0;
        bool anyOpen = false;

        // Fully bypassed (and not fading): raw mic with input gain only.
        const bool fullBypass = fBypass.on && fBypass.mix >= 1.0f;
        if (!fullBypass) resetStartingStages();

        for (int i = 0; i < n; ++i) {
            // 1. Input: pick channel, apply gain.
            float x;
            const float *f = in + i * inChannels;
            if (inChannels < 2 || channel == 0) x = f[0];
            else if (channel == 1) x = f[1];
            else x = 0.5f * (f[0] + f[1]);
            x *= inGainS.next();
            inPk = std::max(inPk, std::fabs(x));
            raw[i] = x;

            if (fullBypass) {
                out[i] = x;
                outPk = std::max(outPk, std::fabs(x));
                continue;
            }

            // What the Voice Gate (and Setup, and the spectrum view) listens to: the microphone after
            // Low Cut, but before Noise Removal. Noise Removal stays nearly silent for the first moments of
            // speech and also removes energy in the low band the gate often listens to, which made the gate
            // open 20-40 ms late and clip the start of phrases. Noise Removal delays the audio by 10 ms, so
            // listening before it also gives the gate 10 ms of look-ahead for free.
            float listen = x;
            if (fLowCut.on) {
                listen = listenCut[0].process(listen);
                if (lowCutSteep) listen = listenCut[1].process(listen);
            }

            // 2. AI noise removal (RNNoise works on 480-sample frames: 10 ms delay).
            if (fNoise.running() && denoise) {
                const float wet = nOut[nPos] * (1.0f / 32768.0f);
                const float dry = nDry[nPos];
                nIn[nPos] = x * 32768.0f;
                nDry[nPos] = x;
                const float y = dry + (wet - dry) * noiseMixS.next();
                if (++nPos == kNoiseFrame) {
                    nPos = 0;
                    lastVad = mf::rnnoise::processFrame(denoise, nOut.data(), nIn.data());
                }
                vadMax = std::max(vadMax, lastVad);
                x = fNoise.apply(x, y);
            }

            // 2b. Hum removal (mains hum and its harmonics).
            if (fHum.running()) x = fHum.apply(x, hum.process(x));

            // 3. Low cut (removes rumble, desk thumps), plus the plosive guard ("p"/"b" pops).
            if (fLowCut.running()) {
                float y = lowCut[0].process(x);
                if (lowCutSteep) y = lowCut[1].process(y);
                if (plosiveOn) y = plosive.process(y);
                x = fLowCut.apply(x, y);
            }

            // 4. Gate / expander. The detector listens to a filtered band; the audio
            //    itself is delayed slightly (look-ahead) so word starts aren't clipped.
            if (recorder) recorder->push(listen);
            if (scope && scope->on.load(std::memory_order_relaxed)) scope->push(listen);
            if (fGate.running()) {
                const float env = det.process(listen);
                gateLvl = std::max(gateLvl, env);
                tilt.process(listen);
                if (env > gateCloseLin) { tiltLowSum += tilt.lowP; tiltHiSum += tilt.hiP; }

                gDelay[gPos] = x;
                const float delayed = gDelay[(gPos - gLookahead) & kGateMask];
                gPos = (gPos + 1) & kGateMask;

                // Opening needs: loud enough AND (if enabled) close enough. Closeness only decides
                // OPENING: every syllable has a vowel that passes it, and level + hold then carry the
                // brighter consonants (s, t, k) through, so your words aren't chopped.
                trackFloor(env);
                // AI voice detector: use Noise Removal's result if it's running, otherwise run our own.
                bool voiceOk = true;
                if (useVad) {
                    if (!(fNoise.running() && denoise)) {
                        vIn[vPos] = x * 32768.0f;
                        if (++vPos == kNoiseFrame) { vPos = 0; lastVad = mf::rnnoise::processFrame(vadState, vOut.data(), vIn.data()); }
                    }
                    // Hold a "voice" verdict for 200 ms so the consonants and gaps inside a phrase pass.
                    if (lastVad >= vadThr) vadHold = vadHoldSamples;
                    else if (vadHold > 0) --vadHold;
                    voiceOk = vadHold > 0;
                }
                const bool closeOpen = !closeCheck || (tiltBrighter ? tilt.below(tiltOpenPow) : tilt.above(tiltOpenPow));
                float target;
                if (env > effOpenLin && closeOpen && voiceOk) {
                    gateIsOpen = true;
                    gateHoldLeft = gateHoldSamples;
                } else if (env < effCloseLin) {
                    if (gateHoldLeft > 0) --gateHoldLeft;
                    else gateIsOpen = false;
                } else if (gateIsOpen && voiceOk) {
                    // Above the close level: keep holding. With the voice detector on, only while it still
                    // hears a voice (its 200 ms memory carries consonants and short gaps).
                    gateHoldLeft = gateHoldSamples;
                } else if (gateIsOpen) {
                    if (gateHoldLeft > 0) --gateHoldLeft;
                    else gateIsOpen = false;
                }
                const int reason = gateIsOpen ? 5 : env > effOpenLin && !closeOpen ? 4 : env > effOpenLin && !voiceOk ? 3
                                 : env > gateOpenLin ? 2 : 1;
                gateReasonMax = std::max(gateReasonMax, reason);
                if (gateIsOpen) target = 0.0f;
                else if (expander) target = std::max(gateRangeDb, (linToDb(env) - gateThrDb) * (gateRatio - 1.0f));
                else target = gateRangeDb;
                anyOpen |= gateIsOpen;

                const float c = target > gateGainDb ? gAtk : gRel;
                gateGainDb = target + (gateGainDb - target) * c;
                gateRed = std::max(gateRed, -gateGainDb);
                x = fGate.apply(x, delayed * dbToLin(gateGainDb));
            }

            // 4a. Breath reduction (opt-in).
            if (fBreath.running()) x = fBreath.apply(x, breath.process(x));

            // 4b. Auto level: steer your speaking level to a target (frozen in pauses).
            if (fAl.running()) x = fAl.apply(x, al.process(x, fGate.on ? gateIsOpen : true));

            // 5. Compressor (feed-forward, soft knee, log domain).
            if (fComp.running()) {
                const float xl = linToDb(std::fabs(x));
                compLvlDb = std::max(compLvlDb, xl);
                float yg;
                const float over = xl - cThr;
                if (2 * over < -cKnee) yg = xl;
                else if (cKnee > 0 && 2 * std::fabs(over) <= cKnee) {
                    const float t = over + cKnee / 2;
                    yg = xl + (1.0f / cRatio - 1.0f) * t * t / (2 * cKnee);
                } else yg = cThr + over / cRatio;
                const float gr = xl - yg;
                const float c = gr > compGrDb ? cAtk : cRel;
                compGrDb = gr + (compGrDb - gr) * c;
                compRed = std::max(compRed, compGrDb);
                x = fComp.apply(x, x * dbToLin(cMakeupS.next() - compGrDb));
            }

            // 6. De-esser: a high shelf that dips only while the "S" band is too loud.
            if (fDeess.running()) {
                const float a = std::fabs(deHp.process(x));
                deEnv = a > deEnv ? deAtk * deEnv + (1 - deAtk) * a : deRel * deEnv + (1 - deRel) * a;
                const float over = linToDb(deEnv) - deThr;
                const float want = over > 0 ? std::min(deMax, over * 0.75f) : 0.0f; // 4:1
                deRed = want > deRed ? want + (deRed - want) * deAtk : want + (deRed - want) * deRelG;
                deRedMax = std::max(deRedMax, deRed);
                if ((++deCount & 31) == 0 && std::fabs(deRed - deShelfDb) > 0.05f) {
                    deShelfDb = deRed;
                    deShelf.set(Biquad::HighShelf, fs, deFreq * 0.7f, 0.7071, -deRed);
                }
                x = fDeess.apply(x, deShelf.process(x));
            }

            // 6b. Voice changer (fun).
            if (fVc.running()) x = fVc.apply(x, vc.process(x, vcLo, vcHi, vcBpHp, vcBpLp));

            // 7. Equaliser.
            if (fEq.running()) {
                float y = x;
                for (int b = 0; b < 5; ++b)
                    if (eq[b].audible()) y = eq[b].process(y);
                x = fEq.apply(x, y);
            }

            // 8. Tone.
            if (fTone.running()) {
                float y = x;
                for (int b = 0; b < 3; ++b)
                    if (tone[b].audible()) y = tone[b].process(y);
                x = fTone.apply(x, y);
            }

            out[i] = x;
        }

        // 8a. Your plugins (VST3 / LV2). They work on whole blocks, so they run between two sample loops.
        if (!fullBypass && pluginsOn && blockHook) blockHook(blockCtx, out, n);

        for (int i = 0; i < n && !fullBypass; ++i) {
            float x = out[i];

            // 8b. Character and space.
            if (fWarm.running()) x = fWarm.apply(x, warm.process(x));
            if (fEx.running()) {
                // Full-wave rectifying the treble creates new harmonics an octave up: "sparkle".
                const float hp = exHp.process(x);
                x = fEx.apply(x, x + exAmtS.next() * (0.6f * exHp2.process(std::fabs(hp)) + 0.35f * hp));
            }
            if (fDbl.running()) x = fDbl.apply(x, dbl.process(x));
            if (fEcho.running()) x = fEcho.apply(x, echo.process(x));
            if (fRev.running()) x = fRev.apply(x, rev.process(x));

            // 9. Output gain then limiter (never lets the signal clip).
            x *= outGainS.next();
            if (fLim.running()) {
                limDelay[limPos] = x;
                // True peak: the detector's estimate lags by kHalf samples, so it lands kHalf slots
                // later in the window; the window (and delay) are long enough to cover that.
                limAbs[limPos] = std::max(std::fabs(x), tpDet.process(x));
                limPos = (limPos + 1) & kLimMask;
                float pk = 0;
                for (int k = 0; k < limLookahead; ++k)
                    pk = std::max(pk, limAbs[(limPos - 1 - k) & kLimMask]);
                const float target = pk > limCeil ? limCeil / pk : 1.0f;
                limGain = target < limGain ? target + (limGain - target) * lAtk
                                           : target + (limGain - target) * lRel;
                float y = limDelay[(limPos - limLookahead) & kLimMask] * limGain;
                y = std::clamp(y, -limCeil, limCeil);
                limRed = std::max(limRed, -linToDb(limGain));
                x = fLim.apply(x, y);
            }

            // Compare (bypass) crossfades between processed and raw instead of switching abruptly.
            x = fBypass.apply(x, raw[i]);

            out[i] = x;
            outPk = std::max(outPk, std::fabs(x));
        }

        // Mute: 10 ms linear fade to exact silence, then hold it. Applies in bypass too, so a muted mic
        // never sends audio. Un-muting fades back in the same way (no clicks either way).
        if (muted || muteGain < 1.0f) {
            outPk = 0;
            for (int i = 0; i < n; ++i) {
                muteGain = muted ? std::max(0.0f, muteGain - muteStep) : std::min(1.0f, muteGain + muteStep);
                out[i] *= muteGain;
                outPk = std::max(outPk, std::fabs(out[i]));
            }
        }

        Meters::bump(m.inPeak, linToDb(inPk));
        Meters::bump(m.outPeak, linToDb(outPk));
        if (fullBypass) return;
        if (fNoise.on) Meters::bump(m.noiseVad, vadMax);
        if (fGate.on) {
            // Closeness of whatever is making sound right now (only while something is audible).
            if (tiltLowSum > 0 || tiltHiSum > 0) {
                m.gateTilt.store(10.0f * std::log10((tiltLowSum + 1e-14f) / (tiltHiSum + 1e-14f)),
                                 std::memory_order_relaxed);
                tiltLowSum = tiltHiSum = 0;
            }
            Meters::bump(m.gateLevel, linToDb(gateLvl));
            Meters::bump(m.gateReduction, gateRed);
            Meters::bump(m.gateOpen, anyOpen ? 1.0f : 0.0f);
            if (gateReasonMax > m.gateReason.load(std::memory_order_relaxed)) m.gateReason.store(gateReasonMax, std::memory_order_relaxed);
            gateReasonMax = 0;
            m.gateFloor.store(floorValid ? floorDb : -120.0f, std::memory_order_relaxed);
            m.gateOpenAt.store(linToDb(effOpenLin), std::memory_order_relaxed);
        }
        if (fComp.on) {
            Meters::bump(m.compLevel, compLvlDb);
            Meters::bump(m.compReduction, compRed);
        }
        if (fDeess.on) Meters::bump(m.deessReduction, deRedMax);
        if (fLim.on) Meters::bump(m.limReduction, limRed);
        m.autoLevelGain.store(fAl.on ? al.gainDb : 0.0f, std::memory_order_relaxed);
        m.humBase.store(fHum.on ? hum.base : 0.0f, std::memory_order_relaxed);
    }

    void update(const Params &p) {
        const bool jump = !configured; // the first configuration applies at once; later changes glide
        configured = true;
        channel = int(p.get(InputChannel) + 0.5f);
        inGainS.configure(fs); inGainS.set(dbToLin(p.get(InputGain)), jump);
        fBypass.set(p.get(Bypass) > 0.5f, jump, fs);
        muted = p.get(Mute) > 0.5f;
        muteStep = 1.0f / (0.010f * fs);

        auto active = [&](int added, int on) { return p.get(added) > 0.5f && p.get(on) > 0.5f; };
        fNoise.set(active(NoiseAdded, NoiseOn), jump, fs);
        noiseMixS.configure(fs); noiseMixS.set(p.get(NoiseAmount) / 100.0f, jump);

        fHum.set(active(HumAdded, HumOn), jump, fs);
        hum.configure(fs, int(p.get(HumFreq) + 0.5f), int(p.get(HumHarmonics) + 0.5f), jump);
        plosiveOn = p.get(LowCutPlosive) > 0.5f;
        if (jump) plosive.configure(fs);
        fLowCut.set(active(LowCutAdded, LowCutOn), jump, fs);
        lowCutSteep = p.get(LowCutSteep) > 0.5f;
        if (lowCutSteep) { // 4th-order Butterworth = two stages
            lowCut[0].set(Biquad::HP, fs, p.get(LowCutFreq), 0.5412, 0, jump);
            lowCut[1].set(Biquad::HP, fs, p.get(LowCutFreq), 1.3066, 0, jump);
            listenCut[0].set(Biquad::HP, fs, p.get(LowCutFreq), 0.5412, 0, jump);
            listenCut[1].set(Biquad::HP, fs, p.get(LowCutFreq), 1.3066, 0, jump);
        } else {
            lowCut[0].set(Biquad::HP, fs, p.get(LowCutFreq), 0.7071, 0, jump);
            listenCut[0].set(Biquad::HP, fs, p.get(LowCutFreq), 0.7071, 0, jump);
        }

        fGate.set(active(GateAdded, GateOn), jump, fs);
        expander = p.get(GateMode) > 0.5f;
        det.configure(fs, p.get(GateFocus) > 0.5f, p.get(GateScLow), p.get(GateScHigh));
        tilt.configure(fs);
        closeCheck = p.get(GateCloseCheck) > 0.5f;
        tiltBrighter = p.get(GateTiltDir) > 0.5f;
        tiltOpenPow = std::pow(10.0f, p.get(GateTilt) / 10.0f);
        gateThrDb = p.get(GateThreshold);
        gateOpenLin = dbToLin(gateThrDb);
        gateHystDb = p.get(GateHysteresis);
        gateCloseLin = dbToLin(gateThrDb - gateHystDb);
        adaptive = p.get(GateAdaptive) > 0.5f;
        applyFloor();
        gateRangeDb = p.get(GateRange);
        gateRatio = p.get(GateRatio);
        gateHoldSamples = int(p.get(GateHold) * 0.001f * fs);
        useVad = p.get(GateVad) > 0.5f && vadState;
        vadThr = p.get(GateVadThreshold) / 100.0f;
        vadHoldSamples = int(0.2f * fs);
        // The voice detector needs ~20 ms to decide, so the audio waits that long (word starts aren't clipped).
        gLookahead = std::clamp(int(std::max(p.get(GateLookahead), useVad ? 20.0f : 0.0f) * 0.001f * fs), 0, kGateSize - 1);
        gAtk = onePole(p.get(GateAttack) / 2.2f, fs);
        gRel = onePole(p.get(GateRelease) / 2.2f, fs);

        fComp.set(active(CompAdded, CompOn), jump, fs);
        cThr = p.get(CompThreshold);
        cRatio = std::max(1.0f, p.get(CompRatio));
        cKnee = p.get(CompKnee);
        cMakeupS.configure(fs); cMakeupS.set(p.get(CompMakeup), jump);
        cAtk = onePole(p.get(CompAttack), fs);
        cRel = onePole(p.get(CompRelease), fs);

        fDeess.set(p.get(DeessAdded) > 0.5f && p.get(DeessOn) > 0.5f, jump, fs);
        deFreq = p.get(DeessFreq);
        deHp.set(Biquad::HP, fs, deFreq, 0.7071);
        deShelf.set(Biquad::HighShelf, fs, deFreq * 0.7f, 0.7071, -deRed);
        deShelfDb = deRed;
        deThr = p.get(DeessThreshold);
        deMax = p.get(DeessMax);
        deAtk = onePole(0.5f, fs);
        deRel = onePole(40.0f, fs);
        deRelG = onePole(60.0f, fs);

        fEq.set(p.get(EqAdded) > 0.5f && p.get(EqOn) > 0.5f, jump, fs);
        for (int b = 0; b < 5; ++b) {
            const float fr = p.get(Eq1Freq + 3 * b), g = p.get(Eq1Gain + 3 * b), q = p.get(Eq1Q + 3 * b);
            const Biquad::Type t = b == 0 ? Biquad::LowShelf : b == 4 ? Biquad::HighShelf : Biquad::Peak;
            eq[b].set(t, fs, fr, q, g, jump);
        }

        fTone.set(active(ToneAdded, ToneOn), jump, fs);
        pluginsOn = active(PluginsAdded, PluginsOn);

        fBreath.set(p.get(BreathOn) > 0.5f, jump, fs);
        breath.configure(fs, p.get(BreathAmount));
        fAl.set(active(AutoLevelAdded, AutoLevelOn), jump, fs);
        al.configure(fs, p.get(AutoLevelTarget), p.get(AutoLevelMaxGain), int(p.get(AutoLevelSpeed) + 0.5f));

        fVc.set(active(VcAdded, VcOn), jump, fs);
        vc.configure(fs, p.get(VcPitch), p.get(VcCharacter), p.get(VcRobot), p.get(VcRobotFreq), p.get(VcRadio),
                     p.get(VcDrive), p.get(VcWobble), p.get(VcWobbleRate), p.get(VcMix));
        vcLo.set(Biquad::LowShelf, fs, 700, 0.7071, -p.get(VcCharacter) * 0.5f);
        vcHi.set(Biquad::HighShelf, fs, 1800, 0.7071, p.get(VcCharacter) * 0.5f);
        vcBpHp.set(Biquad::HP, fs, 450, 0.9);
        vcBpLp.set(Biquad::LP, fs, 2800, 0.9);

        fWarm.set(active(WarmthAdded, WarmthOn), jump, fs);
        warm.configure(fs, p.get(WarmthAmount));
        fEx.set(active(ExciterAdded, ExciterOn), jump, fs);
        exAmtS.configure(fs); exAmtS.set(p.get(ExciterAmount) / 100.0f, jump);
        exHp.set(Biquad::HP, fs, p.get(ExciterFreq), 0.7071, 0, jump);
        exHp2.set(Biquad::HP, fs, p.get(ExciterFreq), 0.7071, 0, jump);
        fDbl.set(active(DoublerAdded, DoublerOn), jump, fs);
        dbl.configure(fs, p.get(DoublerWidth), p.get(DoublerMix));
        fEcho.set(active(EchoAdded, EchoOn), jump, fs);
        echo.configure(fs, p.get(EchoTime), p.get(EchoFeedback), p.get(EchoMix), p.get(EchoTone));
        fRev.set(active(ReverbAdded, ReverbOn), jump, fs);
        rev.configure(p.get(ReverbSize), p.get(ReverbDamp), p.get(ReverbMix));
        tone[0].set(Biquad::LowShelf, fs, 120, 0.7071, p.get(ToneBass), jump);
        tone[1].set(Biquad::Peak, fs, 4500, 0.9, p.get(TonePresence), jump);
        tone[2].set(Biquad::HighShelf, fs, 10000, 0.7071, p.get(ToneAir), jump);

        outGainS.configure(fs); outGainS.set(dbToLin(p.get(OutputGain)), jump);
        fLim.set(p.get(LimiterOn) > 0.5f, jump, fs);
        limCeil = dbToLin(p.get(LimiterCeiling));
        limLookahead = std::clamp(int(0.0015f * fs), 1, kLimSize - 1);
        lAtk = onePole(0.3f, fs);
        lRel = onePole(80.0f, fs);
    }

    float fs;
    uint32_t seenVersion = 0;
    bool configured = false;

    static constexpr int kMaxBlock = 1024;
    std::array<float, kMaxBlock> raw{}; // input after channel + gain (for the bypass crossfade)
    StageFade fBypass, fNoise, fHum, fLowCut, fGate, fBreath, fAl, fComp, fDeess, fVc, fEq, fTone, fWarm, fEx, fDbl, fEcho, fRev, fLim;

    int channel = 0;
    Smoothed inGainS;

    // AI noise removal
    static constexpr int kNoiseFrame = 480;
    DenoiseState *denoise = nullptr;
    Smoothed noiseMixS;
    float lastVad = 0;
    std::array<float, kNoiseFrame> nIn{}, nOut{}, nDry{};
    int nPos = 0;

    HumRemover hum;
    PlosiveGuard plosive;
    bool plosiveOn = true;
    bool lowCutSteep = true;
    SmoothBiquad lowCut[2];
    SmoothBiquad listenCut[2]; // the same low cut on what the gate hears (before noise removal)

    bool expander = false;
    GateDetector det;
    TiltDetector tilt;
    bool closeCheck = false, tiltBrighter = false;
    float tiltOpenPow = 4, tiltLowSum = 0, tiltHiSum = 0;
    float gateThrDb = -20, gateOpenLin = 0.1f, gateCloseLin = 0.06f, gateRangeDb = -30, gateRatio = 3;
    int gateHoldSamples = 0, gateHoldLeft = 0;
    float gAtk = 0, gRel = 0, gateGainDb = 0;
    bool gateIsOpen = false;
    int gateReasonMax = 0;
    // AI voice detector for the gate (optional).
    DenoiseState *vadState = nullptr;
    bool useVad = false;
    float vadThr = 0.5f;
    int vadHold = 0, vadHoldSamples = 9600, vPos = 0;
    std::array<float, 480> vIn{}, vOut{};
    // Adaptive noise floor (see trackFloor).
    static constexpr int kFloorFrame = 960, kFloorFrames = 25, kFloorRiseRun = 6, kFloorHistory = 8; // 20 ms, 0.5 s, 3 s, 4 s
    static constexpr float kNotSteady = 1e9f;
    bool adaptive = true, floorValid = false;
    float gateHystDb = 4, effOpenLin = 0.1f, effCloseLin = 0.06f;
    float frameMax = 0, swSum = 0, swSq = 0, floorDb = -120;
    std::array<float, kFloorHistory> swHist{};
    int frameCount = 0, swFrames = 0, swHead = 0, swCount = 0;
    static constexpr int kGateSize = 1024, kGateMask = kGateSize - 1;
    std::array<float, kGateSize> gDelay{};
    int gPos = 0, gLookahead = 0;

    float cThr = -24, cRatio = 3, cKnee = 6, cAtk = 0, cRel = 0;
    Smoothed cMakeupS;
    float compGrDb = 0;

    Biquad deHp, deShelf;
    float deFreq = 6000, deShelfDb = 0;
    uint32_t deCount = 0;
    float deThr = -30, deMax = 8, deAtk = 0, deRel = 0, deRelG = 0, deEnv = 0, deRed = 0;

    SmoothBiquad eq[5];
    SmoothBiquad tone[3];

    bool pluginsOn = false;
    bool muted = false;
    float muteGain = 1.0f, muteStep = 1.0f / 480.0f;
    AutoLevel al;
    BreathReducer breath;
    VoiceChanger vc;
    Biquad vcLo, vcHi, vcBpHp, vcBpLp;
    Warmth warm;
    SmoothBiquad exHp, exHp2;
    Smoothed exAmtS;
    Doubler dbl;
    Echo echo;
    Reverb rev;

    Smoothed outGainS;
    float limCeil = 0.89f, limGain = 1, lAtk = 0, lRel = 0;
    static constexpr int kLimSize = 256, kLimMask = kLimSize - 1;
    std::array<float, kLimSize> limDelay{}, limAbs{};
    int limPos = 0, limLookahead = 72;
    TruePeakDetector tpDet;
};

} // namespace mf
