// Linux audio backend: PipeWire capture stream (chosen microphone), virtual microphone
// (Audio/Source node), optional monitor playback, device list, default-device tracking and
// reconnection. All processing happens in the shared AudioCore.
#pragma once

#include "audio/audiobackend.h"
#include "audio/audiocore.h"

#include <atomic>
#include <map>
#include <memory>
#include <mutex>
#include <string>

struct pw_thread_loop;
struct pw_context;
struct pw_core;
struct pw_registry;
struct pw_stream;
struct pw_metadata;
struct spa_hook;
struct spa_dict;

namespace mf {

class PipeWireBackend final : public IAudioBackend {
public:
    PipeWireBackend(AudioCore &core, const BackendConfig &config);
    ~PipeWireBackend() override;

    std::string name() const override { return "PipeWire"; }
    bool start() override;
    std::string error() const override;
    bool connected() const override { return m_connected; }
    bool inputRunning() const override { return m_captureRunning; }
    bool virtualMicInUse() const override { return m_virtualMicInUse; }
    void setInputDevice(const std::string &id) override;
    void setMonitor(bool on) override;
    std::string version() const override;
    std::string defaultInputDevice() const override;

    // --- internal, used by the C callbacks ---
    void processCapture();
    void processVirtualMic();
    void processMonitor();
    void registryGlobal(uint32_t id, const char *type, const spa_dict *props);
    void registryRemove(uint32_t id);
    void captureState(int state);
    void virtualMicState(int state);
    void connectionLost(const char *message);
    void metadataProperty(const char *key, const char *value);

private:
    void teardown();
    void createCapture();
    void destroyCapture();
    void createVirtualMic();
    void createMonitor();
    void destroyMonitor();
    void publishSources();
    void updateInUse();
    void setError(const std::string &e);
    std::string virtualMicName() const { return m_config.idPrefix + "_mic"; }

    AudioCore &m_core;
    BackendConfig m_config;

    pw_thread_loop *loop = nullptr;
    pw_context *context = nullptr;
    pw_core *core = nullptr;
    pw_registry *registry = nullptr;
    pw_metadata *metadata = nullptr;
    pw_stream *capture = nullptr;
    pw_stream *vmic = nullptr;
    pw_stream *monitor = nullptr;
    std::unique_ptr<spa_hook> coreHook, registryHook, metadataHook, captureHook, vmicHook, monitorHook;

    std::atomic<bool> m_connected{false}, m_captureRunning{false}, m_virtualMicInUse{false};
    uint64_t m_vmicCycles = 0; // audio thread only
    mutable std::mutex m_mutex; // guards m_error and m_defaultInput (PipeWire thread <-> UI; never the RT callbacks)
    std::string m_error, m_defaultInput;

    std::string targetName;
    bool targetMissing = false;
    std::map<uint32_t, InputDevice> sources;
    std::map<uint32_t, uint32_t> links; // link id -> output node id (to detect apps using the virtual mic)
};

} // namespace mf
