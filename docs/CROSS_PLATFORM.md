# Cross-platform review

Goal: one codebase. Linux keeps native PipeWire; Windows gets a native backend later. Nothing here has
been implemented beyond Phase 1's small portable helpers. The interfaces below are a **proposal for Phase 2**.

## What is already platform-independent

| Area | Files | Notes |
|---|---|---|
| DSP chain and effects | `src/dsp.h`, `src/effects.h`, `src/params.h` | Header-only C++20; no Qt, no PipeWire. Uses SSE `_mm_setcsr` behind an x86 guard (works with MSVC; needs an ARM64 path for FTZ) |
| Voice-separation analysis | `src/separation.h` | Pure C++ |
| Optional-library loading | `src/platform/dynlib.h`, `src/optional/` | `dlopen` / `LoadLibrary` already both implemented |
| Plugin abstraction | `src/plugins/plugins.h` | Format-neutral `PluginInstance` / `PluginRack` |
| VST3 hosting | `src/plugins/vst3host.cpp` | SDK is cross-platform; needs `module_win32.cpp` instead of `module_linux.cpp` |
| UI | `qml/` | Qt Quick: portable. Font/DPI review needed on Windows |
| Start-up helpers | `src/startup.*` | QLockFile/QLocalServer are portable; signals and process checks already `#ifdef _WIN32`-guarded |
| Settings | QSettings | Portable (registry or INI on Windows; prefer INI for preset/backup parity) |

## Linux-specific today

| Dependency | Where | Windows equivalent / plan |
|---|---|---|
| **PipeWire** (capture, virtual source, monitor, device registry) | `src/audioengine.*` | WASAPI capture/render + device notifications; virtual mic via a virtual audio device (see WINDOWS_RESEARCH.md) |
| `pactl get-default-source` | `Controller::systemDefaultSource` | Move behind the backend (`defaultInputDevice()`); `IMMDeviceEnumerator::GetDefaultAudioEndpoint` |
| PipeWire node names as device IDs (`m_currentSource`) | controller, settings | Backend-neutral device ID (string) + display name; Windows endpoint ID |
| XDG autostart `.desktop` file | `Controller::writeAutostartEntry` | `HKCU\...\Run` value or Startup-folder shortcut |
| lilv / LV2 | `src/plugins/lv2host.cpp` | LV2 exists on Windows but is rare; keep optional, VST3 is the primary cross-platform format |
| VST3 search paths (`~/.vst3`, `/usr/lib/vst3`) | `src/plugins/plugins.cpp` | `%COMMONPROGRAMFILES%\VST3`, `%LOCALAPPDATA%\Programs\Common\VST3` |
| POSIX signals (clean quit) | `src/startup.cpp` | `WM_QUERYENDSESSION`/`WM_ENDSESSION` (Qt `commitDataRequest`) |
| `kbuildsycoca6`, `update-desktop-database`, PKGBUILD, AppStream | packaging | WiX/Inno Setup installer; MSIX optional |
| qmake | `micforge.pro` | CMake (works with MSVC, CI, and Linux) |
| Rotating log location `~/.local/state` | `QStandardPaths::StateLocation` | Resolves to `%LOCALAPPDATA%` on Windows automatically |

## Proposed seams (Phase 2, for approval)

Only abstractions the current code actually needs:

```cpp
// Audio I/O provided by the OS. PipeWireBackend wraps today's AudioEngine; WasapiBackend comes later.
class IAudioBackend {
public:
    struct Device { std::string id, name; int channels; };
    virtual bool start() = 0;                         // connect; false = retry later
    virtual void setInputDevice(const std::string &id, int channelMode) = 0;
    virtual void setMonitor(bool on) = 0;
    virtual std::string defaultInputDevice() = 0;
    virtual BackendState state() const = 0;           // disconnected / connecting / running / error
    std::function<void(std::vector<Device>)> onDevicesChanged;
    std::function<void()> onStateChanged, onConnectionLost;
    // The backend owns the RT thread and calls VoiceChain::process; the chain itself is shared.
};

class IAutostart { virtual bool enabled() = 0; virtual void setEnabled(bool, bool hidden) = 0; };
```

`VoiceChain`, `Params`, `Meters`, `Recorder` and `PluginRack` stay as they are. Each backend owns them the
way `AudioEngine` does today. A virtual-microphone interface is **not** proposed yet: on Linux it's part
of the PipeWire backend, and on Windows its shape depends on the Phase 8 research.

**Benefits:** isolates the only large Linux-only component; makes a Windows backend additive.
**Risks:** churn in the engine/controller boundary; mitigated by keeping `AudioEngine`'s behaviour and
tests (`startup_test.sh`, `reconnect_test.sh`) unchanged.
**Migration:** (1) CMake build producing identical binaries; (2) extract `IAudioBackend` with the PipeWire
implementation, no behaviour change; (3) move device-ID and default-device logic out of `Controller`;
(4) `IAutostart` with the XDG implementation.

## Build-time selection

CMake options (proposed): `MICFORGE_BACKEND=pipewire|wasapi` (default by platform), `MICFORGE_WITH_LV2`
(default ON on Linux, OFF on Windows), `MICFORGE_WITH_VST3` (ON). DSP tests build without Qt or a backend.
