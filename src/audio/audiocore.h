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

// Health counters written by the audio thread, read by Diagnostics. All lock-free.
struct EngineStats {
    std::atomic<uint64_t> blocks{0};            // capture blocks processed
    std::atomic<uint32_t> blockFrames{0};       // size of the last capture block (≈ the graph quantum)
    std::atomic<float> procUsAvg{0};            // processing time per block, µs (moving average)
    std::atomic<float> procUsMax{0};            // max since the UI last read it (UI resets it)
    std::atomic<float> loadMax{0};              // max processing time / block duration since last read
    std::atomic<uint64_t> lateCallbacks{0};     // capture blocks that arrived > 2 blocks late (dropouts)
    std::atomic<uint64_t> vmicUnderruns{0};     // an app read the virtual mic faster than audio arrived
    std::atomic<uint64_t> monitorUnderruns{0};
    std::atomic<uint64_t> inputClips{0};        // input samples at digital full scale (before input gain)
    std::atomic<int64_t> lastClipNs{0};         // steady-clock time of the last clip
    // Filled in by the backend (0 = unknown).
    std::atomic<uint32_t> graphRate{0};         // device/graph sample rate, Hz
    std::atomic<float> inputLatencyMs{0};       // device → MicForge
    std::atomic<float> outputLatencyMs{0};      // MicForge → apps (virtual mic stream)

    int64_t lastBlockNs = 0;                    // audio thread only
    void reset() {
        blocks = 0; blockFrames = 0; procUsAvg = 0; procUsMax = 0; loadMax = 0; lateCallbacks = 0;
        vmicUnderruns = 0; monitorUnderruns = 0; inputClips = 0; lastClipNs = 0; lastBlockNs = 0;
    }
};

// Mic test: simultaneous raw (channel + input gain only) and processed recordings, and playback of
// either one to the monitor output only (never to the virtual microphone).
struct MicTest {
    std::vector<float> raw, processed;          // allocated by the UI thread while idle
    std::atomic<size_t> pos{0};                 // frames recorded
    std::atomic<bool> recording{false};
    // Playback (read by the monitor stream).
    std::atomic<int> playing{0};                // 0 = none, 1 = raw, 2 = processed
    std::atomic<size_t> playPos{0};
    std::atomic<float> playGain{1};
    size_t playLen = 0;                         // set before `playing`
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
    EngineStats stats;
    MicTest test;

    // ---- real-time (audio thread)
    // Process `frames` interleaved input frames with `channels` channels (1 or 2).
    void processInput(const float *interleaved, int channels, uint32_t frames);
    // Fill `n` mono samples for the virtual microphone / the monitor (silence on underrun).
    void readVirtualMic(float *out, uint32_t n);
    void readMonitor(float *out, uint32_t n);

    // ---- control (any thread)
    // Whether the monitor output stream should exist (live listening or mic-test playback).
    void setMonitorEnabled(bool on) { monitorOn.store(on, std::memory_order_relaxed); }
    bool monitorEnabled() const { return monitorOn.load(std::memory_order_relaxed); }
    // Whether the live processed signal is sent to the monitor ("Listen").
    void setLiveMonitor(bool on) { liveMonitor.store(on, std::memory_order_relaxed); }
    size_t virtualMicQueued() const { return vmicRing.available(); }

    // ---- mic test (UI thread)
    void prepareTest(size_t frames);               // allocate; only while not recording or playing
    void startTestRecording();
    void stopTestRecording() { test.recording.store(false, std::memory_order_release); }
    void startPlayback(int which, float gain);     // 1 = raw, 2 = processed
    void stopPlayback() { test.playing.store(0, std::memory_order_release); }

    // ---- call only while no audio callbacks run (e.g. while (re)creating streams)
    void resetChain() { chain.reset(); stats.lastBlockNs = 0; }
    void resetOutputs() { vmicPrimed = monitorPrimed = false; }
    void resetMonitor() { monitorPrimed = false; }

private:
    static bool readRing(Ring &ring, bool &primed, float *out, uint32_t n); // false = underrun

    VoiceChain chain{float(kRate)};
    std::vector<float> scratch;
    Ring vmicRing, monitorRing;
    bool vmicPrimed = false, monitorPrimed = false;
    std::atomic<bool> monitorOn{false};
    std::atomic<bool> liveMonitor{false};
};

} // namespace mf
