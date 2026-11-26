// Offline sanity test for the voice chain: feeds synthetic signals and checks behaviour.
// Build: g++ -std=c++20 -O2 -Isrc tests/dsp_test.cpp src/optional/rnnoise_api.cpp -ldl -o dsp_test && ./dsp_test
#include "dsp.h"
#include "separation.h"

#include <cstdio>
#include <vector>

using namespace mf;

static int failures = 0;
static void check(bool ok, const char *what, double val) {
    std::printf("[%s] %-55s %8.2f\n", ok ? " OK " : "FAIL", what, val);
    if (!ok) ++failures;
}

// Stereo sine on the left channel only (like a mic on input 1 of an audio interface).
static std::vector<float> tone(double freq, double db, int frames, double rightDb = -200) {
    std::vector<float> v(frames * 2);
    const double a = std::pow(10.0, db / 20), ar = std::pow(10.0, rightDb / 20);
    for (int i = 0; i < frames; ++i) {
        v[2 * i] = float(a * std::sin(2 * M_PI * freq * i / 48000.0));
        v[2 * i + 1] = float(ar * std::sin(2 * M_PI * 1000 * i / 48000.0));
    }
    return v;
}

static double rmsDb(const std::vector<float> &v, int from, int to) {
    double s = 0;
    for (int i = from; i < to; ++i) s += double(v[i]) * v[i];
    return 10 * std::log10(std::max(s / (to - from), 1e-20));
}
static double peakDb(const std::vector<float> &v) {
    float p = 0;
    for (float x : v) p = std::max(p, std::fabs(x));
    return linToDb(p);
}

static std::vector<float> run(Params &p, const std::vector<float> &in) {
    VoiceChain chain(48000);
    Meters m;
    const int frames = int(in.size() / 2);
    std::vector<float> out(frames);
    for (int i = 0; i < frames; i += 256) {
        const int n = std::min(256, frames - i);
        chain.process(in.data() + 2 * i, 2, out.data() + i, n, p, m);
    }
    return out;
}

static void allOff(Params &p) {
    p.set(InputGain, 0); p.set(LowCutOn, 0); p.set(GateOn, 0); p.set(CompOn, 0);
    p.set(ToneOn, 0); p.set(LimiterOn, 0); p.set(OutputGain, 0);
}

int main() {
    const int N = 48000;
    { // Pass-through with everything off: unity gain, left channel only.
        Params p; allOff(p);
        auto out = run(p, tone(1000, -20, N, -6));
        check(std::fabs(rmsDb(out, N / 2, N) - (-23.01)) < 0.1, "passthrough: 1 kHz -20 dBpk -> -23 dB RMS (right ignored)", rmsDb(out, N / 2, N));
    }
    { // Input gain.
        Params p; allOff(p); p.set(InputGain, 15);
        auto out = run(p, tone(1000, -30, N));
        check(std::fabs(rmsDb(out, N / 2, N) - (-18.01)) < 0.1, "input gain +15 dB", rmsDb(out, N / 2, N));
    }
    { // Low cut at 80 Hz 24 dB/oct: 30 Hz heavily reduced, 1 kHz untouched.
        Params p; allOff(p); p.set(LowCutOn, 1);
        auto lo = run(p, tone(30, -20, N));
        auto hi = run(p, tone(1000, -20, N));
        check(rmsDb(lo, N / 2, N) < -23 - 30, "low cut: 30 Hz attenuated > 30 dB", rmsDb(lo, N / 2, N) + 23.01);
        check(std::fabs(rmsDb(hi, N / 2, N) + 23.01) < 0.2, "low cut: 1 kHz unchanged", rmsDb(hi, N / 2, N) + 23.01);
    }
    { // Gate: quiet signal (below threshold) attenuated by the range; loud signal passes.
        Params p; allOff(p); p.set(GateOn, 1); p.set(GateThreshold, -30); p.set(GateRange, -30);
        auto quiet = run(p, tone(300, -50, N));
        auto loud = run(p, tone(300, -15, N));
        check(std::fabs(rmsDb(quiet, N / 2, N) - (-53.01 - 30)) < 1.0, "gate: -50 dB tone reduced by 30 dB", rmsDb(quiet, N / 2, N) + 53.01);
        check(std::fabs(rmsDb(loud, N / 2, N) + 18.01) < 0.2, "gate: -15 dB tone passes untouched", rmsDb(loud, N / 2, N) + 18.01);
    }
    { // Gate with focus on voice: a loud 6 kHz hiss should NOT open it (sidechain LP at 500 Hz).
        Params p; allOff(p); p.set(GateOn, 1); p.set(GateThreshold, -30); p.set(GateRange, -30);
        auto hiss = run(p, tone(6000, -25, N));
        check(rmsDb(hiss, N / 2, N) < -28 - 20, "gate focus: -25 dB 6 kHz hiss stays gated", rmsDb(hiss, N / 2, N) + 28.01);
    }
    { // Compressor: 3:1 at -24, no knee, no makeup. Input -6 dBpk sine (18 dB over) -> ~12 dB GR.
        Params p; allOff(p); p.set(CompOn, 1); p.set(CompThreshold, -24); p.set(CompRatio, 3);
        p.set(CompKnee, 0); p.set(CompMakeup, 0);
        auto out = run(p, tone(1000, -6, N));
        const double pk = linToDb(std::fabs(out[N - 1]) > 0 ? 0 : 0); (void)pk;
        double outPk = 0;
        for (int i = N / 2; i < N; ++i) outPk = std::max(outPk, double(std::fabs(out[i])));
        // Peak detector with smoothing gives slightly less than the static 12 dB.
        check(linToDb(float(outPk)) < -14 && linToDb(float(outPk)) > -20, "compressor: -6 dBpk at 3:1 above -24 -> about -18 dBpk", linToDb(float(outPk)));
    }
    { // Limiter: +12 dB of output gain into a -6 dB sine must never exceed -1 dBFS.
        Params p; allOff(p); p.set(OutputGain, 12); p.set(LimiterOn, 1); p.set(LimiterCeiling, -1);
        auto out = run(p, tone(200, -6, N));
        check(peakDb(out) <= -0.99, "limiter: peaks held at or below -1 dBFS", peakDb(out));
    }
    { // Full default chain on a speech-like level, must be finite and not clip.
        Params p;
        auto out = run(p, tone(220, -25, N));
        bool finite = true;
        for (float x : out) finite &= std::isfinite(x);
        check(finite, "default chain: output finite", 0);
        check(peakDb(out) <= -0.99, "default chain: never above limiter ceiling", peakDb(out));
    }
    { // Bypass: only input gain applied.
        Params p; p.set(Bypass, 1); p.set(InputGain, 6);
        auto out = run(p, tone(1000, -20, N));
        check(std::fabs(rmsDb(out, N / 2, N) + 17.01) < 0.1, "bypass: input gain only", rmsDb(out, N / 2, N) + 17.01);
    }
    { // Expander: 10 dB below threshold at 1:3 -> about 20 dB down (not the full 60 dB range).
        Params p; allOff(p); p.set(GateOn, 1); p.set(GateMode, 1); p.set(GateFocus, 0);
        p.set(GateThreshold, -20); p.set(GateHysteresis, 0); p.set(GateRatio, 3); p.set(GateRange, -60);
        auto out = run(p, tone(1000, -30, N));
        const double red = rmsDb(out, N / 2, N) + 33.01;
        check(red < -15 && red > -25, "expander: 10 dB under at 1:3 -> ~20 dB down", red);
    }
    { // Gate look-ahead: output is delayed by the look-ahead time.
        Params p; allOff(p); p.set(GateOn, 1); p.set(GateThreshold, -60); p.set(GateLookahead, 5);
        std::vector<float> in(4000 * 2, 0.0f);
        in[2 * 1000] = 0.5f; // impulse at sample 1000
        auto out = run(p, in);
        int at = 0;
        for (int i = 0; i < 4000; ++i) if (std::fabs(out[i]) > 0.1f) { at = i; break; }
        check(at == 1240, "gate look-ahead: 5 ms = 240 samples delay", at - 1000);
    }
    { // De-esser: loud 8 kHz is reduced, 300 Hz untouched.
        Params p; allOff(p); p.set(DeessAdded, 1); p.set(DeessOn, 1); p.set(DeessFreq, 6000);
        p.set(DeessThreshold, -30); p.set(DeessMax, 10);
        auto hi = run(p, tone(8000, -10, N));
        auto lo = run(p, tone(300, -10, N));
        check(rmsDb(hi, N / 2, N) + 13.01 < -6, "de-esser: loud 8 kHz reduced > 6 dB", rmsDb(hi, N / 2, N) + 13.01);
        check(std::fabs(rmsDb(lo, N / 2, N) + 13.01) < 0.5, "de-esser: 300 Hz untouched", rmsDb(lo, N / 2, N) + 13.01);
    }
    { // EQ: +6 dB bell at 1 kHz.
        Params p; allOff(p); p.set(EqAdded, 1); p.set(EqOn, 1); p.set(Eq3Freq, 1000); p.set(Eq3Gain, 6);
        auto out = run(p, tone(1000, -20, N));
        check(std::fabs(rmsDb(out, N / 2, N) + 23.01 - 6) < 0.2, "EQ: +6 dB bell at 1 kHz", rmsDb(out, N / 2, N) + 23.01);
    }
    if (!mf::rnnoise::available()) {
        std::printf("[SKIP] AI noise removal tests: %s\n", mf::rnnoise::error().c_str());
    } else { // RNNoise runs, adds a 10 ms frame of delay, output finite; white noise is reduced.
        Params p; allOff(p); p.set(NoiseAdded, 1); p.set(NoiseOn, 1);
        std::vector<float> in(N * 2);
        uint32_t s = 1;
        for (int i = 0; i < N; ++i) { s = s * 1664525u + 1013904223u; in[2 * i] = 0.05f * ((s >> 8) / 8388608.0f - 1.0f); }
        auto out = run(p, in);
        bool finite = true;
        for (float x : out) finite &= std::isfinite(x);
        check(finite, "AI noise removal: output finite", 0);
        check(rmsDb(out, N / 2, N) < rmsDb(in, N, 2 * N) - 6 - 3, "AI noise removal: steady noise reduced > 6 dB",
              rmsDb(out, N / 2, N) - (rmsDb(in, N, 2 * N) - 3));
    }
    { // Voice separation: close low voice (you) vs distant higher voice (someone else).
        auto voice = [&](double f0, double levelDb, double bassTilt, int seed) {
            // Harmonic voice with syllables (on 180 ms / off 70 ms). bassTilt = dB per octave
            // falling from f0 (positive = more bass, like the SM57 proximity effect up close).
            std::vector<float> v(N * 5);
            const double a = std::pow(10.0, levelDb / 20);
            for (size_t i = 0; i < v.size(); ++i) {
                const double t = i / 48000.0;
                const double syl = std::fmod(t + seed * 0.05, 0.25) < 0.18 ? 1.0 : 0.0;
                double x = 0;
                for (int h = 1; h * f0 < 5000; ++h)
                    x += std::pow(10.0, -bassTilt * std::log2(h) / 20) / h * std::sin(2 * M_PI * h * f0 * t + h);
                v[i] = float(a * syl * x);
            }
            return v;
        };
        std::vector<float> room(N * 4);
        uint32_t s = 7;
        for (auto &x : room) { s = s * 1664525u + 1013904223u; x = 0.001f * ((s >> 8) / 8388608.0f - 1.0f); }
        auto you = voice(110, -14, 6, 0);       // close + low + proximity bass boost
        auto other = voice(210, -24, -3, 3);  // further away, higher, thinner
        for (size_t i = 0; i < you.size(); ++i) { you[i] += room[i % room.size()]; other[i] += room[i % room.size()]; }
        auto r = analyseSeparation(room, other, you);
        std::printf("       chosen band: %s %.0f-%.0f Hz, room %.1f, others %.1f, you %.1f, threshold %.1f\n",
                    r.focus ? "on" : "off", r.scLow, r.scHigh, r.roomDb, r.othersDb, r.youDb, r.threshold);
        check(r.heardYou, "separation: heard the user", 0);
        check(r.focus && r.scHigh <= 500, "separation: picks a low-pass detector band", r.scHigh);
        check(r.separation > r.broadbandSeparation + 3, "separation: band beats broadband by > 3 dB",
              r.separation - r.broadbandSeparation);
        check(r.threshold > r.othersDb && r.threshold < r.youDb, "separation: threshold between other voice and you", r.threshold);

        // Apply it and check the gate really keeps the other out and lets you in.
        Params p; allOff(p);
        p.set(GateOn, 1); p.set(GateFocus, r.focus); p.set(GateScLow, r.scLow); p.set(GateScHigh, r.scHigh);
        p.set(GateThreshold, r.threshold); p.set(GateHysteresis, r.hysteresis); p.set(GateRange, -40);
        auto stereo = [](const std::vector<float> &m) { std::vector<float> v(m.size() * 2); for (size_t i = 0; i < m.size(); ++i) v[2 * i] = m[i]; return v; };
        auto pOut = run(p, stereo(other));
        auto yOut = run(p, stereo(you));
        const double pRed = rmsDb(pOut, N, 5 * N) - rmsDb(other, N, 5 * N);
        const double yRed = rmsDb(yOut, N, 5 * N) - rmsDb(you, N, 5 * N);
        check(pRed < -25, "separation: other voice gated out (> 25 dB down)", pRed);
        check(yRed > -1.5, "separation: your voice passes (< 1.5 dB lost)", yRed);

        // --- Loud other: +15 dB (shouting), slightly thinner. Louder at the mic than YOU are.
        auto loud = voice(210, -9, -4, 5);
        for (size_t i = 0; i < loud.size(); ++i) loud[i] += room[i % room.size()];
        std::vector<float> othersAll(other);
        othersAll.insert(othersAll.end(), loud.begin(), loud.end());
        auto r2 = analyseSeparation(room, othersAll, you);
        std::printf("       closeness: you %.1f dB, others %.1f dB, gap %.1f, threshold %.1f, measured %s, level thr %.1f\n",
                    r2.youTilt, r2.othersTilt, r2.tiltGap, r2.tiltThreshold, r2.tiltMeasured ? "yes" : "no", r2.threshold);
        check(r2.tiltMeasured, "closeness: gap measured from the recordings", r2.tiltGap);
        check(r2.threshold < r2.youDb, "closeness: level threshold stays below your voice", r2.threshold - r2.youDb);

        // Level-only gate (old behaviour): the shouting other voice gets through.
        auto loudOutLevelOnly = run(p, stereo(loud));
        const double leakLevelOnly = rmsDb(loudOutLevelOnly, N, 5 * N) - rmsDb(loud, N, 5 * N);
        check(leakLevelOnly > -6, "closeness: (without it) shouting other voice leaks through", leakLevelOnly);

        // With the closeness check: blocked, and you still pass.
        p.set(GateThreshold, r2.threshold); p.set(GateCloseCheck, 1); p.set(GateTilt, r2.tiltThreshold);
        auto lOut = run(p, stereo(loud));
        auto y2Out = run(p, stereo(you));
        auto pOut2 = run(p, stereo(other));
        const double lRed = rmsDb(lOut, N, 5 * N) - rmsDb(loud, N, 5 * N);
        const double y2Red = rmsDb(y2Out, N, 5 * N) - rmsDb(you, N, 5 * N);
        const double p2Red = rmsDb(pOut2, N, 5 * N) - rmsDb(other, N, 5 * N);
        check(lRed < -25, "closeness: shouting other voice gated out (> 25 dB down)", lRed);
        check(p2Red < -25, "closeness: normal other voice still gated out", p2Red);
        check(y2Red > -1.5, "closeness: your voice still passes (< 1.5 dB lost)", y2Red);

        // --- No other-voices recording at all (they won't take part): closeness from your voice only.
        auto r3 = analyseSeparation(room, {}, you);
        check(r3.heardYou && !r3.tiltMeasured, "solo setup: works without other voices", r3.tiltThreshold);
        Params q; allOff(q);
        q.set(GateOn, 1); q.set(GateFocus, r3.focus); q.set(GateScLow, r3.scLow); q.set(GateScHigh, r3.scHigh);
        q.set(GateThreshold, r3.threshold); q.set(GateHysteresis, r3.hysteresis); q.set(GateRange, -40);
        q.set(GateCloseCheck, 1); q.set(GateTilt, r3.tiltThreshold);
        const double l3 = rmsDb(run(q, stereo(loud)), N, 5 * N) - rmsDb(loud, N, 5 * N);
        const double y3 = rmsDb(run(q, stereo(you)), N, 5 * N) - rmsDb(you, N, 5 * N);
        check(l3 < -25, "solo setup: shouting other voice still gated out", l3);
        check(y3 > -1.5, "solo setup: your voice passes", y3);

        // --- Digitally silent room (AI noise removal before the gate): must not break the analysis.
        std::vector<float> silent(room.size(), 0.0f);
        auto r4 = analyseSeparation(silent, other, you);
        std::printf("       silent room: room %.1f, you %.1f, others %.1f, threshold %.1f\n", r4.roomDb, r4.youDb, r4.othersDb, r4.threshold);
        check(r4.heardYou && r4.youDb > -45, "silent room: your speech level measured correctly", r4.youDb);
        check(r4.threshold < r4.youDb && r4.threshold > r4.othersDb, "silent room: threshold between others and you", r4.threshold);

        // --- Per-step checks: flag the recording that went wrong.
        check(checkRoom(r).status == StepCheck::Ok && checkYou(r).status == StepCheck::Ok &&
              checkOthers(r).status == StepCheck::Ok, "step checks: a good run is all OK", 0);
        std::vector<float> noisyRoom(room);
        for (size_t i = 0; i < you.size() / 3 && i < noisyRoom.size(); ++i) noisyRoom[i] += you[i]; // talked during "quiet"
        check(checkRoom(analyseSeparation(noisyRoom, other, you)).status == StepCheck::Warn,
              "step checks: talking in the quiet step is flagged", 0);
        check(checkOthers(analyseSeparation(room, you, you)).status == StepCheck::Warn,
              "step checks: 'other voices' that were really you is flagged", 0);
        check(checkYou(analyseSeparation(room, other, room)).status == StepCheck::Bad,
              "step checks: silence in the 'you' step is flagged", 0);
    }
    // ================================================================ new effects
    auto zcFreq = [](const std::vector<float> &v, int from, int to) {
        int zc = 0;
        for (int i = from + 1; i < to; ++i) if ((v[i - 1] < 0) != (v[i] < 0)) ++zc;
        return zc / 2.0 / ((to - from) / 48000.0);
    };
    auto offAll = [&](Params &p) {
        allOff(p);
        for (int id : {GateAdded, CompAdded, ToneAdded, LowCutAdded}) p.set(id, 0);
    };
    { // Voice changer pitch: +12 st doubles, -12 st halves.
        Params p; offAll(p); p.set(VcAdded, 1); p.set(VcOn, 1); p.set(VcPitch, 12);
        auto up = run(p, tone(200, -12, N));
        p.set(VcPitch, -12);
        auto dn = run(p, tone(400, -12, N));
        const double fu = zcFreq(up, N / 2, N), fd = zcFreq(dn, N / 2, N);
        check(std::fabs(fu - 400) < 25, "voice changer: +12 semitones turns 200 Hz into 400 Hz", fu);
        check(std::fabs(fd - 200) < 15, "voice changer: -12 semitones turns 400 Hz into 200 Hz", fd);
    }
    { // Radio: removes deep bass. Robot: changes the sound.
        Params p; offAll(p); p.set(VcAdded, 1); p.set(VcOn, 1); p.set(VcRadio, 100);
        auto lo = run(p, tone(100, -12, N));
        check(rmsDb(lo, N / 2, N) + 15.01 < -10, "voice changer: radio cuts 100 Hz by > 10 dB", rmsDb(lo, N / 2, N) + 15.01);
        p.set(VcRadio, 0); p.set(VcRobot, 100); p.set(VcRobotFreq, 50);
        auto in = tone(1000, -12, N);
        auto ro = run(p, in);
        double dot = 0, ea = 0, eb = 0;
        for (int i = N / 2; i < N; ++i) { dot += in[2 * i] * ro[i]; ea += in[2 * i] * in[2 * i]; eb += double(ro[i]) * ro[i]; }
        check(std::fabs(dot / std::sqrt(ea * eb)) < 0.3, "voice changer: robot ring-modulates (output unlike input)", dot / std::sqrt(ea * eb));
    }
    { // Echo: an impulse comes back after the echo time.
        Params p; offAll(p); p.set(EchoAdded, 1); p.set(EchoOn, 1); p.set(EchoTime, 250); p.set(EchoMix, 50);
        p.set(EchoFeedback, 0); p.set(EchoTone, 16000);
        std::vector<float> in(N * 2, 0.0f); in[2 * 1000] = 0.8f;
        auto out = run(p, in);
        int at = 0; float mx = 0;
        for (int i = 2000; i < N; ++i) if (std::fabs(out[i]) > mx) { mx = std::fabs(out[i]); at = i; }
        check(std::abs(at - (1000 + 12000)) < 30, "echo: repeat arrives 250 ms later", (at - 1000) / 48.0);
    }
    { // Reverb: an impulse leaves a decaying tail.
        Params p; offAll(p); p.set(ReverbAdded, 1); p.set(ReverbOn, 1); p.set(ReverbSize, 70); p.set(ReverbMix, 50);
        std::vector<float> in(N * 2, 0.0f); in[2 * 100] = 0.8f;
        auto out = run(p, in);
        const double early = rmsDb(out, 4800, 14400), late = rmsDb(out, 38400, 48000);
        check(early > -60 && late < early - 10, "reverb: tail present and decaying", early - late);
    }
    { // Auto level: a quiet talker (-40 dB) is brought up towards the -18 dB target.
        Params p; offAll(p); p.set(AutoLevelAdded, 1); p.set(AutoLevelOn, 1); p.set(AutoLevelTarget, -18);
        p.set(AutoLevelMaxGain, 24); p.set(AutoLevelSpeed, 2);
        std::vector<float> in(N * 6 * 2);
        for (int i = 0; i < N * 6; ++i) in[2 * i] = float(0.01414 * std::sin(2 * M_PI * 300 * i / 48000.0)); // -40 dB RMS
        auto out = run(p, in);
        const double lvl = rmsDb(out, N * 5, N * 6);
        check(std::fabs(lvl + 18) < 3, "auto level: -40 dB voice brought to about -18 dB", lvl);
    }
    { // Warmth: rounds off loud peaks but leaves quiet sound alone.
        Params p; offAll(p); p.set(WarmthAdded, 1); p.set(WarmthOn, 1); p.set(WarmthAmount, 100);
        auto loud = run(p, tone(300, -1, N));
        auto quiet = run(p, tone(300, -40, N));
        check(peakDb(loud) < -3, "warmth: loud peaks rounded off", peakDb(loud));
        check(std::fabs(rmsDb(quiet, N / 2, N) + 43.01) < 1.0, "warmth: quiet sound almost unchanged", rmsDb(quiet, N / 2, N) + 43.01);
    }
    { // Exciter: adds sparkle to treble, leaves bass alone.
        Params p; offAll(p); p.set(ExciterAdded, 1); p.set(ExciterOn, 1); p.set(ExciterAmount, 100); p.set(ExciterFreq, 3000);
        auto hi = run(p, tone(5000, -12, N));
        auto lo = run(p, tone(200, -12, N));
        check(rmsDb(hi, N / 2, N) > -15.01 + 1, "exciter: treble gets brighter", rmsDb(hi, N / 2, N) + 15.01);
        check(std::fabs(rmsDb(lo, N / 2, N) + 15.01) < 0.5, "exciter: bass untouched", rmsDb(lo, N / 2, N) + 15.01);
    }
    { // Doubler: keeps the level sensible.
        Params p; offAll(p); p.set(DoublerAdded, 1); p.set(DoublerOn, 1);
        auto out = run(p, tone(300, -12, N));
        check(std::fabs(rmsDb(out, N / 2, N) + 15.01) < 4, "doubler: level within 4 dB", rmsDb(out, N / 2, N) + 15.01);
    }
    { // Removing a core effect really takes it out of the signal path.
        Params p; offAll(p); p.set(CompOn, 1); p.set(CompMakeup, 12); p.set(CompThreshold, -60);
        auto out = run(p, tone(1000, -20, N));
        check(std::fabs(rmsDb(out, N / 2, N) + 23.01) < 0.1, "removed compressor: no effect on the sound", rmsDb(out, N / 2, N) + 23.01);
    }
    { // Everything on at once, extreme settings: stays finite and below the limiter ceiling.
        Params p;
        for (int id : {NoiseAdded, AutoLevelAdded, DeessAdded, VcAdded, EqAdded, WarmthAdded, ExciterAdded, DoublerAdded,
                       EchoAdded, ReverbAdded}) p.set(id, 1);
        p.set(VcPitch, 7); p.set(VcRobot, 50); p.set(VcDrive, 100); p.set(VcWobble, 100); p.set(EchoFeedback, 90);
        p.set(ReverbSize, 100); p.set(OutputGain, 24);
        auto out = run(p, tone(220, -6, N * 2));
        bool finite = true;
        for (float x : out) finite &= std::isfinite(x);
        check(finite && peakDb(out) <= -0.99, "all effects at extremes: finite, never above ceiling", peakDb(out));
    }
    std::printf("\n%s (%d failure%s)\n", failures ? "FAILED" : "ALL PASSED", failures, failures == 1 ? "" : "s");
    return failures ? 1 : 0;
}
