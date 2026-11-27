// Extra voice effects: auto level, warmth, exciter, voice changer, doubler, echo, reverb.
// Realtime-safe: buffers are allocated in constructors, never in process().
// Included by dsp.h after dbToLin / onePole / Biquad are defined; not meant to be included on its own.
#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstdint>
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

// ------------------------------------------------------------------ Formant shifter
// Phase-vocoder pitch shifter that moves pitch and formants ("voice size") independently, so a voice can
// go higher without sounding like a chipmunk (child, woman) or lower and bigger (orc, dragon).
// Each frame: FFT, spectral envelope from the cepstrum, shift the flattened harmonics by the pitch ratio,
// re-apply the envelope stretched by the formant ratio, optional random phase (whisper), inverse FFT.
// Latency: frame - hop = 1792 samples at 48 kHz (37 ms). All buffers are allocated in the constructor.
struct FormantShifter {
    static constexpr int kOsamp = 8;
    int n = 2048, half = 1024, hop = 256, lat = 1792, rover = 1792, lifter = 86;
    float pitch = 1, formant = 1, whisper = 0, eIn = 0, eOut = 0, eC = 1.0f / 14400, gain = 1, gainT = 1;
    bool primed = false;
    uint32_t rng = 0x9e3779b9u;
    std::vector<float> inF, outF, acc, win, lastPh, sumPh, anaMag, anaFrq, env, synMag, synFrq;
    std::vector<std::complex<float>> buf, cep, tw;
    std::vector<uint32_t> rev;

    explicit FormantShifter(float fs = 48000) { setRate(fs); }
    void setRate(float fs) {
        n = fs >= 32000 ? 2048 : 1024;
        half = n / 2; hop = n / kOsamp; lat = n - hop;
        eC = 1.0f / (0.3f * fs);
        lifter = std::max(8, int(fs * 0.0018f)); // keeps detail coarser than the harmonics of voices up to ~550 Hz
        inF.assign(size_t(n), 0); outF.assign(size_t(n), 0); acc.assign(size_t(2 * n), 0);
        win.resize(size_t(n));
        for (int k = 0; k < n; ++k) win[size_t(k)] = 0.5f - 0.5f * std::cos(kTwoPi * float(k) / float(n));
        for (auto *v : {&lastPh, &sumPh, &anaMag, &anaFrq, &env, &synMag, &synFrq}) v->assign(size_t(half + 1), 0);
        buf.assign(size_t(n), {}); cep.assign(size_t(n), {});
        tw.resize(size_t(half));
        for (int k = 0; k < half; ++k) tw[size_t(k)] = std::polar(1.0f, -kTwoPi * float(k) / float(n));
        rev.resize(size_t(n));
        int bits = 0; while ((1 << bits) < n) ++bits;
        for (uint32_t i = 0; i < uint32_t(n); ++i) {
            uint32_t r = 0; for (int b = 0; b < bits; ++b) if (i & (1u << b)) r |= 1u << (bits - 1 - b);
            rev[i] = r;
        }
        reset();
    }
    void configure(float semis, float formantSemis, float whisperPct) {
        pitch = std::pow(2.0f, semis / 12.0f);
        formant = std::pow(2.0f, formantSemis / 12.0f);
        whisper = std::clamp(whisperPct / 100.0f, 0.0f, 1.0f);
    }
    void reset() {
        std::fill(inF.begin(), inF.end(), 0.0f);
        rover = lat;
        resetSynthesis();
    }
    // Forget the output side (used when the shifter pauses); the input history is kept.
    void resetSynthesis() {
        std::fill(outF.begin(), outF.end(), 0.0f); std::fill(acc.begin(), acc.end(), 0.0f);
        std::fill(lastPh.begin(), lastPh.end(), 0.0f); std::fill(sumPh.begin(), sumPh.end(), 0.0f);
        primed = false; eIn = eOut = 0; gain = gainT = 1;
    }
    int latency() const { return lat; }

    // `run` = false keeps the input history (so switching on later is smooth) but skips the FFT work.
    inline float process(float x, bool run) {
        inF[size_t(rover)] = x;
        float y = run && primed ? outF[size_t(rover - lat)] : 0.0f;
        if (run) {
            // Keep the loudness of the input: how much a shift loses or gains depends on the voice and the
            // settings, so follow both levels slowly (~0.3 s) and correct by up to ±9 dB, only while sound is present.
            const float xd = inF[size_t(rover - lat)]; // the input this output sample came from
            eIn += eC * (xd * xd - eIn);
            eOut += eC * (y * y - eOut);
            if (eIn > 1e-7f && eOut > 1e-9f) gainT = std::clamp(std::sqrt(eIn / eOut), 0.35f, 2.8f);
            gain += eC * (gainT - gain);
            y *= gain;
        }
        if (++rover >= n) {
            rover = lat;
            if (run) { frame(); primed = true; }
            else if (primed) resetSynthesis();
            for (int k = 0; k < lat; ++k) inF[size_t(k)] = inF[size_t(k + hop)];
        }
        return y;
    }

private:
    void fft(std::complex<float> *a, bool inverse) const {
        for (uint32_t i = 0; i < uint32_t(n); ++i) if (i < rev[i]) std::swap(a[i], a[rev[i]]);
        for (int len = 2; len <= n; len <<= 1) {
            const int step = n / len, h = len / 2;
            for (int i = 0; i < n; i += len)
                for (int k = 0; k < h; ++k) {
                    std::complex<float> w = tw[size_t(k * step)];
                    if (inverse) w = std::conj(w);
                    const auto u = a[i + k], v = a[i + k + h] * w;
                    a[i + k] = u + v; a[i + k + h] = u - v;
                }
        }
    }
    float envAt(float bin) const {
        if (bin >= float(half)) return env[size_t(half)];
        const int i = int(bin); const float f = bin - float(i);
        return env[size_t(i)] + (env[size_t(i + 1)] - env[size_t(i)]) * f;
    }
    float rand01() { rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5; return float(rng) * 2.3283064e-10f; }

    void frame() {
        constexpr float kPi = 3.14159265f;
        const float expct = kTwoPi * float(hop) / float(n);
        for (int k = 0; k < n; ++k) buf[size_t(k)] = {inF[size_t(k)] * win[size_t(k)], 0.0f};
        fft(buf.data(), false);
        // Analysis: magnitude and true frequency (in bins) of each bin.
        for (int k = 0; k <= half; ++k) {
            const auto c = buf[size_t(k)];
            const float ph = std::atan2(c.imag(), c.real());
            float d = ph - lastPh[size_t(k)] - float(k) * expct;
            lastPh[size_t(k)] = ph;
            d -= kTwoPi * std::floor(d / kTwoPi + 0.5f);
            anaMag[size_t(k)] = 2.0f * std::abs(c);
            anaFrq[size_t(k)] = float(k) + d * float(kOsamp) / kTwoPi;
        }
        // Spectral envelope: low-quefrency part of the real cepstrum. First bridge the gaps between harmonics
        // (largest bin within ±3, i.e. ±70 Hz) and floor at -60 dB, so deep gaps don't drag the envelope down.
        float peak = 1e-9f;
        for (int k = 0; k <= half; ++k) peak = std::max(peak, anaMag[size_t(k)]);
        for (int k = 0; k <= half; ++k) {
            float m = 0;
            for (int d = std::max(0, k - 3); d <= std::min(half, k + 3); ++d) m = std::max(m, anaMag[size_t(d)]);
            cep[size_t(k)] = {std::log(std::max(m, peak * 1e-3f)), 0.0f};
        }
        for (int k = half + 1; k < n; ++k) cep[size_t(k)] = cep[size_t(n - k)];
        fft(cep.data(), true);
        for (int q = 0; q < n; ++q) {
            const int d = std::min(q, n - q);
            const float wgt = d < lifter ? 1.0f : (d == lifter ? 0.5f : 0.0f);
            cep[size_t(q)] = {cep[size_t(q)].real() * wgt / float(n), 0.0f};
        }
        fft(cep.data(), false);
        for (int k = 0; k <= half; ++k) env[size_t(k)] = std::exp(cep[size_t(k)].real());
        // Move the flattened harmonics by the pitch ratio, then put formants back, stretched by `formant`.
        std::fill(synMag.begin(), synMag.end(), 0.0f);
        for (int k = 0; k <= half; ++k) synFrq[size_t(k)] = float(k);
        for (int k = 0; k <= half; ++k) {
            const int j = int(float(k) * pitch + 0.5f);
            if (j > half) break;
            synMag[size_t(j)] += anaMag[size_t(k)] / env[size_t(k)];
            synFrq[size_t(j)] = anaFrq[size_t(k)] * pitch;
        }
        for (int j = 0; j <= half; ++j) synMag[size_t(j)] *= envAt(float(j) / formant);
        // Synthesis.
        for (int j = 0; j <= half; ++j) {
            sumPh[size_t(j)] += (synFrq[size_t(j)] - float(j)) * kTwoPi / float(kOsamp) + float(j) * expct;
            sumPh[size_t(j)] -= kTwoPi * std::floor(sumPh[size_t(j)] / kTwoPi);
            float ph = sumPh[size_t(j)];
            if (whisper > 0) ph += whisper * kPi * (2.0f * rand01() - 1.0f);
            buf[size_t(j)] = std::polar(synMag[size_t(j)], ph);
        }
        for (int j = half + 1; j < n; ++j) buf[size_t(j)] = {};
        fft(buf.data(), true);
        const float g = 4.0f / (3.0f * float(half) * float(kOsamp)); // unity gain with Hann analysis + synthesis at 8x overlap
        for (int k = 0; k < n; ++k) acc[size_t(k)] += g * win[size_t(k)] * buf[size_t(k)].real();
        for (int k = 0; k < hop; ++k) outF[size_t(k)] = acc[size_t(k)];
        std::copy(acc.begin() + hop, acc.begin() + hop + n, acc.begin());
    }
};

// ------------------------------------------------------------------ Voice changer
// Pitch (two crossfading delay taps, ~15 ms, when formants simply follow pitch; the formant shifter
// otherwise), character tilt, vibrato wobble, growl (rough, irregular throat rattle), robot ring
// modulation, radio band-limit and drive.
struct VoiceChanger {
    DelayLine pitchBuf{8192}, wobbleBuf{4096};
    FormantShifter shifter;
    float fs = 48000, ratio = 1, window = 1440, phase = 0;
    bool pitchOn = false, useShifter = false;
    float shifterMix = 0;              // 0 = delay-tap shifter, 1 = formant shifter (glides when switching)
    float robot = 0, robotInc = 0, robotPh = 0;
    float radio = 0, drive = 0, driveNorm = 1;
    float wobble = 0, wobInc = 0, wobPh = 0;
    float growl = 0, grHp = 0, grLp1 = 0, grLp2 = 0, grHpC = 0, grLpC = 0;
    uint32_t grRng = 0x2545f491u;
    float mix = 1;
    float charDb = 0;

    void configure(float sampleRate, float semis, float character, float robotPct, float robotHz,
                   float radioPct, float drivePct, float wobblePct, float wobbleHz, float mixPct,
                   float formantSemis = NAN, float growlPct = 0, float whisperPct = 0) {
        if (sampleRate != fs) { fs = sampleRate; shifter.setRate(fs); }
        if (std::isnan(formantSemis)) formantSemis = semis; // classic: formants follow pitch
        ratio = std::pow(2.0f, semis / 12.0f);
        pitchOn = std::fabs(semis) > 0.01f;
        window = 0.030f * fs;
        useShifter = std::fabs(formantSemis - semis) > 0.05f || whisperPct > 0.5f;
        shifter.configure(semis, formantSemis, whisperPct);
        charDb = character;
        robot = robotPct / 100.0f;
        robotInc = kTwoPi * robotHz / fs;
        radio = radioPct / 100.0f;
        drive = 1.0f + drivePct / 100.0f * 24.0f;
        driveNorm = 0.1f / std::tanh(drive * 0.1f); // same loudness at a speaking level (-20 dBFS): grit, not volume
        wobble = wobblePct / 100.0f;
        wobInc = kTwoPi * wobbleHz / fs;
        growl = std::clamp(growlPct / 100.0f, 0.0f, 1.0f) * 0.9f;
        grHpC = std::exp(-kTwoPi * 25.0f / fs);
        grLpC = std::exp(-kTwoPi * 90.0f / fs);
        mix = mixPct / 100.0f;
    }
    void reset() {
        pitchBuf.clear(); wobbleBuf.clear(); shifter.reset();
        phase = robotPh = wobPh = 0; grHp = grLp1 = grLp2 = 0;
        shifterMix = useShifter ? 1.0f : 0.0f;
    }

    // `shelfLo`, `shelfHi`, `bpHp`, `bpLp` are filters owned by the caller (Biquad is defined in dsp.h).
    template <typename F>
    inline float process(float x, F &shelfLo, F &shelfHi, F &bpHp, F &bpLp) {
        float y = x;
        glide(shifterMix, useShifter ? 1.0f : 0.0f, 0.0008f); // ~25 ms crossfade between the two shifters
        const bool runShifter = useShifter || shifterMix > 1e-4f;
        const float s = shifter.process(x, runShifter);
        if (shifterMix < 0.9999f && pitchOn) {
            pitchBuf.push(y);
            phase += (1.0f - ratio) / window;
            phase -= std::floor(phase);
            const float p2 = phase + 0.5f - std::floor(phase + 0.5f);
            const float g1 = std::sin(3.14159265f * phase), g2 = std::sin(3.14159265f * p2);
            y = g1 * g1 * pitchBuf.tap(1.0f + phase * window) + g2 * g2 * pitchBuf.tap(1.0f + p2 * window);
        } else {
            pitchBuf.push(y);
        }
        if (runShifter) y = y * (1.0f - shifterMix) + s * shifterMix;
        if (std::fabs(charDb) > 0.05f) y = shelfHi.process(shelfLo.process(y));
        if (wobble > 0) {
            wobbleBuf.push(y);
            wobPh += wobInc;
            if (wobPh > kTwoPi) wobPh -= kTwoPi;
            y = wobbleBuf.tap(0.006f * fs + wobble * 0.004f * fs * std::sin(wobPh));
        }
        if (growl > 0) {
            // Band-limited noise (25-90 Hz) modulating the level: an irregular rattle like a growl or rasp.
            grRng ^= grRng << 13; grRng ^= grRng >> 17; grRng ^= grRng << 5;
            const float w = float(grRng) * 4.656613e-10f - 1.0f;
            grHp = grHpC * grHp + (1 - grHpC) * w;
            grLp1 = grLpC * grLp1 + (1 - grLpC) * (w - grHp);
            grLp2 = grLpC * grLp2 + (1 - grLpC) * grLp1;
            const float m = std::clamp(grLp2 * 9.0f, -1.0f, 1.0f);
            y *= 1.0f + growl * m;
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
