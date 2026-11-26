// Extra voice effects: auto level, warmth, exciter, voice changer, doubler, echo, reverb.
// Realtime-safe: buffers are allocated in constructors, never in process().
// Included by dsp.h after dbToLin / onePole / Biquad are defined; not meant to be included on its own.
#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace mf {

// Fractional delay line (power-of-two length, linear interpolation).
class DelayLine {
public:
    explicit DelayLine(int lengthPow2 = 8192) : b(size_t(lengthPow2), 0.0f), mask(lengthPow2 - 1) {}
    inline void push(float x) { b[size_t(w)] = x; w = (w + 1) & mask; }
    // Sample from `d` samples ago (1 = the sample just pushed).
    inline float tap(float d) const {
        float rp = float(w) - d;
        while (rp < 0) rp += float(mask + 1);
        const int i = int(rp);
        const float f = rp - float(i);
        const float a = b[size_t(i & mask)], c = b[size_t((i + 1) & mask)];
        return a + (c - a) * f;
    }
    void clear() { std::fill(b.begin(), b.end(), 0.0f); }
    int length() const { return mask + 1; }

private:
    std::vector<float> b;
    int mask, w = 0;
};

constexpr float kTwoPi = 6.283185307179586f;

// ------------------------------------------------------------------ Auto level
// Slowly steers your speaking level towards a target, only while you're talking
// (it freezes in pauses so it never pumps up background noise).
struct AutoLevel {
    float pw = 0, gainDb = 0, cDet = 0, cGain = 0, target = -18, maxGain = 12;
    void configure(float fs, float targetDb, float maxDb, int speed) {
        target = targetDb;
        maxGain = maxDb;
        cDet = onePole(300.0f, fs);
        const float tc[] = {4000.0f, 1500.0f, 500.0f};
        cGain = onePole(tc[std::clamp(speed, 0, 2)], fs);
    }
    void reset() { pw = 0; gainDb = 0; }
    inline float process(float x, bool speaking) {
        pw = cDet * pw + (1 - cDet) * x * x;
        if (speaking && pw > 1e-10f) {
            const float want = std::clamp(target - 10.0f * std::log10(pw), -maxGain, maxGain);
            gainDb = want + (gainDb - want) * cGain;
        }
        return x * dbToLin(gainDb);
    }
};

// Glide `v` towards `t` (per-sample, ~10 ms at 48 kHz); snaps when close.
inline void glide(float &v, float t, float k = 0.0021f) {
    if (v != t) { v += k * (t - v); if (std::fabs(t - v) < 1e-5f) v = t; }
}

// ------------------------------------------------------------------ Warmth
// Gentle analogue-style saturation: rounds off peaks and adds a little even-harmonic richness.
struct Warmth {
    float drive = 1, driveT = 1, bias = 0.05f, offset = 0, comp = 1, dc = 0, dcC = 0;
    bool ready = false;
    void configure(float fs, float amount) {
        driveT = 1.0f + amount / 100.0f * 3.0f;
        if (!ready) { drive = driveT; ready = true; recalc(); }
        dcC = onePole(50.0f, fs);
    }
    void recalc() {
        offset = std::tanh(drive * bias);
        comp = 1.0f / (1.0f - offset * offset); // unity gain for quiet signals
    }
    void reset() { dc = 0; }
    inline float process(float x) {
        if (drive != driveT) { glide(drive, driveT); recalc(); }
        float y = (std::tanh(drive * (x + bias)) - offset) / drive * comp;
        dc = dcC * dc + (1 - dcC) * y; // remove the small DC the bias creates
        return y - dc;
    }
};

// ------------------------------------------------------------------ Voice changer
// Pitch shifter (two crossfading delay taps), formant-like character tilt, vibrato wobble,
// robot ring modulation, radio band-limit and drive.
struct VoiceChanger {
    DelayLine pitchBuf{8192}, wobbleBuf{4096};
    float fs = 48000, ratio = 1, window = 1440, phase = 0;
    bool pitchOn = false;
    float robot = 0, robotInc = 0, robotPh = 0;
    float radio = 0, drive = 0, driveNorm = 1;
    float wobble = 0, wobInc = 0, wobPh = 0;
    float mix = 1;
    float charDb = 0;

    void configure(float sampleRate, float semis, float character, float robotPct, float robotHz,
                   float radioPct, float drivePct, float wobblePct, float wobbleHz, float mixPct) {
        fs = sampleRate;
        ratio = std::pow(2.0f, semis / 12.0f);
        pitchOn = std::fabs(semis) > 0.01f;
        window = 0.030f * fs;
        charDb = character;
        robot = robotPct / 100.0f;
        robotInc = kTwoPi * robotHz / fs;
        radio = radioPct / 100.0f;
        drive = 1.0f + drivePct / 100.0f * 24.0f;
        driveNorm = 1.0f / std::sqrt(drive);
        wobble = wobblePct / 100.0f;
        wobInc = kTwoPi * wobbleHz / fs;
        mix = mixPct / 100.0f;
    }
    void reset() { pitchBuf.clear(); wobbleBuf.clear(); phase = robotPh = wobPh = 0; }

    // `shelfLo`, `shelfHi`, `bpHp`, `bpLp` are filters owned by the caller (Biquad is defined in dsp.h).
    template <typename F>
    inline float process(float x, F &shelfLo, F &shelfHi, F &bpHp, F &bpLp) {
        float y = x;
        if (pitchOn) {
            pitchBuf.push(y);
            phase += (1.0f - ratio) / window;
            phase -= std::floor(phase);
            const float p2 = phase + 0.5f - std::floor(phase + 0.5f);
            const float g1 = std::sin(3.14159265f * phase), g2 = std::sin(3.14159265f * p2);
            y = g1 * g1 * pitchBuf.tap(1.0f + phase * window) + g2 * g2 * pitchBuf.tap(1.0f + p2 * window);
        }
        if (std::fabs(charDb) > 0.05f) y = shelfHi.process(shelfLo.process(y));
        if (wobble > 0) {
            wobbleBuf.push(y);
            wobPh += wobInc;
            if (wobPh > kTwoPi) wobPh -= kTwoPi;
            y = wobbleBuf.tap(0.006f * fs + wobble * 0.004f * fs * std::sin(wobPh));
        }
        if (robot > 0) {
            robotPh += robotInc;
            if (robotPh > kTwoPi) robotPh -= kTwoPi;
            y = y * (1.0f - robot) + y * std::sin(robotPh) * robot * 1.4f;
        }
        if (radio > 0) {
            const float bp = bpLp.process(bpHp.process(y));
            y = y * (1.0f - radio) + bp * radio * 1.6f;
        }
        if (drive > 1.01f) y = std::tanh(drive * y) * driveNorm;
        return x * (1.0f - mix) + y * mix;
    }
};

// ------------------------------------------------------------------ Doubler
// Two slowly drifting copies of your voice, 12-25 ms behind: sounds thicker, like double-tracking.
struct Doubler {
    DelayLine buf{4096};
    float fs = 48000, width = 0.5f, widthT = 0.5f, mix = 0.4f, mixT = 0.4f, ph1 = 0, ph2 = 1.7f, inc1 = 0, inc2 = 0;
    bool ready = false;
    void configure(float sampleRate, float widthPct, float mixPct) {
        fs = sampleRate;
        widthT = widthPct / 100.0f;
        mixT = mixPct / 100.0f;
        if (!ready) { width = widthT; mix = mixT; ready = true; }
        inc1 = kTwoPi * 0.31f / fs;
        inc2 = kTwoPi * 0.47f / fs;
    }
    void reset() { buf.clear(); }
    inline float process(float x) {
        glide(mix, mixT);
        glide(width, widthT);
        buf.push(x);
        ph1 += inc1; if (ph1 > kTwoPi) ph1 -= kTwoPi;
        ph2 += inc2; if (ph2 > kTwoPi) ph2 -= kTwoPi;
        const float d1 = (0.012f + width * 0.0025f * std::sin(ph1)) * fs;
        const float d2 = (0.019f + width * 0.0035f * std::sin(ph2) + width * 0.006f) * fs;
        const float wet = 0.5f * (buf.tap(d1) + buf.tap(d2));
        return (x + wet * mix) / (1.0f + 0.5f * mix);
    }
};

// ------------------------------------------------------------------ Echo
struct Echo {
    DelayLine buf{131072}; // ~2.7 s at 48 kHz
    float delay = 15000, delayT = 15000, fb = 0.35f, fbT = 0.35f, mix = 0.25f, mixT = 0.25f, lp = 0, lpC = 0;
    bool ready = false;
    void configure(float fs, float ms, float fbPct, float mixPct, float toneHz) {
        delayT = std::clamp(ms * 0.001f * fs, 1.0f, float(buf.length() - 2));
        fbT = fbPct / 100.0f;
        mixT = mixPct / 100.0f;
        if (!ready) { delay = delayT; fb = fbT; mix = mixT; ready = true; }
        lpC = std::exp(-kTwoPi * toneHz / fs);
    }
    void reset() { buf.clear(); lp = 0; }
    inline float process(float x) {
        glide(mix, mixT);
        glide(fb, fbT);
        glide(delay, delayT, 0.0005f); // a moving delay time glides (like tape) instead of jumping
        float rep = buf.tap(delay);
        lp = lpC * lp + (1 - lpC) * rep; // repeats get darker, like a tape/analogue echo
        buf.push(x + lp * fb);
        return x + lp * mix;
    }
};

// ------------------------------------------------------------------ Reverb
// Compact Schroeder/Freeverb-style mono reverb: 6 damped combs into 3 allpasses.
struct Reverb {
    struct Comb {
        std::vector<float> b; size_t i = 0; float store = 0;
        inline float process(float x, float fb, float damp) {
            const float out = b[i];
            store = out * (1 - damp) + store * damp;
            b[i] = x + store * fb;
            if (++i == b.size()) i = 0;
            return out;
        }
    };
    struct Allpass {
        std::vector<float> b; size_t i = 0;
        inline float process(float x) {
            const float bo = b[i];
            const float out = -x + bo;
            b[i] = x + bo * 0.5f;
            if (++i == b.size()) i = 0;
            return out;
        }
    };
    std::array<Comb, 6> combs;
    std::array<Allpass, 3> aps;
    float fb = 0.84f, damp = 0.2f, mix = 0.2f, fbT = 0.84f, dampT = 0.2f, mixT = 0.2f;
    bool ready = false;

    explicit Reverb(float fs = 48000) {
        const int cl[6] = {1116, 1188, 1277, 1356, 1422, 1491};
        const int al[3] = {556, 441, 341};
        const float sc = fs / 44100.0f;
        for (int k = 0; k < 6; ++k) combs[size_t(k)].b.assign(size_t(cl[k] * sc), 0.0f);
        for (int k = 0; k < 3; ++k) aps[size_t(k)].b.assign(size_t(al[k] * sc), 0.0f);
    }
    void configure(float sizePct, float dampPct, float mixPct) {
        fbT = 0.70f + sizePct / 100.0f * 0.28f;
        dampT = dampPct / 100.0f * 0.5f;
        mixT = mixPct / 100.0f;
        if (!ready) { fb = fbT; damp = dampT; mix = mixT; ready = true; }
    }
    void reset() {
        for (auto &c : combs) { std::fill(c.b.begin(), c.b.end(), 0.0f); c.store = 0; }
        for (auto &a : aps) std::fill(a.b.begin(), a.b.end(), 0.0f);
    }
    inline float process(float x) {
        glide(mix, mixT); glide(fb, fbT); glide(damp, dampT);
        const float in = x * 0.015f;
        float s = 0;
        for (auto &c : combs) s += c.process(in, fb, damp);
        for (auto &a : aps) s = a.process(s);
        return x * (1.0f - 0.4f * mix) + s * 3.0f * mix;
    }
};

} // namespace mf
