// Operating-system audio backend: connects the platform's audio system to the shared AudioCore.
// Linux: PipeWireBackend. Windows (planned): WASAPI capture/render plus a virtual microphone.
// See docs/CROSS_PLATFORM.md.
#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace mf {

class AudioCore;

struct InputDevice {
    std::string id;   // stable, persisted identifier (PipeWire node name, Windows endpoint ID)
    std::string name; // human-readable
};

struct BackendConfig {
    std::string idPrefix = "micforge";                         // node/endpoint names (tests use "micforge_test")
    std::string virtualMicDescription = "MicForge (processed mic)";
};

class IAudioBackend {
public:
    virtual ~IAudioBackend() = default;

    virtual std::string name() const = 0;   // e.g. "PipeWire"
    // Connect (or reconnect) to the audio system and create the streams. False = not reachable yet;
    // the caller retries. Safe to call repeatedly.
    virtual bool start() = 0;
    virtual std::string error() const = 0;  // reason for the last failure ("" when connected)

    virtual bool connected() const = 0;
    virtual bool inputRunning() const = 0;     // the chosen microphone is delivering audio
    virtual bool virtualMicInUse() const = 0;  // an application is recording from the virtual mic

    // The microphone to process. Never silently replaced by another device if it disappears.
    virtual void setInputDevice(const std::string &id) = 0;
    virtual void setMonitor(bool on) = 0;
    virtual std::string defaultInputDevice() const = 0; // system default microphone, "" if unknown

    // Callbacks are invoked on backend threads; receivers must marshal to their own thread.
    std::function<void(std::vector<InputDevice>)> onDevicesChanged;
    std::function<void()> onStateChanged;
    std::function<void()> onConnectionLost;      // owner should call start() again (from its own thread)
    std::function<void()> onDefaultInputChanged;
};

// Creates the backend for this platform.
std::unique_ptr<IAudioBackend> createAudioBackend(AudioCore &core, const BackendConfig &config);

} // namespace mf
