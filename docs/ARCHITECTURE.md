# MicForge architecture

## Processes and threads

| Thread | Runs | Rules |
|---|---|---|
| UI (Qt main) | QML, `Controller`, `PluginManager`, start-up, settings | May block briefly; never touched by audio callbacks |
| PipeWire loop (`pw_thread_loop`) | Stream `process` callbacks → `AudioCore` → `VoiceChain::process`, plugin `process` | **Real-time**: no allocation, locks, I/O, logging |
| PipeWire loop (non-RT events) | Registry/state/core-error callbacks | Must not tear down PipeWire objects from inside callbacks; marshal to UI thread |
| Worker (`QtConcurrent`) | Voice-separation analysis | Pure computation on copied buffers |
| Helper process | `micforge --scan-vst3 <bundle>` | Isolates VST3 scanning crashes |

Communication: UI → audio via `Params` (atomic floats + version counter), `ParamQueue` (plugins),
`PluginRack` slots (atomic pointers + `busy` flag for safe hand-over). Audio → UI via `Meters` (atomics,
max-since-last-read) and lock-free `Ring` buffers (virtual mic, monitor).

## Signal flow

```
PipeWire capture (stereo, chosen node) ─► VoiceChain::process
  channel select → input gain → [noise removal] → low cut → voice gate (detector: band filter + closeness)
  → [auto level] → compressor → [de-esser] → [voice changer] → [EQ] → tone
  → (block hook) PluginRack → [warmth] → [exciter] → [doubler] → [echo] → [reverb] → output gain → limiter
─► Ring ─► virtual mic stream (Audio/Source "MicForge (processed mic)")
       └► Ring ─► monitor stream (headphones, optional)
```

## Code structure (Phase 2)

```
Controller (QML-facing state, presets, calibration)
  ├─ mf::AudioCore        src/audio/audiocore.*     platform-independent: Params, Meters, VoiceChain,
  │                                                 Recorder, PluginRack, virtual-mic/monitor rings
  ├─ mf::IAudioBackend    src/audio/audiobackend.h  OS audio I/O, devices, default device, connection state
  │     └─ PipeWireBackend src/audio/pipewirebackend.*  (Linux; a WASAPI backend is planned)
  └─ mf::IAutostart       src/platform/autostart.h  start at login
        └─ XdgAutostart    src/platform/autostart_xdg.cpp
```

The backend calls `AudioCore::processInput / readVirtualMic / readMonitor` from its real-time thread;
everything inside `AudioCore` follows the real-time rules. Backend events (devices, state, connection lost,
default device) are `std::function` callbacks fired on the backend's thread; `Controller` marshals them
to the UI thread. The backend is chosen at build time (`createAudioBackend`), not via plugins.

**Application state** (`Controller::appState`): `disconnected` → `connecting` → `no-input` →
`processing`, with `muted` and `bypassed` overriding `processing`. The header, tray icon and notices all
read this one value.

## Diagnostics, mic test and calibration profiles (Phase 3)

- **`AudioCore::stats`** (`EngineStats`): written only by the audio thread with relaxed atomics; the UI
  reads them and resets the "max since last read" values with `exchange`. Timing uses `steady_clock`
  (vDSO, real-time safe). A block arriving more than two block-lengths (+2 ms) after the previous one
  counts as late; gaps over 1 s are pauses, and `resetChain()` clears the reference time so a stream
  restart isn't counted. Clipping is measured on the raw device samples of the selected channel, before
  input gain, because that's the clipping software can't undo.
- **Mic test** (`AudioCore::test`, `MicTestSession`): the UI thread allocates both buffers while idle; the
  audio thread writes raw (channel + input gain) and processed samples at the same index, so the takes are
  time-aligned. Playback is read by `readMonitor` only; the virtual mic path never touches the test
  buffers. Live "Listen" and playback share the monitor stream: `Controller::updateMonitorStream()` keeps
  it open while either needs it; while playing, the live signal isn't sent to the monitor.
- **Diagnostics** (`src/diagnostics.*`) polls once a second only while its page is visible. Personal
  details are removed by `src/privacy.h` (pure functions, unit-tested): home folder → `~`, computer name
  (first, since it often contains the user name) and user name → placeholders as whole words in any case,
  paths → last component, device names → their type unless the user opts in.
- **Calibration profiles** (`src/calibprofiles.*`): the active Setup is still `calib/*` settings plus
  `calib/{room,you,others}.f32`; profiles are copies in `calib/devices/<sha1(device)>/` with a
  `profile.ini`. `calib/device` records which mic the active Setup belongs to. On a mic switch the
  outgoing Setup is saved before anything is restored; files are replaced via a temporary file. Profiles
  hold only mic/room parameters (`CalibrationProfiles::profileParams`), never sound-preset ones.

## UI structure (Phase 2)

`Main.qml` holds the header (status, Mute, Listen, Compare), the sidebar and page notices, and loads each
page on first visit (`OverviewPage`, `IsolationPage`, `EffectsPage`, `DevicesPage`, `PresetsPage`,
`SettingsPage`, and from Phase 3 `MicTestPage`, `DiagnosticsPage`). Effect panels (`Stage*.qml`, built on `StagePanel`) are reused unchanged by the Effects,
Voice Isolation and Audio Devices pages. Pages get the window as `app` for shared helpers
(`stageDefs`, `openSetup()`, `openStage(id)`, `navigate(id)`).

**Design system.** `Theme.qml` is the single source of colours (semantic tokens such as `bg`, `card`,
`text`, `onAccent`, `well`, `focus`, and stage accents), spacing (`sp1..sp6`), radii, type scale and
control sizes. Each colour has a dark and a light value (`pick(dark, light)`); light mode uses deeper
accent shades so accent-coloured text stays readable. `Theme.mode` follows `Controller.themeMode`
(`system` uses `Qt.styleHints.colorScheme`). Rule: **no hard-coded colours in QML**. Standard controls are
wrapped (`MfComboBox`, `MfTextField`, `MfMenu*`) so they follow the theme instead of Fusion. Focusable
custom controls show `FocusRing` and handle Space/Enter.

## Key decisions

### Optional run-time dependencies (2026-11-26)
rnnoise and lilv are loaded with `dlopen`/`LoadLibrary` (`src/platform/dynlib.h`, `src/optional/*`)
instead of being linked. **Why:** a missing optional library (removed as an "orphan" by pacman) stopped
MicForge from starting at all, with no error visible to the user. Now only the feature is disabled.
Core libraries (Qt, PipeWire) stay linked because MicForge can't work without them.
lilv calls keep their normal names in `lv2host.cpp` and are redirected by macros in `lilv_api.h`.

### Start-up lifecycle (2026-11-26)
`src/startup.*`: logging → single instance (QLockFile with PID-based staleness + local-socket handshake) →
signal handling → crash-loop guard → settings check/backup → controller/engine → QML → tray.
- **XDG autostart** (`~/.config/autostart/micforge.desktop`, `--minimized`) is the only start-up mechanism.
  KDE/GNOME run it as a systemd unit. A separate systemd user service would compete with it.
- The engine connects reactively (retry with back-off), never after a fixed sleep.
- Safe mode never writes settings or the plugin chain, so it can't damage the user's configuration.

### Audio engine resilience
`AudioEngine::start()` can be called repeatedly: it tears down any previous connection and rebuilds the
context, core, registry and streams. A core `EPIPE` error marks the connection lost; the controller
reconnects from the UI thread. The capture stream uses `node.dont-reconnect`, so PipeWire never
silently substitutes another microphone; the registry watcher reconnects when the chosen device returns.

### Plugin hosting
VST3 via Steinberg's MIT-licensed SDK hosting helpers (only the needed sources are compiled); LV2 via lilv.
Scanning is out-of-process; processing is in-process (runtime isolation is a Phase 5 evaluation).
A crash while *loading* a plugin is remembered and that plugin is disabled at next start.

### Settings and data locations (Linux)
- Settings: `~/.config/MicForge/MicForge.conf` (QSettings INI)
- Calibration recordings: `~/.local/share/MicForge/MicForge/calib/*.f32`
- Log + backups + launch guard: `~/.local/state/MicForge/MicForge/`
- Test instance uses `MicForgeTest` in place of the second `MicForge`.
