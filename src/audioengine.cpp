#include "audioengine.h"

#include <pipewire/pipewire.h>
#include <spa/param/audio/format-utils.h>
#include <spa/utils/result.h>

#include <cstring>

namespace mf {

// ---------------------------------------------------------------- Ring

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

// ---------------------------------------------------------------- C callbacks

namespace {

void onCaptureProcess(void *d) { static_cast<AudioEngine *>(d)->processCapture(); }
void onVmicProcess(void *d) { static_cast<AudioEngine *>(d)->processVirtualMic(); }
void onMonitorProcess(void *d) { static_cast<AudioEngine *>(d)->processMonitor(); }

void onCaptureState(void *d, pw_stream_state, pw_stream_state state, const char *) {
    static_cast<AudioEngine *>(d)->captureState(state);
}
void onVmicState(void *d, pw_stream_state, pw_stream_state state, const char *) {
    static_cast<AudioEngine *>(d)->virtualMicState(state);
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
    static_cast<AudioEngine *>(d)->registryGlobal(id, type, props);
}
void onRegistryRemove(void *d, uint32_t id) { static_cast<AudioEngine *>(d)->registryRemove(id); }

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
    info.rate = AudioEngine::kRate;
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

AudioEngine::AudioEngine() : scratch(16384), vmicRing(16384), monitorRing(16384) {
    chain.recorder = &recorder;
    chain.blockHook = &PluginRack::hook;
    chain.blockCtx = &rack;
}

AudioEngine::~AudioEngine() {
    if (!loop) return;
    pw_thread_loop_lock(loop);
    destroyMonitor();
    destroyCapture();
    if (vmic) pw_stream_destroy(vmic);
    vmic = nullptr;
    if (registry) {
        spa_hook_remove(registryHook.get());
        pw_proxy_destroy(reinterpret_cast<pw_proxy *>(registry));
    }
    if (core) pw_core_disconnect(core);
    pw_thread_loop_unlock(loop);
    pw_thread_loop_stop(loop);
    if (context) pw_context_destroy(context);
    pw_thread_loop_destroy(loop);
}

bool AudioEngine::start() {
    loop = pw_thread_loop_new("micforge", nullptr);
    if (!loop) return false;
    context = pw_context_new(pw_thread_loop_get_loop(loop), nullptr, 0);
    if (!context) return false;
    if (pw_thread_loop_start(loop) < 0) return false;

    pw_thread_loop_lock(loop);
    core = pw_context_connect(context, nullptr, 0);
    if (!core) {
        pw_thread_loop_unlock(loop);
        return false;
    }
    connected = true;

    registryHook = std::make_unique<spa_hook>();
    std::memset(registryHook.get(), 0, sizeof(spa_hook));
    registry = pw_core_get_registry(core, PW_VERSION_REGISTRY, 0);
    pw_registry_add_listener(registry, registryHook.get(), &registryEvents, this);

    createVirtualMic();
    if (!targetName.empty()) createCapture();
    pw_thread_loop_unlock(loop);
    return true;
}

void AudioEngine::setCaptureTarget(const std::string &nodeName) {
    if (!loop) {
        targetName = nodeName;
        return;
    }
    pw_thread_loop_lock(loop);
    targetName = nodeName;
    destroyCapture();
    if (!targetName.empty()) createCapture();
    pw_thread_loop_unlock(loop);
}

void AudioEngine::setMonitor(bool on) {
    if (!loop) return;
    pw_thread_loop_lock(loop);
    monitorOn = on;
    if (on && !monitor) createMonitor();
    if (!on && monitor) destroyMonitor();
    pw_thread_loop_unlock(loop);
}

// ---------------------------------------------------------------- streams

void AudioEngine::createCapture() {
    pw_properties *props = pw_properties_new(
        PW_KEY_MEDIA_TYPE, "Audio",
        PW_KEY_MEDIA_CATEGORY, "Capture",
        PW_KEY_APP_NAME, "MicForge",
        PW_KEY_NODE_NAME, (prefix + "_input").c_str(),
        PW_KEY_NODE_DESCRIPTION, "MicForge input",
        PW_KEY_NODE_LATENCY, "256/48000",
        PW_KEY_NODE_GROUP, (prefix + ".group").c_str(),
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
    chain.reset();
    pw_stream_connect(capture, PW_DIRECTION_INPUT, PW_ID_ANY,
                      pw_stream_flags(PW_STREAM_FLAG_AUTOCONNECT | PW_STREAM_FLAG_MAP_BUFFERS |
                                      PW_STREAM_FLAG_RT_PROCESS),
                      params, 1);
    targetMissing = false;
}

void AudioEngine::destroyCapture() {
    if (!capture) return;
    spa_hook_remove(captureHook.get());
    pw_stream_destroy(capture);
    capture = nullptr;
    captureRunning = false;
    if (onStateChanged) onStateChanged();
}

void AudioEngine::createVirtualMic() {
    pw_properties *props = pw_properties_new(
        PW_KEY_MEDIA_TYPE, "Audio",
        PW_KEY_MEDIA_CLASS, "Audio/Source",
        PW_KEY_APP_NAME, "MicForge",
        PW_KEY_NODE_NAME, virtualMicName().c_str(),
        PW_KEY_NODE_DESCRIPTION, micDescription.c_str(),
        PW_KEY_NODE_NICK, "MicForge",
        PW_KEY_NODE_LATENCY, "256/48000",
        PW_KEY_NODE_GROUP, (prefix + ".group").c_str(),
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

void AudioEngine::createMonitor() {
    pw_properties *props = pw_properties_new(
        PW_KEY_MEDIA_TYPE, "Audio",
        PW_KEY_MEDIA_CATEGORY, "Playback",
        PW_KEY_MEDIA_ROLE, "Production",
        PW_KEY_APP_NAME, "MicForge",
        PW_KEY_NODE_NAME, (prefix + "_monitor").c_str(),
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
    monitorPrimed = false;
    pw_stream_connect(monitor, PW_DIRECTION_OUTPUT, PW_ID_ANY,
                      pw_stream_flags(PW_STREAM_FLAG_AUTOCONNECT | PW_STREAM_FLAG_MAP_BUFFERS |
                                      PW_STREAM_FLAG_RT_PROCESS),
                      params, 1);
}

void AudioEngine::destroyMonitor() {
    if (!monitor) return;
    spa_hook_remove(monitorHook.get());
    pw_stream_destroy(monitor);
    monitor = nullptr;
}

// ---------------------------------------------------------------- realtime

void AudioEngine::processCapture() {
    pw_buffer *b = pw_stream_dequeue_buffer(capture);
    if (!b) return;
    spa_data &d = b->buffer->datas[0];
    if (d.data && d.chunk) {
        const uint32_t offs = std::min(d.chunk->offset, d.maxsize);
        const uint32_t size = std::min(d.chunk->size, d.maxsize - offs);
        const float *in = static_cast<const float *>(SPA_PTROFF(d.data, offs, void));
        uint32_t frames = size / (sizeof(float) * 2);
        while (frames > 0) {
            const uint32_t n = std::min<uint32_t>(frames, scratch.size());
            chain.process(in, 2, scratch.data(), int(n), params, meters);
            vmicRing.write(scratch.data(), n);
            if (monitorOn.load(std::memory_order_relaxed)) monitorRing.write(scratch.data(), n);
            in += n * 2;
            frames -= n;
        }
    }
    pw_stream_queue_buffer(capture, b);
}

// Pull from a ring with a small safety cushion and bounded latency.
void AudioEngine::readRing(Ring &ring, bool &primed, float *out, uint32_t n) {
    size_t avail = ring.available();
    if (!primed) {
        if (avail < size_t(n) * 2) {
            std::fill(out, out + n, 0.0f);
            return;
        }
        primed = true;
    }
    if (avail > size_t(n) * 4 + 1024) { // too much queued (e.g. nobody was listening): catch up
        ring.skip(avail - size_t(n) * 2);
    }
    const size_t got = ring.read(out, n);
    if (got < n) {
        std::fill(out + got, out + n, 0.0f);
        primed = false;
    }
}

void AudioEngine::processVirtualMic() {
    pw_buffer *b = pw_stream_dequeue_buffer(vmic);
    if (!b) return;
    spa_data &d = b->buffer->datas[0];
    if (d.data) {
        uint32_t n = d.maxsize / sizeof(float);
        if (b->requested) n = std::min<uint32_t>(n, b->requested);
        readRing(vmicRing, vmicPrimed, static_cast<float *>(d.data), n);
        d.chunk->offset = 0;
        d.chunk->stride = sizeof(float);
        d.chunk->size = n * sizeof(float);
    }
    pw_stream_queue_buffer(vmic, b);
}

void AudioEngine::processMonitor() {
    pw_buffer *b = pw_stream_dequeue_buffer(monitor);
    if (!b) return;
    spa_data &d = b->buffer->datas[0];
    if (d.data) {
        uint32_t n = d.maxsize / sizeof(float);
        if (b->requested) n = std::min<uint32_t>(n, b->requested);
        readRing(monitorRing, monitorPrimed, static_cast<float *>(d.data), n);
        d.chunk->offset = 0;
        d.chunk->stride = sizeof(float);
        d.chunk->size = n * sizeof(float);
    }
    pw_stream_queue_buffer(monitor, b);
}

// ---------------------------------------------------------------- state / registry

void AudioEngine::captureState(int state) {
    captureRunning = state == PW_STREAM_STATE_STREAMING;
    if (onStateChanged) onStateChanged();
}

void AudioEngine::virtualMicState(int) { updateInUse(); }

// "In use" = at least one link leaves the virtual mic node (an app is recording from it).
void AudioEngine::updateInUse() {
    const uint32_t me = vmic ? pw_stream_get_node_id(vmic) : SPA_ID_INVALID;
    bool used = false;
    for (auto &[id, outNode] : links)
        if (outNode == me) used = true;
    if (used != virtualMicInUse.exchange(used) && onStateChanged) onStateChanged();
}

void AudioEngine::registryGlobal(uint32_t id, const char *type, const spa_dict *props) {
    if (props && std::strcmp(type, PW_TYPE_INTERFACE_Link) == 0) {
        if (const char *out = spa_dict_lookup(props, PW_KEY_LINK_OUTPUT_NODE)) {
            links[id] = uint32_t(std::strtoul(out, nullptr, 10));
            updateInUse();
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
    sources[id] = SourceInfo{id, name, desc ? desc : name};
    publishSources();

    // Our mic came back (e.g. USB interface re-plugged): reconnect to it.
    if (targetMissing && targetName == name) {
        destroyCapture();
        createCapture();
    }
}

void AudioEngine::registryRemove(uint32_t id) {
    if (links.erase(id)) {
        updateInUse();
        return;
    }
    auto it = sources.find(id);
    if (it == sources.end()) return;
    if (it->second.name == targetName) targetMissing = true;
    sources.erase(it);
    publishSources();
}

void AudioEngine::publishSources() {
    if (!onSourcesChanged) return;
    std::vector<SourceInfo> list;
    for (auto &[id, s] : sources) list.push_back(s);
    onSourcesChanged(std::move(list));
}

} // namespace mf
