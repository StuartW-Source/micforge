// Platform-independent audio core: the processing chain plus the buffers between the input and the two
// outputs (virtual microphone, monitor). An OS backend (PipeWire today, WASAPI later) only moves audio
// in and out by calling the three real-time functions below. No Qt, no platform types.
#pragma once

#include "dsp.h"
#include "plugins/plugins.h"

#include <atomic>
#include <cstdint>
#include <vector>

namespace mf {

// Lock-free single-producer / single-consumer float ring buffer.
class Ring {
public:
    explicit Ring(size_t capacityPow2 = 16384) : buf(capacityPow2), mask(capacityPow2 - 1) {}
    size_t available() const { return w.load(std::memory_order_acquire) - r.load(std::memory_order_relaxed); }
    size_t capacity() const { return buf.size(); }
    void write(const float *src, size_t n);
    size_t read(float *dst, size_t n);
    void skip(size_t n) { r.fetch_add(std::min(n, available()), std::memory_order_release); }

private:
    std::vector<float> buf;
    size_t mask;
    std::atomic<size_t> w{0}, r{0};
};

class AudioCore {
public:
    static constexpr int kRate = 48000; // processing rate; backends convert if the device differs

    AudioCore();

    // Shared state (UI <-> audio): see docs/ARCHITECTURE.md for the threading rules.
    Params params;
    Meters meters;
    Recorder recorder{45}; // tap used by the voice-separation setup
    PluginRack rack;       // third-party VST3 / LV2 plugins

    // ---- real-time (audio thread)
    // Process `frames` interleaved input frames with `channels` channels (1 or 2).
    void processInput(const float *interleaved, int channels, uint32_t frames);
    // Fill `n` mono samples for the virtual microphone / the monitor (silence on underrun).
    void readVirtualMic(float *out, uint32_t n);
    void readMonitor(float *out, uint32_t n);

    // ---- control (any thread)
    void setMonitorEnabled(bool on) { monitorOn.store(on, std::memory_order_relaxed); }
    bool monitorEnabled() const { return monitorOn.load(std::memory_order_relaxed); }

    // ---- call only while no audio callbacks run (e.g. while (re)creating streams)
    void resetChain() { chain.reset(); }
    void resetOutputs() { vmicPrimed = monitorPrimed = false; }
    void resetMonitor() { monitorPrimed = false; }

private:
    static void readRing(Ring &ring, bool &primed, float *out, uint32_t n);

    VoiceChain chain{float(kRate)};
    std::vector<float> scratch;
    Ring vmicRing, monitorRing;
    bool vmicPrimed = false, monitorPrimed = false;
    std::atomic<bool> monitorOn{false};
};

} // namespace mf
