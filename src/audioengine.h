#pragma once

#include "dsp.h"
#include "plugins/plugins.h"

#include <atomic>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

struct pw_thread_loop;
struct pw_context;
struct pw_core;
struct pw_registry;
struct pw_stream;
struct spa_hook;
struct spa_dict;

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

struct SourceInfo {
    uint32_t id;
    std::string name;
    std::string description;
};

class AudioEngine {
public:
    // Node names. A test instance (MICFORGE_TEST) uses a prefix so it never clashes with the real one.
    static inline std::string prefix = "micforge";
    static inline std::string micDescription = "MicForge (processed mic)";
    static std::string virtualMicName() { return prefix + "_mic"; }
    static constexpr int kRate = 48000;

    AudioEngine();
    ~AudioEngine();

    bool start();
    void setCaptureTarget(const std::string &nodeName);
    void setMonitor(bool on);

    Params params;
    Meters meters;
    Recorder recorder{45}; // tap for the voice-separation setup
    PluginRack rack;       // third-party VST3 / LV2 plugins

    // Called from the PipeWire thread; the receiver must marshal to its own thread.
    std::function<void(std::vector<SourceInfo>)> onSourcesChanged;
    std::function<void()> onStateChanged;

    // Status snapshot (safe to read from any thread).
    std::atomic<bool> captureRunning{false};
    std::atomic<bool> virtualMicInUse{false};
    std::atomic<bool> connected{false};

    // --- internal, used by the C callbacks ---
    void processCapture();
    void processVirtualMic();
    void processMonitor();
    void registryGlobal(uint32_t id, const char *type, const spa_dict *props);
    void registryRemove(uint32_t id);
    void captureState(int state);
    void virtualMicState(int state);

private:
    void createCapture();
    void destroyCapture();
    void createVirtualMic();
    void createMonitor();
    void destroyMonitor();
    void publishSources();
    void readRing(Ring &ring, bool &primed, float *out, uint32_t n);

    pw_thread_loop *loop = nullptr;
    pw_context *context = nullptr;
    pw_core *core = nullptr;
    pw_registry *registry = nullptr;
    pw_stream *capture = nullptr;
    pw_stream *vmic = nullptr;
    pw_stream *monitor = nullptr;
    std::unique_ptr<spa_hook> registryHook, captureHook, vmicHook, monitorHook;

    std::string targetName;
    bool targetMissing = false;
    std::map<uint32_t, SourceInfo> sources;
    std::map<uint32_t, uint32_t> links; // link id -> output node id (to detect apps using the virtual mic)
    void updateInUse();

    VoiceChain chain{float(kRate)};
    std::vector<float> scratch;
    Ring vmicRing, monitorRing;
    bool vmicPrimed = false, monitorPrimed = false;
    std::atomic<bool> monitorOn{false};
};

} // namespace mf
