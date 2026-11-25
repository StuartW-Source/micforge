// Analyse saved setup recordings (room/others/you .f32) with MicForge's own detectors.
// Build: g++ -std=c++20 -O2 -Isrc tests/analyse_recordings.cpp -lrnnoise -o analyse_recordings
// Run:   ./analyse_recordings ~/.local/share/MicForge/MicForge/calib
#include "separation.h"

#include <cstdio>
#include <fstream>
#include <string>

using namespace mf;

static std::vector<float> load(const std::string &path) {
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f) return {};
    const auto n = size_t(f.tellg()) / sizeof(float);
    std::vector<float> v(n);
    f.seekg(0);
    f.read(reinterpret_cast<char *>(v.data()), std::streamsize(n * sizeof(float)));
    return v;
}

struct Frame { float level, tilt; };

// Per 10 ms frame: detector level in the given band, and closeness (tilt).
static std::vector<Frame> frames(const std::vector<float> &x, float gainDb, bool focus, float lo, float hi) {
    GateDetector d; d.configure(48000, focus, lo, hi);
    TiltDetector t; t.configure(48000);
    const float g = dbToLin(gainDb);
    std::vector<Frame> out;
    float mx = 0; int k = 0;
    for (size_t i = 0; i < x.size(); ++i) {
        const float s = x[i] * g;
        mx = std::max(mx, d.process(s));
        t.process(s);
        if (++k == 480) { out.push_back({linToDb(mx), t.tiltDb()}); mx = 0; k = 0; }
    }
    return out;
}

static float pct(std::vector<float> v, float p) { return detail::percentile(std::move(v), p); }

int main(int argc, char **argv) {
    const std::string dir = argc > 1 ? argv[1] : ".";
    auto room = load(dir + "/room.f32"), others = load(dir + "/others.f32"), you = load(dir + "/you.f32");
    std::printf("Lengths: room %.1f s, others %.1f s, you %.1f s\n\n", room.size() / 48000.0, others.size() / 48000.0,
                you.size() / 48000.0);

    const auto r = analyseSeparation(room, others, you);
    std::printf("Analysis: band %s %.0f-%.0f Hz | room %.1f  others %.1f  you %.1f | separation %.1f (broadband %.1f) | quality %d\n",
                r.focus ? "on" : "off", r.scLow, r.scHigh, r.roomDb, r.othersDb, r.youDb, r.separation, r.broadbandSeparation, r.quality);
    std::printf("Threshold %.1f (hyst %.1f) | closeness: you typical %.1f, others extreme %.1f, gap %.1f, measured %d, %s, threshold %.1f\n",
                r.threshold, r.hysteresis, r.youTilt, r.othersTilt, r.tiltGap, r.tiltMeasured,
                r.tiltBrighter ? "you brighter" : "you bassier", r.tiltThreshold);
    std::printf("Checks: room=%d others=%d you=%d   (0 ok, 1 warn, 2 bad)\n\n",
                checkRoom(r).status, checkOthers(r).status, checkYou(r).status);

    // Closeness distributions during speech frames.
    for (auto [name, buf] : {std::pair{"you", &you}, std::pair{"others", &others}}) {
        auto f = frames(*buf, 0, false, 20, 8000);
        std::vector<float> lv; for (auto &x : f) lv.push_back(x.level);
        const float gate = detail::speechGate(lv, std::max(-90.0f, pct(frames(room, 0, false, 20, 8000).empty() ? std::vector<float>{-90} : [&]{ std::vector<float> q; for (auto &x : frames(room, 0, false, 20, 8000)) q.push_back(x.level); return q; }(), 0.95f)));
        std::vector<float> tl; for (auto &x : f) if (x.level > gate) tl.push_back(x.tilt);
        std::printf("Closeness (%s, %zu speech frames): P5 %.1f  P15 %.1f  P50 %.1f  P85 %.1f  P95 %.1f\n", name, tl.size(),
                    pct(tl, 0.05f), pct(tl, 0.15f), pct(tl, 0.5f), pct(tl, 0.85f), pct(tl, 0.95f));
    }

    // Simulate the real gate on each recording, with others made louder.
    std::printf("\nGate simulation (fraction of speech let through; others should be ~0, you ~1):\n");
    std::printf("%-34s %8s %8s %8s %8s\n", "rule", "you", "oth+0", "oth+6", "oth+12");
    auto run = [&](const char *label, bool close, int dir, float tiltThr) {
        auto passFrac = [&](const std::vector<float> &x, float gainDb) {
            Params p;
            for (int id : {NoiseAdded, CompOn, ToneOn, LimiterOn, LowCutOn}) p.set(id, 0);
            p.set(InputGain, gainDb);
            p.set(GateOn, 1); p.set(GateFocus, r.focus); p.set(GateScLow, r.scLow); p.set(GateScHigh, r.scHigh);
            p.set(GateThreshold, r.threshold); p.set(GateHysteresis, r.hysteresis); p.set(GateRange, -40);
            p.set(GateLookahead, 0);
            p.set(GateCloseCheck, close ? 1 : 0); p.set(GateTilt, tiltThr);
            if (dir < 0) p.set(GateTiltDir, 1);
            VoiceChain c(48000); Meters m;
            std::vector<float> st(x.size() * 2), out(x.size());
            for (size_t i = 0; i < x.size(); ++i) st[2 * i] = x[i];
            for (size_t i = 0; i < x.size(); i += 480) {
                const int n = int(std::min<size_t>(480, x.size() - i));
                c.process(st.data() + 2 * i, 2, out.data() + i, n, p, m);
            }
            // energy-weighted pass fraction
            double ein = 0, eout = 0;
            for (size_t i = 0; i < x.size(); ++i) { ein += double(x[i]) * x[i]; eout += double(out[i]) * out[i]; }
            return ein > 0 ? eout / (ein * std::pow(10.0, gainDb / 10)) : 0.0;
        };
        std::printf("%-34s %8.3f %8.3f %8.3f %8.3f\n", label, passFrac(you, 0), passFrac(others, 0), passFrac(others, 6),
                    passFrac(others, 12));
    };
    run("level only", false, 1, 0);
    char lbl[80];
    std::snprintf(lbl, sizeof lbl, "closeness (setup's choice: %s %.1f)", r.tiltBrighter ? "<" : ">", r.tiltThreshold);
    run(lbl, true, r.tiltBrighter ? -1 : 1, r.tiltThreshold);
    for (int i = 2; i < argc; ++i) {
        const float thr = std::stof(argv[i]);
        std::snprintf(lbl, sizeof lbl, "closeness: need more bass (> %.1f)", thr);
        run(lbl, true, 1, thr);
    }
    return 0;
}
