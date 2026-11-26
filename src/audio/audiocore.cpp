#include "audio/audiocore.h"

#include <algorithm>

namespace mf {

void Ring::write(const float *src, size_t n) {
    const size_t wi = w.load(std::memory_order_relaxed);
    const size_t space = buf.size() - (wi - r.load(std::memory_order_acquire));
    n = std::min(n, space); // drop overflow; the reader trims latency
    for (size_t i = 0; i < n; ++i) buf[(wi + i) & mask] = src[i];
    w.store(wi + n, std::memory_order_release);
}

size_t Ring::read(float *dst, size_t n) {
    const size_t ri = r.load(std::memory_order_relaxed);
    n = std::min(n, w.load(std::memory_order_acquire) - ri);
    for (size_t i = 0; i < n; ++i) dst[i] = buf[(ri + i) & mask];
    r.store(ri + n, std::memory_order_release);
    return n;
}

AudioCore::AudioCore() : scratch(16384), vmicRing(16384), monitorRing(16384) {
    chain.recorder = &recorder;
    chain.blockHook = &PluginRack::hook;
    chain.blockCtx = &rack;
}

void AudioCore::processInput(const float *in, int channels, uint32_t frames) {
    while (frames > 0) {
        const uint32_t n = std::min<uint32_t>(frames, uint32_t(scratch.size()));
        chain.process(in, channels, scratch.data(), int(n), params, meters);
        vmicRing.write(scratch.data(), n);
        if (monitorOn.load(std::memory_order_relaxed)) monitorRing.write(scratch.data(), n);
        in += size_t(n) * size_t(channels);
        frames -= n;
    }
}

// Pull from a ring with a small safety cushion and bounded latency.
void AudioCore::readRing(Ring &ring, bool &primed, float *out, uint32_t n) {
    const size_t avail = ring.available();
    if (!primed) {
        if (avail < size_t(n) * 2) {
            std::fill(out, out + n, 0.0f);
            return;
        }
        primed = true;
    }
    if (avail > size_t(n) * 4 + 1024) ring.skip(avail - size_t(n) * 2); // too much queued: catch up
    const size_t got = ring.read(out, n);
    if (got < n) {
        std::fill(out + got, out + n, 0.0f);
        primed = false;
    }
}

void AudioCore::readVirtualMic(float *out, uint32_t n) { readRing(vmicRing, vmicPrimed, out, n); }
void AudioCore::readMonitor(float *out, uint32_t n) { readRing(monitorRing, monitorPrimed, out, n); }

} // namespace mf
