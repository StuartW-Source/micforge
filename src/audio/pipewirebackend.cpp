#include "audio/pipewirebackend.h"

#include <pipewire/extensions/metadata.h>

#include <pipewire/pipewire.h>
#include <spa/param/audio/format-utils.h>
#include <spa/utils/result.h>

#include <cerrno>
#include <cstring>

namespace mf {

// ---------------------------------------------------------------- C callbacks

namespace {

void onCaptureProcess(void *d) { static_cast<PipeWireBackend *>(d)->processCapture(); }
void onVmicProcess(void *d) { static_cast<PipeWireBackend *>(d)->processVirtualMic(); }
void onMonitorProcess(void *d) { static_cast<PipeWireBackend *>(d)->processMonitor(); }

void onCaptureState(void *d, pw_stream_state, pw_stream_state state, const char *) {
    static_cast<PipeWireBackend *>(d)->captureState(state);
}
void onVmicState(void *d, pw_stream_state, pw_stream_state state, const char *) {
    static_cast<PipeWireBackend *>(d)->virtualMicState(state);
}

const pw_stream_events captureEvents = [] {
    pw_stream_events e{};
    e.version = PW_VERSION_STREAM_EVENTS;
    e.state_changed = onCaptureState;
    e.process = onCaptureProcess;
    return e;
}();

const pw_stream_events vmicEvents = [] {
    pw_stream_events e{};
    e.version = PW_VERSION_STREAM_EVENTS;
    e.state_changed = onVmicState;
    e.process = onVmicProcess;
    return e;
}();

const pw_stream_events monitorEvents = [] {
    pw_stream_events e{};
    e.version = PW_VERSION_STREAM_EVENTS;
    e.process = onMonitorProcess;
    return e;
}();

void onRegistryGlobal(void *d, uint32_t id, uint32_t, const char *type, uint32_t, const spa_dict *props) {
    static_cast<PipeWireBackend *>(d)->registryGlobal(id, type, props);
}
void onRegistryRemove(void *d, uint32_t id) { static_cast<PipeWireBackend *>(d)->registryRemove(id); }

// PipeWire reports a lost connection (daemon restarted / crashed) as an error on the core object.
void onCoreError(void *d, uint32_t id, int, int res, const char *message) {
    if (id == PW_ID_CORE && res == -EPIPE) static_cast<PipeWireBackend *>(d)->connectionLost(message);
}

const pw_core_events coreEvents = [] {
    pw_core_events e{};
    e.version = PW_VERSION_CORE_EVENTS;
    e.error = onCoreError;
    return e;
}();

// "default" metadata carries the system's default microphone (key default.audio.source).
int onMetadataProperty(void *d, uint32_t subject, const char *key, const char *, const char *value) {
    if (subject == PW_ID_CORE) static_cast<PipeWireBackend *>(d)->metadataProperty(key, value);
    return 0;
}

const pw_metadata_events metadataEvents = [] {
    pw_metadata_events e{};
    e.version = PW_VERSION_METADATA_EVENTS;
    e.property = onMetadataProperty;
    return e;
}();

const pw_registry_events registryEvents = [] {
    pw_registry_events e{};
    e.version = PW_VERSION_REGISTRY_EVENTS;
    e.global = onRegistryGlobal;
    e.global_remove = onRegistryRemove;
    return e;
}();

const spa_pod *buildFormat(spa_pod_builder *b, uint32_t channels) {
    spa_audio_info_raw info{};
    info.format = SPA_AUDIO_FORMAT_F32;
    info.rate = AudioCore::kRate;
    info.channels = channels;
    if (channels == 1) {
        info.position[0] = SPA_AUDIO_CHANNEL_MONO;
    } else {
        info.position[0] = SPA_AUDIO_CHANNEL_FL;
        info.position[1] = SPA_AUDIO_CHANNEL_FR;
    }
    return spa_format_audio_raw_build(b, SPA_PARAM_EnumFormat, &info);
}

} // namespace

// ---------------------------------------------------------------- lifecycle

PipeWireBackend::PipeWireBackend(AudioCore &audioCore, const BackendConfig &config)
    : m_core(audioCore), m_config(config) {}

std::unique_ptr<IAudioBackend> createAudioBackend(AudioCore &core, const BackendConfig &config) {
    return std::make_unique<PipeWireBackend>(core, config);
}

PipeWireBackend::~PipeWireBackend() {
    if (!loop) return;
    pw_thread_loop_lock(loop);
    teardown();
    pw_thread_loop_unlock(loop);
    pw_thread_loop_stop(loop);
    pw_thread_loop_destroy(loop);
}

// Tear down everything that belongs to one PipeWire connection. Call with the loop locked.
void PipeWireBackend::teardown() {
    destroyMonitor();
    destroyCapture();
    if (vmic) {
        spa_hook_remove(vmicHook.get());
        pw_stream_destroy(vmic);
        vmic = nullptr;
    }
    if (metadata) {
        spa_hook_remove(metadataHook.get());
        pw_proxy_destroy(reinterpret_cast<pw_proxy *>(metadata));
        metadata = nullptr;
    }
    if (registry) {
        spa_hook_remove(registryHook.get());
        pw_proxy_destroy(reinterpret_cast<pw_proxy *>(registry));
        registry = nullptr;
    }
    if (core) {
        spa_hook_remove(coreHook.get());
        pw_core_disconnect(core);
        core = nullptr;
    }
    if (context) {
        pw_context_destroy(context);
        context = nullptr;
    }
    sources.clear();
    links.clear();
    m_connected = false;
    m_captureRunning = false;
    m_virtualMicInUse = false;
}

// Connect to PipeWire and create the streams. Safe to call again after a failure or a lost
// connection: whatever is left of the previous connection is torn down first.
bool PipeWireBackend::start() {
    if (!loop) {
        loop = pw_thread_loop_new("micforge", nullptr);
        if (!loop) {
            setError("couldn't create the audio thread");
            return false;
        }
        if (pw_thread_loop_start(loop) < 0) {
            pw_thread_loop_destroy(loop);
            loop = nullptr;
            setError("couldn't start the audio thread");
            return false;
        }
    }
    pw_thread_loop_lock(loop);
    teardown();
    context = pw_context_new(pw_thread_loop_get_loop(loop), nullptr, 0);
    if (context) core = pw_context_connect(context, nullptr, 0);
    if (!core) {
        setError("PipeWire isn't running (or isn't reachable yet)");
        teardown();
        pw_thread_loop_unlock(loop);
        if (onStateChanged) onStateChanged();
        return false;
    }
    coreHook = std::make_unique<spa_hook>();
    std::memset(coreHook.get(), 0, sizeof(spa_hook));
    pw_core_add_listener(core, coreHook.get(), &coreEvents, this);
    m_connected = true;
    setError("");

    registryHook = std::make_unique<spa_hook>();
    std::memset(registryHook.get(), 0, sizeof(spa_hook));
    registry = pw_core_get_registry(core, PW_VERSION_REGISTRY, 0);
    pw_registry_add_listener(registry, registryHook.get(), &registryEvents, this);

    m_core.resetOutputs();
    m_core.resetChain();
    createVirtualMic();
    if (!targetName.empty()) createCapture();
    if (m_core.monitorEnabled()) createMonitor();
    pw_thread_loop_unlock(loop);
    if (onStateChanged) onStateChanged();
    return true;
}

void PipeWireBackend::setError(const std::string &e) {
    std::lock_guard<std::mutex> l(m_mutex);
    m_error = e;
}

std::string PipeWireBackend::error() const {
    std::lock_guard<std::mutex> l(m_mutex);
    return m_error;
}

std::string PipeWireBackend::defaultInputDevice() const {
    std::lock_guard<std::mutex> l(m_mutex);
    return m_defaultInput;
}

// Value looks like {"name":"alsa_input.usb-..."}.
void PipeWireBackend::metadataProperty(const char *key, const char *value) {
    if (!key || std::strcmp(key, "default.audio.source") != 0) return;
    std::string name;
    if (value) {
        const std::string v(value);
        const auto k = v.find("\"name\"");
        const auto q1 = k == std::string::npos ? k : v.find('"', v.find(':', k) + 1);
        const auto q2 = q1 == std::string::npos ? q1 : v.find('"', q1 + 1);
        if (q2 != std::string::npos) name = v.substr(q1 + 1, q2 - q1 - 1);
    }
    {
        std::lock_guard<std::mutex> l(m_mutex);
        if (name == m_defaultInput) return;
        m_defaultInput = name;
    }
    if (onDefaultInputChanged) onDefaultInputChanged();
}

// Called on the PipeWire thread when the daemon goes away. Only flags the state; the owner
// reconnects from its own thread by calling start() again (never tear down inside a callback).
void PipeWireBackend::connectionLost(const char *message) {
    if (!m_connected.exchange(false)) return;
    setError(std::string("lost the connection to PipeWire") + (message ? std::string(" (") + message + ")" : ""));
    m_captureRunning = false;
    m_virtualMicInUse = false;
    if (onConnectionLost) onConnectionLost();
    if (onStateChanged) onStateChanged();
}

void PipeWireBackend::setInputDevice(const std::string &nodeName) {
    if (!loop || !core) {
        targetName = nodeName;
        return;
    }
    pw_thread_loop_lock(loop);
    targetName = nodeName;
    destroyCapture();
    if (!targetName.empty()) createCapture();
    pw_thread_loop_unlock(loop);
}

std::string PipeWireBackend::version() const {
    return std::string("libpipewire ") + pw_get_library_version();
}

void PipeWireBackend::setMonitor(bool on) {
    m_core.setMonitorEnabled(on);
    if (!loop || !core) return;
    pw_thread_loop_lock(loop);
    if (on && !monitor) createMonitor();
    if (!on && monitor) destroyMonitor();
    pw_thread_loop_unlock(loop);
}

// ---------------------------------------------------------------- streams

void PipeWireBackend::createCapture() {
    pw_properties *props = pw_properties_new(
        PW_KEY_MEDIA_TYPE, "Audio",
        PW_KEY_MEDIA_CATEGORY, "Capture",
        PW_KEY_APP_NAME, "MicForge",
        PW_KEY_NODE_NAME, (m_config.idPrefix + "_input").c_str(),
        PW_KEY_NODE_DESCRIPTION, "MicForge input",
        PW_KEY_NODE_LATENCY, "256/48000",
        PW_KEY_NODE_GROUP, (m_config.idPrefix + ".group").c_str(),
        PW_KEY_TARGET_OBJECT, targetName.c_str(),
        PW_KEY_NODE_DONT_RECONNECT, "true", // never fall back to the default mic (could be ourselves)
        PW_KEY_STREAM_DONT_REMIX, "true",   // keep left/right separate so we can pick the mic channel
        nullptr);

    capture = pw_stream_new(core, "MicForge input", props);
    captureHook = std::make_unique<spa_hook>();
    std::memset(captureHook.get(), 0, sizeof(spa_hook));
    pw_stream_add_listener(capture, captureHook.get(), &captureEvents, this);

    uint8_t buffer[1024];
    spa_pod_builder b = SPA_POD_BUILDER_INIT(buffer, sizeof(buffer));
    const spa_pod *params[1] = {buildFormat(&b, 2)};
    m_core.resetChain();
    pw_stream_connect(capture, PW_DIRECTION_INPUT, PW_ID_ANY,
                      pw_stream_flags(PW_STREAM_FLAG_AUTOCONNECT | PW_STREAM_FLAG_MAP_BUFFERS |
                                      PW_STREAM_FLAG_RT_PROCESS),
                      params, 1);
    targetMissing = false;
}

void PipeWireBackend::destroyCapture() {
    if (!capture) return;
    spa_hook_remove(captureHook.get());
    pw_stream_destroy(capture);
    capture = nullptr;
    m_captureRunning = false;
    if (onStateChanged) onStateChanged();
}

void PipeWireBackend::createVirtualMic() {
    pw_properties *props = pw_properties_new(
        PW_KEY_MEDIA_TYPE, "Audio",
        PW_KEY_MEDIA_CLASS, "Audio/Source",
        PW_KEY_APP_NAME, "MicForge",
        PW_KEY_NODE_NAME, virtualMicName().c_str(),
        PW_KEY_NODE_DESCRIPTION, m_config.virtualMicDescription.c_str(),
        PW_KEY_NODE_NICK, "MicForge",
        PW_KEY_NODE_LATENCY, "256/48000",
        PW_KEY_NODE_GROUP, (m_config.idPrefix + ".group").c_str(),
        "device.icon-name", "audio-input-microphone",
        nullptr);

    vmic = pw_stream_new(core, "MicForge", props);
    vmicHook = std::make_unique<spa_hook>();
    std::memset(vmicHook.get(), 0, sizeof(spa_hook));
    pw_stream_add_listener(vmic, vmicHook.get(), &vmicEvents, this);

    uint8_t buffer[1024];
    spa_pod_builder b = SPA_POD_BUILDER_INIT(buffer, sizeof(buffer));
    const spa_pod *params[1] = {buildFormat(&b, 1)};
    pw_stream_connect(vmic, PW_DIRECTION_OUTPUT, PW_ID_ANY,
                      pw_stream_flags(PW_STREAM_FLAG_MAP_BUFFERS | PW_STREAM_FLAG_RT_PROCESS), params, 1);
}

void PipeWireBackend::createMonitor() {
    pw_properties *props = pw_properties_new(
        PW_KEY_MEDIA_TYPE, "Audio",
        PW_KEY_MEDIA_CATEGORY, "Playback",
        PW_KEY_MEDIA_ROLE, "Production",
        PW_KEY_APP_NAME, "MicForge",
        PW_KEY_NODE_NAME, (m_config.idPrefix + "_monitor").c_str(),
        PW_KEY_NODE_DESCRIPTION, "MicForge monitor (listen to yourself)",
        PW_KEY_NODE_LATENCY, "256/48000",
        nullptr);

    monitor = pw_stream_new(core, "MicForge monitor", props);
    monitorHook = std::make_unique<spa_hook>();
    std::memset(monitorHook.get(), 0, sizeof(spa_hook));
    pw_stream_add_listener(monitor, monitorHook.get(), &monitorEvents, this);

    uint8_t buffer[1024];
    spa_pod_builder b = SPA_POD_BUILDER_INIT(buffer, sizeof(buffer));
    const spa_pod *params[1] = {buildFormat(&b, 1)};
    m_core.resetMonitor();
    pw_stream_connect(monitor, PW_DIRECTION_OUTPUT, PW_ID_ANY,
                      pw_stream_flags(PW_STREAM_FLAG_AUTOCONNECT | PW_STREAM_FLAG_MAP_BUFFERS |
                                      PW_STREAM_FLAG_RT_PROCESS),
                      params, 1);
}

void PipeWireBackend::destroyMonitor() {
    if (!monitor) return;
    spa_hook_remove(monitorHook.get());
    pw_stream_destroy(monitor);
    monitor = nullptr;
}

// ---------------------------------------------------------------- realtime

void PipeWireBackend::processCapture() {
    pw_buffer *b = pw_stream_dequeue_buffer(capture);
    if (!b) return;
    spa_data &d = b->buffer->datas[0];
    if (d.data && d.chunk) {
        const uint32_t offs = std::min(d.chunk->offset, d.maxsize);
        const uint32_t size = std::min(d.chunk->size, d.maxsize - offs);
        const float *in = static_cast<const float *>(SPA_PTROFF(d.data, offs, void));
        m_core.processInput(in, 2, size / uint32_t(sizeof(float) * 2));
    }
    pw_stream_queue_buffer(capture, b);
    // Graph rate and input latency for Diagnostics (pw_stream_get_time_n is real-time safe).
    if ((m_core.stats.blocks.load(std::memory_order_relaxed) & 63) == 1) {
        pw_time t{};
        if (pw_stream_get_time_n(capture, &t, sizeof(t)) == 0 && t.rate.denom > 0) {
            m_core.stats.graphRate.store(t.rate.denom / std::max<uint32_t>(1, t.rate.num), std::memory_order_relaxed);
            m_core.stats.inputLatencyMs.store(float(1000.0 * double(t.delay) * t.rate.num / t.rate.denom), std::memory_order_relaxed);
        }
    }
}

namespace {
template <typename Fill> void fillOutput(pw_stream *stream, Fill fill) {
    pw_buffer *b = pw_stream_dequeue_buffer(stream);
    if (!b) return;
    spa_data &d = b->buffer->datas[0];
    if (d.data) {
        uint32_t n = d.maxsize / sizeof(float);
        if (b->requested) n = std::min<uint32_t>(n, uint32_t(b->requested));
        fill(static_cast<float *>(d.data), n);
        d.chunk->offset = 0;
        d.chunk->stride = sizeof(float);
        d.chunk->size = n * sizeof(float);
    }
    pw_stream_queue_buffer(stream, b);
}
} // namespace

void PipeWireBackend::processVirtualMic() {
    fillOutput(vmic, [this](float *o, uint32_t n) { m_core.readVirtualMic(o, n); });
    if ((++m_vmicCycles & 63) == 1) {
        pw_time t{};
        if (pw_stream_get_time_n(vmic, &t, sizeof(t)) == 0 && t.rate.denom > 0)
            m_core.stats.outputLatencyMs.store(float(1000.0 * double(t.delay) * t.rate.num / t.rate.denom), std::memory_order_relaxed);
    }
}
void PipeWireBackend::processMonitor() { fillOutput(monitor, [this](float *o, uint32_t n) { m_core.readMonitor(o, n); }); }

// ---------------------------------------------------------------- state / registry

void PipeWireBackend::captureState(int state) {
    m_captureRunning = state == PW_STREAM_STATE_STREAMING;
    if (onStateChanged) onStateChanged();
}

void PipeWireBackend::virtualMicState(int) { updateInUse(); }

// "In use" = at least one link leaves the virtual mic node (an app is recording from it).
void PipeWireBackend::updateInUse() {
    const uint32_t me = vmic ? pw_stream_get_node_id(vmic) : SPA_ID_INVALID;
    bool used = false;
    for (auto &[id, outNode] : links)
        if (outNode == me) used = true;
    if (used != m_virtualMicInUse.exchange(used) && onStateChanged) onStateChanged();
}

void PipeWireBackend::registryGlobal(uint32_t id, const char *type, const spa_dict *props) {
    if (props && std::strcmp(type, PW_TYPE_INTERFACE_Link) == 0) {
        if (const char *out = spa_dict_lookup(props, PW_KEY_LINK_OUTPUT_NODE)) {
            links[id] = uint32_t(std::strtoul(out, nullptr, 10));
            updateInUse();
        }
        return;
    }
    if (props && !metadata && std::strcmp(type, PW_TYPE_INTERFACE_Metadata) == 0) {
        const char *mname = spa_dict_lookup(props, "metadata.name");
        if (mname && std::strcmp(mname, "default") == 0) {
            metadata = static_cast<pw_metadata *>(pw_registry_bind(registry, id, PW_TYPE_INTERFACE_Metadata, PW_VERSION_METADATA, 0));
            metadataHook = std::make_unique<spa_hook>();
            std::memset(metadataHook.get(), 0, sizeof(spa_hook));
            pw_metadata_add_listener(metadata, metadataHook.get(), &metadataEvents, this);
        }
        return;
    }
    if (!props || std::strcmp(type, PW_TYPE_INTERFACE_Node) != 0) return;
    const char *cls = spa_dict_lookup(props, PW_KEY_MEDIA_CLASS);
    const char *name = spa_dict_lookup(props, PW_KEY_NODE_NAME);
    if (!cls || !name || std::strncmp(cls, "Audio/Source", 12) != 0) return;
    if (virtualMicName() == name || std::strncmp(name, "micforge", 8) == 0) return; // never offer ourselves as input

    const char *desc = spa_dict_lookup(props, PW_KEY_NODE_DESCRIPTION);
    if (!desc) desc = spa_dict_lookup(props, PW_KEY_NODE_NICK);
    sources[id] = InputDevice{name, desc ? desc : name};
    publishSources();

    // Our mic came back (e.g. USB interface re-plugged): reconnect to it.
    if (targetMissing && targetName == name) {
        destroyCapture();
        createCapture();
    }
}

void PipeWireBackend::registryRemove(uint32_t id) {
    if (links.erase(id)) {
        updateInUse();
        return;
    }
    auto it = sources.find(id);
    if (it == sources.end()) return;
    if (it->second.id == targetName) targetMissing = true;
    sources.erase(it);
    publishSources();
}

void PipeWireBackend::publishSources() {
    if (!onDevicesChanged) return;
    std::vector<InputDevice> list;
    for (auto &[id, s] : sources) list.push_back(s);
    onDevicesChanged(std::move(list));
}

} // namespace mf
