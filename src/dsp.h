// Real-time voice processing chain. Runs on the PipeWire audio thread:
// no allocation, no locks, no syscalls inside process().
#pragma once

#include "params.h"

#include <rnnoise.h>

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
    std::atomic<float> gateTilt{-120};     // closeness (bass minus treble, dB) of current sound; -120 = silence
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
    enum Type { LP, HP, Peak, LowShelf, HighShelf };
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
        denoise = rnnoise_create(nullptr);
    }
    ~VoiceChain() {
        if (denoise) rnnoise_destroy(denoise);
    }
    VoiceChain(const VoiceChain &) = delete;
    VoiceChain &operator=(const VoiceChain &) = delete;

    Recorder *recorder = nullptr; // optional tap at the gate input
    // Optional block-processing hook for third-party plugins (kept as a plain callback so this
    // header stays free of plugin/Qt dependencies).
    void (*blockHook)(void *ctx, float *io, int n) = nullptr;
    void *blockCtx = nullptr;

    void reset() {
        for (auto &b : lowCut) b.reset();
        for (auto &b : tone) b.reset();
        for (auto &b : eq) b.reset();
        deHp.reset();
        det.reset();
        tilt.reset();
        gateIsOpen = false; gateHoldLeft = 0; gateGainDb = 0;
        gDelay.fill(0); gPos = 0;
        compGrDb = 0; limGain = 1; limPos = 0;
        limDelay.fill(0); limAbs.fill(0);
        deEnv = 0; deRed = 0;
        nIn.fill(0); nOut.fill(0); nDry.fill(0); nPos = 0;
        al.reset(); warm.reset(); vc.reset(); dbl.reset(); echo.reset(); rev.reset();
        exHp.reset(); exHp2.reset(); vcLo.reset(); vcHi.reset(); vcBpHp.reset(); vcBpLp.reset();
    }

    // in: interleaved frames with inChannels channels. out: mono.
    void process(const float *in, int inChannels, float *out, int n, const Params &p, Meters &m) {
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

        for (int i = 0; i < n; ++i) {
            // 1. Input: pick channel, apply gain.
            float x;
            const float *f = in + i * inChannels;
            if (inChannels < 2 || channel == 0) x = f[0];
            else if (channel == 1) x = f[1];
            else x = 0.5f * (f[0] + f[1]);
            x *= inGain;
            inPk = std::max(inPk, std::fabs(x));

            if (bypass) {
                out[i] = x;
                outPk = std::max(outPk, std::fabs(x));
                continue;
            }

            // 2. AI noise removal (RNNoise works on 480-sample frames: 10 ms delay).
            if (noiseOn && denoise) {
                const float wet = nOut[nPos] * (1.0f / 32768.0f);
                const float dry = nDry[nPos];
                nIn[nPos] = x * 32768.0f;
                nDry[nPos] = x;
                x = dry + (wet - dry) * noiseMix;
                if (++nPos == kNoiseFrame) {
                    nPos = 0;
                    lastVad = rnnoise_process_frame(denoise, nOut.data(), nIn.data());
                }
                vadMax = std::max(vadMax, lastVad);
            }

            // 3. Low cut (removes rumble, desk thumps, plosive boom).
            if (lowCutOn) {
                x = lowCut[0].process(x);
                if (lowCutSteep) x = lowCut[1].process(x);
            }

            // 4. Gate / expander. The detector listens to a filtered band; the audio
            //    itself is delayed slightly (look-ahead) so word starts aren't clipped.
            if (recorder) recorder->push(x);
            if (gateOn) {
                const float env = det.process(x);
                gateLvl = std::max(gateLvl, env);
                tilt.process(x);
                if (env > gateCloseLin) { tiltLowSum += tilt.lowP; tiltHiSum += tilt.hiP; }

                gDelay[gPos] = x;
                const float delayed = gDelay[(gPos - gLookahead) & kGateMask];
                gPos = (gPos + 1) & kGateMask;

                // Opening needs: loud enough AND (if enabled) close enough. Closeness only decides
                // OPENING: every syllable has a vowel that passes it, and level + hold then carry the
                // brighter consonants (s, t, k) through, so your words aren't chopped.
                const bool closeOpen = !closeCheck || (tiltBrighter ? tilt.below(tiltOpenPow) : tilt.above(tiltOpenPow));
                float target;
                if (env > gateOpenLin && closeOpen) {
                    gateIsOpen = true;
                    gateHoldLeft = gateHoldSamples;
                } else if (env < gateCloseLin) {
                    if (gateHoldLeft > 0) --gateHoldLeft;
                    else gateIsOpen = false;
                } else if (gateIsOpen) {
                    gateHoldLeft = gateHoldSamples;
                }
                if (gateIsOpen) target = 0.0f;
                else if (expander) target = std::max(gateRangeDb, (linToDb(env) - gateThrDb) * (gateRatio - 1.0f));
                else target = gateRangeDb;
                anyOpen |= gateIsOpen;

                const float c = target > gateGainDb ? gAtk : gRel;
                gateGainDb = target + (gateGainDb - target) * c;
                gateRed = std::max(gateRed, -gateGainDb);
                x = delayed * dbToLin(gateGainDb);
            }

            // 4b. Auto level: steer your speaking level to a target (frozen in pauses).
            if (alOn) x = al.process(x, gateOn ? gateIsOpen : true);

            // 5. Compressor (feed-forward, soft knee, log domain).
            if (compOn) {
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
                x *= dbToLin(cMakeup - compGrDb);
            }

            // 6. De-esser: a high shelf that dips only while the "S" band is too loud.
            if (deessOn) {
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
                x = deShelf.process(x);
            }

            // 6b. Voice changer (fun).
            if (vcOn) x = vc.process(x, vcLo, vcHi, vcBpHp, vcBpLp);

            // 7. Equaliser.
            if (eqOn) {
                for (int b = 0; b < 5; ++b)
                    if (eqActive[b]) x = eq[b].process(x);
            }

            // 8. Tone.
            if (toneOn) {
                for (int b = 0; b < 3; ++b)
                    if (toneActive[b]) x = tone[b].process(x);
            }

            out[i] = x;
        }

        // 8a. Your plugins (VST3 / LV2). They work on whole blocks, so they run between two sample loops.
        if (!bypass && pluginsOn && blockHook) blockHook(blockCtx, out, n);

        for (int i = 0; i < n && !bypass; ++i) {
            float x = out[i];

            // 8b. Character and space.
            if (warmOn) x = warm.process(x);
            if (exOn) {
                // Full-wave rectifying the treble creates new harmonics an octave up: "sparkle".
                const float hp = exHp.process(x);
                x += exAmt * (0.6f * exHp2.process(std::fabs(hp)) + 0.35f * hp);
            }
            if (dblOn) x = dbl.process(x);
            if (echoOn) x = echo.process(x);
            if (revOn) x = rev.process(x);

            // 9. Output gain then limiter (never lets the signal clip).
            x *= outGain;
            if (limiterOn) {
                limDelay[limPos] = x;
                limAbs[limPos] = std::fabs(x);
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
                x = y;
            }

            out[i] = x;
            outPk = std::max(outPk, std::fabs(x));
        }

        Meters::bump(m.inPeak, linToDb(inPk));
        Meters::bump(m.outPeak, linToDb(outPk));
        if (bypass) return;
        if (noiseOn) Meters::bump(m.noiseVad, vadMax);
        if (gateOn) {
            // Closeness of whatever is making sound right now (only while something is audible).
            if (tiltLowSum > 0 || tiltHiSum > 0) {
                m.gateTilt.store(10.0f * std::log10((tiltLowSum + 1e-14f) / (tiltHiSum + 1e-14f)),
                                 std::memory_order_relaxed);
                tiltLowSum = tiltHiSum = 0;
            }
            Meters::bump(m.gateLevel, linToDb(gateLvl));
            Meters::bump(m.gateReduction, gateRed);
            Meters::bump(m.gateOpen, anyOpen ? 1.0f : 0.0f);
        }
        if (compOn) {
            Meters::bump(m.compLevel, compLvlDb);
            Meters::bump(m.compReduction, compRed);
        }
        if (deessOn) Meters::bump(m.deessReduction, deRedMax);
        if (limiterOn) Meters::bump(m.limReduction, limRed);
        m.autoLevelGain.store(alOn ? al.gainDb : 0.0f, std::memory_order_relaxed);
    }

private:
    void update(const Params &p) {
        channel = int(p.get(InputChannel) + 0.5f);
        inGain = dbToLin(p.get(InputGain));
        bypass = p.get(Bypass) > 0.5f;

        auto active = [&](int added, int on) { return p.get(added) > 0.5f && p.get(on) > 0.5f; };
        noiseOn = active(NoiseAdded, NoiseOn);
        noiseMix = p.get(NoiseAmount) / 100.0f;

        lowCutOn = active(LowCutAdded, LowCutOn);
        lowCutSteep = p.get(LowCutSteep) > 0.5f;
        if (lowCutSteep) { // 4th-order Butterworth = two stages
            lowCut[0].set(Biquad::HP, fs, p.get(LowCutFreq), 0.5412);
            lowCut[1].set(Biquad::HP, fs, p.get(LowCutFreq), 1.3066);
        } else {
            lowCut[0].set(Biquad::HP, fs, p.get(LowCutFreq), 0.7071);
        }

        gateOn = active(GateAdded, GateOn);
        expander = p.get(GateMode) > 0.5f;
        det.configure(fs, p.get(GateFocus) > 0.5f, p.get(GateScLow), p.get(GateScHigh));
        tilt.configure(fs);
        closeCheck = p.get(GateCloseCheck) > 0.5f;
        tiltBrighter = p.get(GateTiltDir) > 0.5f;
        tiltOpenPow = std::pow(10.0f, p.get(GateTilt) / 10.0f);
        gateThrDb = p.get(GateThreshold);
        gateOpenLin = dbToLin(gateThrDb);
        gateCloseLin = dbToLin(gateThrDb - p.get(GateHysteresis));
        gateRangeDb = p.get(GateRange);
        gateRatio = p.get(GateRatio);
        gateHoldSamples = int(p.get(GateHold) * 0.001f * fs);
        gLookahead = std::clamp(int(p.get(GateLookahead) * 0.001f * fs), 0, kGateSize - 1);
        gAtk = onePole(p.get(GateAttack) / 2.2f, fs);
        gRel = onePole(p.get(GateRelease) / 2.2f, fs);

        compOn = active(CompAdded, CompOn);
        cThr = p.get(CompThreshold);
        cRatio = std::max(1.0f, p.get(CompRatio));
        cKnee = p.get(CompKnee);
        cMakeup = p.get(CompMakeup);
        cAtk = onePole(p.get(CompAttack), fs);
        cRel = onePole(p.get(CompRelease), fs);

        deessOn = p.get(DeessAdded) > 0.5f && p.get(DeessOn) > 0.5f;
        deFreq = p.get(DeessFreq);
        deHp.set(Biquad::HP, fs, deFreq, 0.7071);
        deShelf.set(Biquad::HighShelf, fs, deFreq * 0.7f, 0.7071, -deRed);
        deShelfDb = deRed;
        deThr = p.get(DeessThreshold);
        deMax = p.get(DeessMax);
        deAtk = onePole(0.5f, fs);
        deRel = onePole(40.0f, fs);
        deRelG = onePole(60.0f, fs);

        eqOn = p.get(EqAdded) > 0.5f && p.get(EqOn) > 0.5f;
        for (int b = 0; b < 5; ++b) {
            const float fr = p.get(Eq1Freq + 3 * b), g = p.get(Eq1Gain + 3 * b), q = p.get(Eq1Q + 3 * b);
            const Biquad::Type t = b == 0 ? Biquad::LowShelf : b == 4 ? Biquad::HighShelf : Biquad::Peak;
            eq[b].set(t, fs, fr, q, g);
            eqActive[b] = std::fabs(g) > 0.05f;
        }

        toneOn = active(ToneAdded, ToneOn);
        pluginsOn = active(PluginsAdded, PluginsOn);

        alOn = active(AutoLevelAdded, AutoLevelOn);
        al.configure(fs, p.get(AutoLevelTarget), p.get(AutoLevelMaxGain), int(p.get(AutoLevelSpeed) + 0.5f));

        vcOn = active(VcAdded, VcOn);
        vc.configure(fs, p.get(VcPitch), p.get(VcCharacter), p.get(VcRobot), p.get(VcRobotFreq), p.get(VcRadio),
                     p.get(VcDrive), p.get(VcWobble), p.get(VcWobbleRate), p.get(VcMix));
        vcLo.set(Biquad::LowShelf, fs, 700, 0.7071, -p.get(VcCharacter) * 0.5f);
        vcHi.set(Biquad::HighShelf, fs, 1800, 0.7071, p.get(VcCharacter) * 0.5f);
        vcBpHp.set(Biquad::HP, fs, 450, 0.9);
        vcBpLp.set(Biquad::LP, fs, 2800, 0.9);

        warmOn = active(WarmthAdded, WarmthOn);
        warm.configure(fs, p.get(WarmthAmount));
        exOn = active(ExciterAdded, ExciterOn);
        exAmt = p.get(ExciterAmount) / 100.0f;
        exHp.set(Biquad::HP, fs, p.get(ExciterFreq), 0.7071);
        exHp2.set(Biquad::HP, fs, p.get(ExciterFreq), 0.7071);
        dblOn = active(DoublerAdded, DoublerOn);
        dbl.configure(fs, p.get(DoublerWidth), p.get(DoublerMix));
        echoOn = active(EchoAdded, EchoOn);
        echo.configure(fs, p.get(EchoTime), p.get(EchoFeedback), p.get(EchoMix), p.get(EchoTone));
        revOn = active(ReverbAdded, ReverbOn);
        rev.configure(p.get(ReverbSize), p.get(ReverbDamp), p.get(ReverbMix));
        const float tb = p.get(ToneBass), tp = p.get(TonePresence), ta = p.get(ToneAir);
        tone[0].set(Biquad::LowShelf, fs, 120, 0.7071, tb);
        tone[1].set(Biquad::Peak, fs, 4500, 0.9, tp);
        tone[2].set(Biquad::HighShelf, fs, 10000, 0.7071, ta);
        toneActive[0] = std::fabs(tb) > 0.05f;
        toneActive[1] = std::fabs(tp) > 0.05f;
        toneActive[2] = std::fabs(ta) > 0.05f;

        outGain = dbToLin(p.get(OutputGain));
        limiterOn = p.get(LimiterOn) > 0.5f;
        limCeil = dbToLin(p.get(LimiterCeiling));
        limLookahead = std::clamp(int(0.0015f * fs), 1, kLimSize - 1);
        lAtk = onePole(0.3f, fs);
        lRel = onePole(80.0f, fs);
    }

    float fs;
    uint32_t seenVersion = 0;

    int channel = 0;
    float inGain = 1;
    bool bypass = false;

    // AI noise removal
    static constexpr int kNoiseFrame = 480;
    DenoiseState *denoise = nullptr;
    bool noiseOn = false;
    float noiseMix = 1, lastVad = 0;
    std::array<float, kNoiseFrame> nIn{}, nOut{}, nDry{};
    int nPos = 0;

    bool lowCutOn = true, lowCutSteep = true;
    Biquad lowCut[2];

    bool gateOn = true, expander = false;
    GateDetector det;
    TiltDetector tilt;
    bool closeCheck = false, tiltBrighter = false;
    float tiltOpenPow = 4, tiltLowSum = 0, tiltHiSum = 0;
    float gateThrDb = -20, gateOpenLin = 0.1f, gateCloseLin = 0.06f, gateRangeDb = -30, gateRatio = 3;
    int gateHoldSamples = 0, gateHoldLeft = 0;
    float gAtk = 0, gRel = 0, gateGainDb = 0;
    bool gateIsOpen = false;
    static constexpr int kGateSize = 1024, kGateMask = kGateSize - 1;
    std::array<float, kGateSize> gDelay{};
    int gPos = 0, gLookahead = 0;

    bool compOn = true;
    float cThr = -24, cRatio = 3, cKnee = 6, cMakeup = 0, cAtk = 0, cRel = 0;
    float compGrDb = 0;

    bool deessOn = false;
    Biquad deHp, deShelf;
    float deFreq = 6000, deShelfDb = 0;
    uint32_t deCount = 0;
    float deThr = -30, deMax = 8, deAtk = 0, deRel = 0, deRelG = 0, deEnv = 0, deRed = 0;

    bool eqOn = false;
    Biquad eq[5];
    bool eqActive[5] = {};

    bool toneOn = true;
    Biquad tone[3];
    bool toneActive[3] = {false, false, false};

    bool pluginsOn = false;
    bool alOn = false, vcOn = false, warmOn = false, exOn = false, dblOn = false, echoOn = false, revOn = false;
    AutoLevel al;
    VoiceChanger vc;
    Biquad vcLo, vcHi, vcBpHp, vcBpLp;
    Warmth warm;
    Biquad exHp, exHp2;
    float exAmt = 0.3f;
    Doubler dbl;
    Echo echo;
    Reverb rev;

    float outGain = 1;
    bool limiterOn = true;
    float limCeil = 0.89f, limGain = 1, lAtk = 0, lRel = 0;
    static constexpr int kLimSize = 256, kLimMask = kLimSize - 1;
    std::array<float, kLimSize> limDelay{}, limAbs{};
    int limPos = 0, limLookahead = 72;
};

} // namespace mf
