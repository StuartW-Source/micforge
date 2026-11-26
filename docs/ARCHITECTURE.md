# MicForge architecture

## Processes and threads

| Thread | Runs | Rules |
|---|---|---|
| UI (Qt main) | QML, `Controller`, `PluginManager`, start-up, settings | May block briefly; never touched by audio callbacks |
| PipeWire loop (`pw_thread_loop`) | Stream `process` callbacks → `VoiceChain::process`, plugin `process` | **Real-time**: no allocation, locks, I/O, logging |
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
