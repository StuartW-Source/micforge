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

## Seams (implemented in Phase 2)

Only the abstractions the code needs today:

- **`AudioCore`** (`src/audio/audiocore.*`): everything between the OS and the apps that isn't OS
  specific: `Params`, `Meters`, `VoiceChain`, `Recorder`, `PluginRack`, and the lock-free rings that feed
  the virtual mic and monitor. A Windows backend reuses it as is.
- **`IAudioBackend`** (`src/audio/audiobackend.h`): `start()` (re-entrant; false = retry later),
  `setInputDevice(id)`, `setMonitor(on)`, `defaultInputDevice()`, `connected()`, `inputRunning()`,
  `virtualMicInUse()`, `error()`, plus callbacks `onDevicesChanged`, `onStateChanged`, `onConnectionLost`,
  `onDefaultInputChanged`. `BackendConfig` names the endpoints (the test instance uses its own).
  `PipeWireBackend` is the only implementation; the virtual microphone stays inside it because its
  Windows shape depends on Phase 8 research.
- **`IAutostart`** (`src/platform/autostart.h`): `enabled()`, `setEnabled(on, hidden)`, `refresh(hidden)`,
  `describe()`. `XdgAutostart` on Linux; Windows will use `HKCU\...\Run`.
- **Not added:** `IPlatformPaths` (`QStandardPaths` already gives correct locations on both platforms)
  and a virtual-microphone interface (see above).

Verified after the extraction: start-up 9/9, reconnect 3/3, DSP, autostart 6/6.

## Build-time selection

CMake options (in `CMakeLists.txt`, not yet built on this machine): `MICFORGE_BACKEND=pipewire` (the only
backend so far; `wasapi` planned), `MICFORGE_WITH_LV2` (default ON on Linux; OFF uses
`src/plugins/lv2_disabled.cpp`), `MICFORGE_WITH_VST3` (ON), `MICFORGE_BUILD_TESTS`. The DSP test builds
without Qt or a backend.
