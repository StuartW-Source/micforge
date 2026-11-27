// Diagnoses "the start of my sentences is quiet" from a Mic Test export, locally:
//   g++ -std=c++20 -O2 -Isrc tests/replay_mictest.cpp src/optional/rnnoise_api.cpp -ldl -o /tmp/replay
//   /tmp/replay ~/.config/MicForge/MicForge.conf micforge-test-…-raw.wav micforge-test-…-processed.wav
// Replays the raw take through the user's settings (checking it matches the exported processed take),
// finds each phrase start (a rise 20 dB above the room after >= 300 ms of quiet), and prints how much the
// first 60 ms and the next 150 ms are turned down, with the Voice Gate off and Noise Removal off for
// comparison. Nothing is sent anywhere or saved.
#include "dsp.h"
#include <algorithm>
#include <array>
#include <cstring>
#include <cstdio>
#include <fstream>
#include <map>
#include <string>
#include <vector>
using namespace mf;

std::vector<float> loadWav(const char *p) {
    std::ifstream f(p, std::ios::binary); std::vector<char> b((std::istreambuf_iterator<char>(f)), {});
    size_t pos = 12; std::vector<float> out;
    while (pos + 8 <= b.size()) {
        std::string id(b.data() + pos, 4); uint32_t sz; std::memcpy(&sz, b.data() + pos + 4, 4);
        if (id == "data") { const int16_t *s = reinterpret_cast<const int16_t *>(b.data() + pos + 8); for (uint32_t i = 0; i < sz / 2; ++i) out.push_back(s[i] / 32768.0f); break; }
        pos += 8 + sz;
    }
    return out;
}
std::map<std::string, double> user;
std::vector<float> run(const std::vector<float> &x, const std::map<int, float> &over) {
    Params p;
    for (int i = 0; i < PARAM_COUNT; ++i) { auto it = user.find(kParams[i].key); if (it != user.end()) p.set(i, float(it->second)); }
    p.set(InputGain, 0); p.set(InputChannel, 0); p.set(Mute, 0); p.set(Bypass, 0); p.set(PluginsOn, 0);
    for (auto &[k, v] : over) p.set(k, v);
    std::vector<float> in(x.size() * 2), out(x.size());
    for (size_t i = 0; i < x.size(); ++i) in[2 * i] = x[i];
    VoiceChain c(48000); Meters m;
    for (size_t i = 0; i + 256 <= x.size(); i += 256) c.process(in.data() + 2 * i, 2, out.data() + i, 256, p, m);
    return out;
}
size_t delayOf(const std::vector<float> &a, const std::vector<float> &y) {
    size_t best = 0; double bc = -1e9;
    for (size_t d = 0; d < 2400; d += 2) {
        double c = 0, e = 1e-12;
        for (size_t i = 0; i + d < y.size() && i < a.size(); i += 6) { c += double(a[i]) * y[i + d]; e += double(y[i + d]) * y[i + d]; }
        if (c / std::sqrt(e) > bc) { bc = c / std::sqrt(e); best = d; }
    }
    return best;
}
std::vector<double> frames(const std::vector<float> &y, size_t shift) {
    std::vector<double> f;
    for (size_t i = 0; i + 960 + shift <= y.size(); i += 960) { double e = 0; for (size_t k = 0; k < 960; ++k) e += double(y[i + k + shift]) * y[i + k + shift]; f.push_back(10 * std::log10(e / 960 + 1e-14)); }
    return f;
}
int main(int, char **argv) {
    std::ifstream conf(argv[1]); std::string line; bool inP = false;
    while (std::getline(conf, line)) { if (!line.empty() && line[0] == '[') inP = line == "[params]"; else if (inP) { auto eq = line.find('='); if (eq != std::string::npos) user[line.substr(0, eq)] = std::stod(line.substr(eq + 1)); } }
    auto raw = loadWav(argv[2]), proc = loadWav(argv[3]);
    // Sanity: my replay vs what MicForge actually produced.
    auto mine = run(raw, {});
    const size_t dm = delayOf(raw, mine), dp = delayOf(raw, proc);
    auto fm = frames(mine, dm), fp = frames(proc, dp), fr = frames(raw, 0);
    std::vector<double> so = fr; std::sort(so.begin(), so.end());
    const double floorDb = so[so.size() / 10], peak = so[so.size() * 95 / 100];
    double diff = 0; int n = 0;
    for (size_t i = 0; i < std::min(fm.size(), fp.size()); ++i) if (fr[i] > peak - 30) { diff += std::fabs(fm[i] - fp[i]); ++n; }
    std::printf("raw take: background %.0f dB, speech peaks %.0f dB. Replay vs your exported processed file: %.2f dB average difference on speech\n\n", floorDb, peak, diff / std::max(1, n));
    // Sample-accurate phrase onsets in the raw take: 5 ms envelope rises 20 dB above the room after >= 300 ms near the room level.
    std::vector<size_t> onsets;
    {
        // 10 ms power envelope (RMS), compared with the room's level in dB.
        double pw = 0; const double roomP = std::pow(10.0, floorDb / 10), a = std::exp(-1.0 / (0.010 * 48000));
        size_t quiet = 0;
        for (size_t i = 0; i < raw.size(); ++i) {
            pw = a * pw + (1 - a) * double(raw[i]) * raw[i];
            if (pw < roomP * 4) ++quiet;                                  // within 6 dB of the room
            else { if (pw > roomP * 100 && quiet > 14400) onsets.push_back(i); if (pw > roomP * 100) quiet = 0; }
        }
    }
    std::map<int, float> refOv = {{NoiseOn, 0}, {GateOn, 0}, {BreathOn, 0}, {CompOn, 0}, {DeessOn, 0}, {LimiterOn, 0}, {LowCutPlosive, 0}, {OutputGain, float(user["compMakeup"])}};
    auto ref = run(raw, refOv); const size_t dref = delayOf(raw, ref);
    auto energy = [](const std::vector<float> &y, size_t from, size_t len) { double e = 0; for (size_t i = from; i < from + len && i < y.size(); ++i) e += double(y[i]) * y[i]; return e; };
    std::printf("%zu phrase starts found\n\n", onsets.size());
    auto per = [&](std::map<int, float> ov) {
        auto y = run(raw, ov); const size_t d = delayOf(raw, y);
        std::vector<std::array<double, 3>> out;
        for (size_t on : onsets) {
            const size_t st = on > 240 ? on - 240 : 0;
            const double r1 = energy(ref, st + dref, 2880), y1 = energy(y, st + d, 2880);
            const double r2 = energy(ref, st + dref + 2880, 7200), y2 = energy(y, st + d + 2880, 7200);
            out.push_back({10 * std::log10(r1 / 2880 + 1e-14), 10 * std::log10(r1 / (y1 + 1e-20)), 10 * std::log10(r2 / (y2 + 1e-20))});
        }
        return out;
    };
    auto a1 = per({}), a2 = per({{GateOn, 0}}), a3 = per({{NoiseOn, 0}});
    std::printf("dB turned down (vs. no gate, no noise removal, no dynamics). The first phrase of a take is often affected by start-up.\n");
    std::printf("phrase at   level(first 60 ms) | your settings: first 60 / next 150 | gate off | noise removal off\n");
    for (size_t k = 0; k < onsets.size(); ++k)
        std::printf("%6.2f s    %6.1f dB           | %5.1f / %5.1f                   | %5.1f / %5.1f | %5.1f / %5.1f\n",
                    onsets[k] / 48000.0, a1[k][0], a1[k][1], a1[k][2], a2[k][1], a2[k][2], a3[k][1], a3[k][2]);
}
