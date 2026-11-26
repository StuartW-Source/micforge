// Spectrum analysis for the Voice Isolation view: FFT, log-spaced dB bins, and averaged spectra of
// recordings. Platform-independent, no Qt. Runs on the UI thread, never in the audio callback.
#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <complex>
#include <cstddef>
#include <vector>

namespace mf {

// Lock-free tap the audio thread writes into and the UI reads the latest samples from (for display
// only: a sample overwritten while being read just looks like noise for one frame).
struct ScopeTap {
    static constexpr size_t kSize = 8192;
    std::array<std::atomic<float>, kSize> buf{};
    std::atomic<size_t> w{0};
    std::atomic<bool> on{false};
    inline void push(float x) {
        const size_t i = w.load(std::memory_order_relaxed);
        buf[i & (kSize - 1)].store(x, std::memory_order_relaxed);
        w.store(i + 1, std::memory_order_release);
    }
    // Copy the newest n samples (n <= kSize) into out.
    void latest(float *out, size_t n) const {
        const size_t end = w.load(std::memory_order_acquire);
        for (size_t k = 0; k < n; ++k) out[k] = end + k >= n ? buf[(end - n + k) & (kSize - 1)].load(std::memory_order_relaxed) : 0.0f;
    }
};

// In-place radix-2 FFT (n must be a power of two).
inline void fft(std::vector<std::complex<float>> &a) {
    const size_t n = a.size();
    for (size_t i = 1, j = 0; i < n; ++i) {
        size_t bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) std::swap(a[i], a[j]);
    }
    for (size_t len = 2; len <= n; len <<= 1) {
        const float ang = -2.0f * float(M_PI) / float(len);
        const std::complex<float> wl(std::cos(ang), std::sin(ang));
        for (size_t i = 0; i < n; i += len) {
            std::complex<float> w(1, 0);
            for (size_t k = 0; k < len / 2; ++k) {
                const auto u = a[i + k], v = a[i + k + len / 2] * w;
                a[i + k] = u + v;
                a[i + k + len / 2] = u - v;
                w *= wl;
            }
        }
    }
}

// Power spectrum (|X|^2 per bin, Hann window, scaled so a full-scale sine reads 0 dB).
inline std::vector<float> powerSpectrum(const float *x, size_t n) {
    std::vector<std::complex<float>> a(n);
    for (size_t i = 0; i < n; ++i) a[i] = x[i] * float(0.5 - 0.5 * std::cos(2 * M_PI * double(i) / double(n)));
    fft(a);
    std::vector<float> p(n / 2);
    const float scale = 4.0f / (float(n) * float(n) * 0.25f); // Hann coherent gain 0.5, one-sided
    for (size_t k = 0; k < n / 2; ++k) p[k] = std::norm(a[k]) * scale;
    return p;
}

// Average power spectrum of a whole recording (frames of n with 50% overlap).
inline std::vector<float> averageSpectrum(const std::vector<float> &x, size_t n) {
    std::vector<float> acc(n / 2, 0.0f);
    size_t frames = 0;
    for (size_t i = 0; i + n <= x.size(); i += n / 2, ++frames) {
        const auto p = powerSpectrum(x.data() + i, n);
        for (size_t k = 0; k < acc.size(); ++k) acc[k] += p[k];
    }
    if (frames) for (float &v : acc) v /= float(frames);
    return acc;
}

// Reduce a power spectrum to `bins` log-spaced bands between fLo and fHi, in dB (each band = the
// strongest FFT bin in it, so tones stay at their true level).
inline std::vector<float> logBandsDb(const std::vector<float> &p, float fs, float fLo, float fHi, int bins) {
    std::vector<float> out(size_t(bins), -120.0f);
    const float binHz = fs / float(2 * p.size());
    for (int b = 0; b < bins; ++b) {
        const float f0 = fLo * std::pow(fHi / fLo, float(b) / bins), f1 = fLo * std::pow(fHi / fLo, float(b + 1) / bins);
        size_t k0 = size_t(f0 / binHz), k1 = std::max(k0 + 1, size_t(f1 / binHz + 0.5f));
        float m = 0;
        for (size_t k = k0; k < k1 && k < p.size(); ++k) m = std::max(m, p[k]);
        out[size_t(b)] = std::max(-120.0f, 10.0f * std::log10(m + 1e-14f));
    }
    return out;
}

} // namespace mf
