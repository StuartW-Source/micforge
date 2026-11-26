#include "audio/audiocore.h"

#include <algorithm>
#include <chrono>
#include <cmath>

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
    chain.scope = &scope;
    chain.blockHook = &PluginRack::hook;
    chain.blockCtx = &rack;
}

namespace {
int64_t nowNs() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
}
} // namespace

void AudioCore::processInput(const float *in, int channels, uint32_t frames) {
    const int64_t t0 = nowNs();
    const double blockNs = 1e9 * double(frames) / kRate;
    if (stats.lastBlockNs != 0) {
        const int64_t gap = t0 - stats.lastBlockNs;
        // A gap over two blocks means audio was lost; gaps over a second are pauses (stream suspended).
        if (gap > int64_t(2.0 * blockNs) + 2'000'000 && gap < 1'000'000'000) stats.lateCallbacks.fetch_add(1, std::memory_order_relaxed);
    }
    stats.lastBlockNs = t0;

    // Input clipping (before input gain) and the mic test's raw take.
    const int channel = int(params.get(InputChannel) + 0.5f);
    const float gain = dbToLin(params.get(InputGain));
    const bool rec = test.recording.load(std::memory_order_acquire);
    size_t tpos = rec ? test.pos.load(std::memory_order_relaxed) : 0;
    uint64_t clips = 0;
    for (uint32_t i = 0; i < frames; ++i) {
        const float *f = in + size_t(i) * size_t(channels);
        const float l = f[0], r = channels > 1 ? f[1] : f[0];
        const float x = channels < 2 || channel == 0 ? l : channel == 1 ? r : 0.5f * (l + r);
        const float pk = channel == 2 ? std::max(std::fabs(l), std::fabs(r)) : std::fabs(x);
        if (pk >= 0.999f) ++clips;
        if (rec && tpos + i < test.raw.size()) test.raw[tpos + i] = x * gain;
    }
    if (clips) {
        stats.inputClips.fetch_add(clips, std::memory_order_relaxed);
        stats.lastClipNs.store(t0, std::memory_order_relaxed);
    }

    const float *src = in;
    uint32_t left = frames;
    size_t ppos = tpos;
    while (left > 0) {
        const uint32_t n = std::min<uint32_t>(left, uint32_t(scratch.size()));
        chain.process(src, channels, scratch.data(), int(n), params, meters);
        vmicRing.write(scratch.data(), n);
        if (liveMonitor.load(std::memory_order_relaxed) && monitorOn.load(std::memory_order_relaxed))
            monitorRing.write(scratch.data(), n);
        if (rec)
            for (uint32_t i = 0; i < n && ppos < test.processed.size(); ++i) test.processed[ppos++] = scratch[i];
        src += size_t(n) * size_t(channels);
        left -= n;
    }
    if (rec) {
        const size_t end = std::min(tpos + frames, test.raw.size());
        test.pos.store(end, std::memory_order_release);
        if (end >= test.raw.size()) test.recording.store(false, std::memory_order_release);
    }

    const float us = float(nowNs() - t0) / 1000.0f;
    const float avg = stats.procUsAvg.load(std::memory_order_relaxed);
    stats.procUsAvg.store(avg == 0 ? us : avg + 0.02f * (us - avg), std::memory_order_relaxed);
    Meters::bump(stats.procUsMax, us);
    Meters::bump(stats.loadMax, float(us * 1000.0 / blockNs));
    stats.blockFrames.store(frames, std::memory_order_relaxed);
    stats.blocks.fetch_add(1, std::memory_order_relaxed);
}

void AudioCore::prepareTest(size_t frames) {
    if (test.recording.load() || test.playing.load()) return;
    test.raw.assign(frames, 0.0f);
    test.processed.assign(frames, 0.0f);
    test.pos.store(0);
}

void AudioCore::startTestRecording() {
    if (test.raw.empty() || test.playing.load()) return;
    test.pos.store(0, std::memory_order_relaxed);
    test.recording.store(true, std::memory_order_release);
}

void AudioCore::startPlayback(int which, float gain) {
    if (test.recording.load()) return;
    test.playing.store(0, std::memory_order_release);
    test.playLen = std::min(test.pos.load(), (which == 1 ? test.raw : test.processed).size());
    test.playPos.store(0, std::memory_order_relaxed);
    test.playGain.store(gain, std::memory_order_relaxed);
    resetMonitor();
    test.playing.store(which, std::memory_order_release);
}

// Pull from a ring with a small safety cushion and bounded latency.
bool AudioCore::readRing(Ring &ring, bool &primed, float *out, uint32_t n) {
    const size_t avail = ring.available();
    if (!primed) {
        if (avail < size_t(n) * 2) {
            std::fill(out, out + n, 0.0f);
            return true; // still filling up: not a dropout
        }
        primed = true;
    }
    if (avail > size_t(n) * 4 + 1024) ring.skip(avail - size_t(n) * 2); // too much queued: catch up
    const size_t got = ring.read(out, n);
    if (got < n) {
        std::fill(out + got, out + n, 0.0f);
        primed = false;
        return false;
    }
    return true;
}

void AudioCore::readVirtualMic(float *out, uint32_t n) {
    if (!readRing(vmicRing, vmicPrimed, out, n)) stats.vmicUnderruns.fetch_add(1, std::memory_order_relaxed);
}

void AudioCore::readMonitor(float *out, uint32_t n) {
    if (const int which = test.playing.load(std::memory_order_acquire)) {
        const std::vector<float> &src = which == 1 ? test.raw : test.processed;
        const float g = test.playGain.load(std::memory_order_relaxed);
        size_t p = test.playPos.load(std::memory_order_relaxed);
        uint32_t i = 0;
        for (; i < n && p < test.playLen; ++i, ++p) out[i] = std::clamp(src[p] * g, -1.0f, 1.0f);
        std::fill(out + i, out + n, 0.0f);
        test.playPos.store(p, std::memory_order_relaxed);
        if (p >= test.playLen) test.playing.store(0, std::memory_order_release);
        return;
    }
    if (!readRing(monitorRing, monitorPrimed, out, n)) stats.monitorUnderruns.fetch_add(1, std::memory_order_relaxed);
}

} // namespace mf
