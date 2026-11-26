# MicForge roadmap

Living document. Phases follow the development brief; each phase needs approval before it starts.
Status: **Phase 1 complete (pending a real reboot/login confirmation by the user). Phase 2 complete
(`phase2-architecture-ui`). Phase 3 complete on branch `phase3-diagnostics`. Phase 4 awaiting approval.**

---

## Phase 1 — Repository audit, start-up fix and reliability ✅

### Incident: MicForge failed to autostart and to open manually (2026-11-26)

**Symptom.** Nothing happened at login or from the menu.

**Evidence.** The user journal showed every launch (`app-micforge@autostart.service` at login and two
menu launches) exiting immediately with status 127:
`error while loading shared libraries: librnnoise.so.0: cannot open shared object file`.

**Root cause.**
1. MicForge was hard-linked against `librnnoise` (AI noise removal) and `liblilv` (LV2 hosting).
2. MicForge was installed with `install.sh` into `~/.local`, which pacman doesn't track.
3. On 2026-11-25 23:10 EasyEffects was uninstalled. Pacman then removed its now-unneeded dependencies,
   including **rnnoise**, without knowing MicForge needed it.
4. The dynamic linker refused to start the program, before any MicForge code (or error message) could run.
   `lilv` was in the same position (installed only as a dependency of other packages).

**Fixes (verified).**
- rnnoise and lilv are now **loaded at run time** (`src/optional/`, `src/platform/dynlib.h`). If missing,
  MicForge starts normally, the affected feature passes audio through, and the UI explains how to install it.
  The binary no longer links either library (`readelf -d` checked).
- `install.sh` warns when run-time packages are missing or only installed as dependencies, and
  suggests `pacman -D --asexplicit`. The AUR packages declare them as dependencies, so pacman protects them.

### Start-up lifecycle hardening (verified by `tests/startup_test.sh`, 9/9)

| Area | Before | Now |
|---|---|---|
| Single instance | 300 ms socket probe. A frozen instance silently blocked all launches | Lock file (PID-based staleness) + "show" handshake with reply. Frozen instance detected; `--replace` or an interactive prompt replaces it; login launches exit with a log entry instead of hanging |
| Stale lock/socket after a crash | Partly handled | Recovered automatically |
| PipeWire not ready | Engine failed once, never retried | Starts anyway; reconnect with back-off (0.5–5 s) |
| PipeWire restart/crash | Virtual mic gone until app restart | Lost connection detected (core `EPIPE`); reconnects and re-creates the virtual mic (`tests/reconnect_test.sh`, private daemon, 3/3) |
| Tray missing at login | Window popped up | Keeps processing in the background; adds the icon when the tray appears (window shown only if no tray after 2 min) |
| QML load failure | Exit code 1, no message | Error dialog with details + log path |
| Diagnostics | stderr only | Rotating log `~/.local/state/MicForge/MicForge/micforge.log` (+ journal) |
| Crash loops | None | Two failed launches in a row → next launch in **safe mode** |
| Safe mode | None | `--safe-mode`: default sound settings, no plugins, **writes nothing**; banner + "Restart normally" |
| Settings safety | None | Daily backups (14 kept); unreadable file copied before defaults are used; invalid values clamped |
| Logout / shutdown | SIGTERM killed instantly | SIGTERM/SIGINT/SIGHUP → clean quit (settings saved) |
| Autostart entry | Rewritten to whichever binary ran last (incl. dev builds) | Only rewritten by an installed copy, or when its target is gone |

**Decision: XDG autostart, not a systemd user service.** KDE (and GNOME) already turn
`~/.config/autostart/*.desktop` into a systemd unit (`app-micforge@autostart.service`), which gives journal
logging and correct session ordering. A separate systemd unit would compete with it. XDG autostart also
matches how the user toggles it in System Settings, and maps cleanly onto Windows' per-user startup.
There are no arbitrary start-up delays: PipeWire and tray readiness are handled reactively.

**Still to confirm by the user:** a real logout/login and a reboot (cannot be performed from this session).

### Audit findings (whole repository, 2026-11-26)

Severity: 🔴 high, 🟠 medium, 🟢 low.

**Reliability and real-time safety**
- 🟠 Third-party plugins run **in-process** on the audio thread. A plugin that crashes or blocks takes
  audio (or the app) down. VST3 *scanning* is isolated; *processing* is not (see Phase 5).
- 🟢 VST3 parameter changes allocate only beyond pre-reserved capacity (`kQueueReservedPoints`); acceptable, keep watching.
- 🟠 No processing-overload, xrun or callback-time measurement; latency is estimated, not measured (Phase 3).
- 🟠 Fixed 48 kHz processing; PipeWire resamples other rates. No explicit sample-rate-change handling (Phase 4).
- 🟠 No **mute** state or control (tray/shortcut/UI); no explicit disconnected/processing/bypassed/error
  state model beyond the status pill (Phases 2–3).
- 🟢 Privacy: a disconnected mic is never replaced by another (`node.dont-reconnect`), and the virtual mic
  outputs silence without input. However, "Compare"/bypass intentionally sends unprocessed audio, and a
  failing VST3 `process()` passes its input through (to review in Phase 4).
- 🟢 `Controller` runs `pactl get-default-source` synchronously (≤1.5 s) on first run only.

**Architecture and debt**
- 🟠 `Controller` (~870 lines) mixes parameters, presets, calibration, autostart and engine lifecycle.
  Split into focused services when Phase 2 touches it (no rewrite for its own sake).
- 🟠 Filter maths are duplicated in `qml/ResponseCurve.qml` (JS) and `src/dsp.h`; they can drift.
  Expose the response curve from C++.
- 🟠 Two preset systems: built-in sound presets in C++, Voice Changer/Echo/Reverb presets in `Theme.qml`.
  No versioned preset format (Phase 5).
- 🟠 Build system is qmake. CMake is needed for MSVC/Windows and CI (Phase 2 proposal).
- 🟢 ~90 hard-coded colours across QML files outside `Theme.qml`. No spacing/typography scale, no light
  theme, limited keyboard navigation (Phase 2 design system).

**Testing**
- 🟠 No CI. Plugin-host and analysis tools are built by hand. No QML/controller tests (Phases 2, 7).
- 🟢 DSP (52 checks + rnnoise checks when installed), start-up (9) and reconnection (3) suites exist.

**Packaging**
- 🔴 *(root cause, fixed)* `~/.local` installs aren't tracked by pacman; documented, and `install.sh` warns.
- 🟠 Release PKGBUILD has `SKIP` for the release tarball checksum (must be filled after tagging).
- 🟠 A `~/.local` install and an AUR install can coexist; `~/.local` shadows `/usr` in menus/PATH.
  `install.sh` now warns; add an uninstall script (Phase 7).
- 🟢 No CHANGELOG before this phase (added).

**Features described in the README: implementation status**

| Feature | Status |
|---|---|
| Voice gate, closeness check, setup, learn/redo | Implemented, tested on synthetic + one user's real recordings |
| 15 built-in effects | Implemented; DSP-tested. No per-effect parameter smoothing for all controls (zipper noise possible on fast changes) |
| VST3/LV2 hosting | Implemented with generic controls; no plugin GUIs, no plugin presets, no runtime isolation |
| Autostart / tray | Implemented; hardened in Phase 1 |
| AppStream/desktop/AUR | Implemented; AUR not yet published |

---

## Phase 2 — Cross-platform architecture, UI design system, polish ✅ (2026-11-26)

| Item | Result |
|---|---|
| CMake build alongside qmake | `CMakeLists.txt` (options `MICFORGE_BACKEND`, `MICFORGE_WITH_LV2/VST3`, `MICFORGE_BUILD_TESTS`, ctest). Verified at the start of Phase 3 (CMake 4.5, Ninja): clean build, all ctest suites pass, rnnoise/lilv not linked. |
| Platform seams | `AudioCore` (platform-independent processing, rings, meters, plugin rack) + `IAudioBackend` / `PipeWireBackend`; `IAutostart` / `XdgAutostart`. Default-device tracking moved into the backend (PipeWire `default` metadata; no more `pactl`). `IPlatformPaths` **not** added: `QStandardPaths` already covers both platforms. |
| Split `Controller` | Audio I/O, default device and autostart moved out. Calibration and presets stay in `Controller` for now: splitting them adds churn without enabling anything yet; revisit when Phase 3 (calibration) or Phase 5 (versioned presets) changes them. |
| Mute + state model | `mute` parameter (10 ms fade, never saved/preset), `appState` = disconnected / connecting / no-input / muted / bypassed / processing, shown in the header, the tray (icon overlay + menu) and page notices. Ctrl+M. |
| Design system | Semantic colour tokens with dark and light values, spacing/radius/type/control-size tokens in `Theme.qml`; System/Dark/Light setting. No QML file hard-codes colours. Themed ComboBox/TextField/Menu. Tab focus ring, Space/Enter, slider keys, accessible names. |
| Information architecture | Sidebar: Overview, Voice Isolation, Effects (chain strip kept), Audio Devices, Presets, Settings (Ctrl+1..6). Diagnostics is Phase 3. |
| Screenshot review | Test-instance snapshots of every page in both themes at 860×600, 1200×760, 1400×880 and 1920×1080; overflow and overlap problems found and fixed (see TESTING.md). |

Not verified yet: keyboard focus traversal by hand (only code-reviewed), the System theme following a live
desktop theme switch, fractional-scaling displays.

## Proposed priorities for the remaining phases

### Phase 3 — Diagnostics, recording and calibration ✅ (2026-11-26)

| Item | Result |
|---|---|
| Engine measurements | `AudioCore::stats` (lock-free): block size, processing time (avg/max), load, late input blocks, virtual-mic and monitor underruns, input clipping before gain. PipeWire adds graph rate and input/output latency (`pw_stream_get_time_n`). Real xrun counts from the PipeWire driver aren't exposed to clients; late blocks and underruns are what MicForge can observe itself. |
| Diagnostics page | Problems in plain English, engine, devices, timing with load bars, glitch counters (resettable), plugin errors, optional features, log viewer. |
| Report | Copy / save / preview. Always removes home folder, user and computer names, preset names, plugin paths; device names and log lines only when ticked; never audio. Verified with `tests/privacy_test.cpp` and a real report from the test instance. |
| Mic test | 5–30 s simultaneous raw + processed takes; playback to the monitor output only (tested: never reaches the virtual mic); loudness matching; waveforms; level analysis (peak, speech, background, voice above background) with findings; 16-bit WAV export; in memory until exported. |
| Calibration | Per-microphone Setup profiles (saved on Setup, restored on mic switch, nothing deleted). Setup flags input clipping during "Your voice" and a low voice-to-room level. Re-running individual steps already existed (R2.8). |
| Controller split | Diagnostics, mic test and profiles are separate classes (`src/diagnostics.*`, `src/mictest.*`, `src/calibprofiles.*`), not more Controller code. |

Not verified: the Setup results screen with the new level rows (needs a real Setup run, which records
the room), the save-file and folder dialogs (portal dialogs can't run offscreen), playback audibility on
real speakers.

### Phase 4 — Voice isolation and DSP
Gate decision explanation ("opened: level + closeness"), detector spectrum view, adaptive noise floor,
whisper handling, VAD (rnnoise VAD output) as an optional input, parameter smoothing everywhere, true-peak
limiter, hum removal (50/60 Hz), plosive and breath reduction. Each must beat the current behaviour in tests.

### Phase 5 — Presets, profiles, shortcuts, plugins
Versioned preset files (import/export, cross-platform), device profiles, undo/redo, backups.
Global shortcuts via the XDG GlobalShortcuts portal (mute, push-to-talk). Plugins: blacklist, scan
progress/cancel, per-plugin CPU, plugin presets, editor windows (X11/XWayland first), and an evaluation of
out-of-process runtime hosting.

### Phase 6 — Experimental speaker verification (research only, opt-in)
Benchmark lightweight speaker-embedding models (ONNX Runtime, CPU) against the current DSP approach.
No dependency is added unless it clearly improves results; all data local and deletable.

### Phase 7 — Linux release readiness
GitHub Actions (build, tests, makepkg, namcap), CHANGELOG/versioning, README refresh, uninstall script,
publish AUR packages (with approval).

### Phases 8–10 — Windows
Feasibility (WASAPI backend + existing virtual cable as Path A), then native backend, then
virtual-microphone driver research (Path B, SysVAD as reference only), installer and signing.
See `docs/WINDOWS_RESEARCH.md`. Nothing Windows-specific is built until Linux is stable and Phase 8 is approved.
