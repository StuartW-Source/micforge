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
  channel select → input gain → [noise removal] → [hum removal] → low cut (+ plosive guard)
  → voice gate (detector: band filter + closeness + adaptive floor + optional voice detector)
  → [breath reduction] → [auto level] → compressor → [de-esser] → [voice changer] → [EQ] → tone
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

## DSP decisions (Phase 4)

- **Smoothing.** `Smoothed` (one-pole ~10 ms) for gains; `SmoothBiquad` glides frequency/Q/gain and
  recomputes coefficients every 16 samples; `StageFade` crossfades each stage over 10 ms when switched and
  clears its state before it fades in; Compare crossfades raw/processed (`kMaxBlock` = 1024 raw samples
  kept). The first configuration after start applies at once.
- **True peak.** `TruePeakDetector`: 4× windowed-sinc interpolation (16 taps per phase); its 8-sample lag
  fits inside the limiter's existing 1.5 ms look-ahead, so latency is unchanged.
- **Adaptive floor.** 20 ms detector peaks → 0.5 s spread; steady half-seconds are background. Floor =
  quietest steady half-second of the last 4 s; it rises only with steadiness in two separate gaps or 3 s
  unbroken, and falls at once. Applied only if it would reach the close level (close = floor + 3, open = +5).
- **Voice detector.** rnnoise VAD (Noise Removal's, or a second instance). Opening and holding need it;
  200 ms memory; look-ahead raised to 20 ms while on.
- **Hum Removal.** Notches 3 Hz wide on f0..10·f0. Auto mode: Goertzel at k·50/k·60 and ±8 Hz neighbours,
  2 of 3 harmonics 10 dB "peaky", the same verdict in two consecutive seconds.
- **Plosive guard.** Sub-120 Hz vs 300 Hz–3 kHz envelope ratio > 6 dB (and sub-bass > −35 dB) → 200 Hz low
  shelf down to −24 dB, 2 ms / 80 ms.
- **Breath reduction.** 20 ms frames: autocorrelation periodicity on a 12 kHz copy, share above 4.5 kHz,
  level vs recent voiced level; 8 breathy frames in a row → turn down.
- **Spectrum view.** `ScopeTap` (atomic ring written only while the view is open) → FFT on the UI thread.

## Presets, undo, shortcuts, plugins (Phase 5)

- **Presets** (`src/presetfile.*`): sound settings only; `isSetupKey()` lists what never travels. The plugin
  chain is stored by format + id + parameter values (`PluginManager::exportChain/importChain`); on import a
  plugin is resolved from scan results, the current chain, or (LV2) its URI.
- **Undo** (`src/undostack.h`): recorded in `Controller::applyToEngine`, the one point every change passes;
  `Controller::UndoGroup` (RAII) groups preset loads, Setup and mic switches.
- **Global shortcuts** (`src/platform/globalshortcuts*.{h,cpp}`): `IGlobalShortcuts`, XDG portal
  implementation over Qt D-Bus (request/response signals subscribed before each call).
- **Plugins**: `PluginRack::load[]` per-slot CPU share (steady_clock around each `process`); block list in
  settings; plugin presets as files. Out-of-process hosting evaluated in docs/PLUGIN_HOSTING.md.

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
