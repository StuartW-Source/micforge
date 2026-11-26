// Level analysis of a recording: peak, speech level, noise floor, dynamic range and clipping.
// Used by the mic test and by Setup. Platform-independent, no Qt.
#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

namespace mf {

struct LevelStats {
    bool valid = false;        // long enough to analyse
    float peakDb = -120;       // highest sample, dBFS
    float speechDb = -120;     // loud part of the recording (90th percentile of 50 ms RMS windows)
    float noiseFloorDb = -120; // quiet part (10th percentile)
    float activeRmsDb = -120;  // average level of the windows clearly above the noise floor
    float dynamicRangeDb = 0;  // speechDb - noiseFloorDb
    size_t clipped = 0;        // samples at digital full scale
};

inline LevelStats analyseLevels(const float *x, size_t n, int rate) {
    LevelStats s;
    const size_t win = size_t(rate) / 20; // 50 ms
    if (n < win * 4) return s;
    std::vector<float> db;
    db.reserve(n / win);
    float peak = 0;
    for (size_t i = 0; i < n; ++i) {
        const float a = std::fabs(x[i]);
        peak = std::max(peak, a);
        if (a >= 0.999f) ++s.clipped;
    }
    for (size_t w = 0; w + win <= n; w += win) {
        double p = 0;
        for (size_t i = 0; i < win; ++i) p += double(x[w + i]) * x[w + i];
        db.push_back(float(10.0 * std::log10(p / double(win) + 1e-12)));
    }
    std::vector<float> sorted = db;
    std::sort(sorted.begin(), sorted.end());
    auto pct = [&](double q) { return sorted[std::min(sorted.size() - 1, size_t(q * double(sorted.size() - 1) + 0.5))]; };
    s.peakDb = peak > 0 ? 20.0f * std::log10(peak) : -120.0f;
    s.noiseFloorDb = std::max(-120.0f, pct(0.10));
    s.speechDb = std::max(-120.0f, pct(0.90));
    s.dynamicRangeDb = s.speechDb - s.noiseFloorDb;
    double pw = 0;
    int cnt = 0;
    for (float d : db)
        if (d > s.noiseFloorDb + 10.0f) { pw += std::pow(10.0, d / 10.0); ++cnt; }
    s.activeRmsDb = cnt ? float(10.0 * std::log10(pw / cnt)) : s.speechDb;
    s.valid = true;
    return s;
}

// Peak envelope with `bins` points (0..1), for drawing a waveform.
inline std::vector<float> peakEnvelope(const float *x, size_t n, size_t bins) {
    std::vector<float> out(bins, 0.0f);
    if (n == 0 || bins == 0) return out;
    for (size_t b = 0; b < bins; ++b) {
        const size_t from = b * n / bins, to = std::max(from + 1, (b + 1) * n / bins);
        float pk = 0;
        for (size_t i = from; i < to && i < n; ++i) pk = std::max(pk, std::fabs(x[i]));
        out[b] = std::min(1.0f, pk);
    }
    return out;
}

} // namespace mf
