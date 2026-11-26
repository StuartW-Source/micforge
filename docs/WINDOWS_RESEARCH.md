# Windows research notes

Status: **not started** (Phase 8). These are starting points and open questions, not verified findings.
Every item below must be checked against current Microsoft documentation and tested on real Windows 11 x64
machines before decisions are made. Nothing here claims Windows compatibility.

## Audio backend (Phase 9)
- **WASAPI shared mode, event-driven** (`IAudioClient` with `AUDCLNT_STREAMFLAGS_EVENTCALLBACK`) for capture
  and monitor playback. The shared `VoiceChain` runs in the capture callback thread.
- **MMCSS** (`AvSetMmThreadCharacteristics("Pro Audio")`) for the processing thread.
- Device enumeration and hot-plug: `IMMDeviceEnumerator` + `IMMNotificationClient`.
- Recovery: handle `AUDCLNT_E_DEVICE_INVALIDATED` and Audio Service restarts by re-creating the client
  (mirrors the PipeWire reconnect logic).
- Format: request float32. Multi-channel interfaces are exposed as multi-channel endpoints, and the existing
  channel-select stage applies.
- Questions: interaction with Windows "audio enhancements" and communications ducking; microphone privacy
  permission UX; exclusive mode/ASIO only if measurements justify it (not required).

## Virtual microphone
- **Path A (prototype):** route output to an existing virtual audio cable to validate the backend and DSP.
  Check each candidate's licence; never bundle a proprietary driver without permission.
- **Path B (native):** a MicForge virtual audio driver, using Microsoft's **SysVAD** sample as a reference
  only. Open questions: WaveRT vs. other architectures; user↔kernel transport (shared ring buffer); driver
  signing (attestation signing via the Hardware Dev Center requires an EV certificate, cost to confirm);
  HVCI/memory-integrity compatibility; install/uninstall/upgrade; crash recovery.
  **Production must not require disabling Secure Boot, memory integrity or driver signature enforcement.**
- DSP stays in user space; the driver only moves audio.

## Desktop integration
Startup via `HKCU\Software\Microsoft\Windows\CurrentVersion\Run`; tray via `QSystemTrayIcon`; single
instance via the existing QLockFile/QLocalServer code (named pipes on Windows). Global shortcuts via
`RegisterHotKey`. Dark/light theme from the system palette.

## Distribution
CMake + MSVC x64; `windeployqt`; WiX or Inno Setup; code signing for the app (and the driver for Path B);
SmartScreen reputation; GitHub Actions Windows runner. Windows 10 and ARM64 to be evaluated separately.

## Acceptance (from the brief)
Installs and opens, autostarts, detects devices, DSP verified, virtual mic usable in Discord/OBS/Teams/
browsers, device reconnection, measured latency/CPU/memory, settings persist, clean upgrade/uninstall,
no weakened security settings, all on real Windows hardware.
