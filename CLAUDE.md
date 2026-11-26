# MicForge — working guide

Read this first, then `docs/ROADMAP.md` (current phase and priorities) and the doc relevant to the task
(`docs/ARCHITECTURE.md`, `docs/CROSS_PLATFORM.md`, `docs/TESTING.md`, `docs/TROUBLESHOOTING.md`,
`docs/REQUIREMENTS.md`, `docs/WINDOWS_RESEARCH.md`). Record important decisions in those docs and add a
`CHANGELOG.md` entry for user-visible changes.

## What MicForge is

A local-first microphone processor (Linux/PipeWire today, Windows planned). Its distinguishing feature is
keeping *other people in the room* off the user's mic (Voice Gate: band-filtered detector + closeness
check). It exposes a virtual mic, "MicForge (processed mic)".

## Build and test

```bash
./scripts/fetch-vst3sdk.sh                    # once (or: git submodule update --init)
cmake -B build-cmake -G Ninja && cmake --build build-cmake
ctest --test-dir build-cmake --output-on-failure   # dsp, audiocore, privacy, calibprofiles, autostart,
                                                   # startup lifecycle, PipeWire reconnect (private daemon)
# qmake still works too: mkdir -p build && cd build && qmake6 ../micforge.pro && make -j$(nproc)
```

Plugin-host tests and the recordings analyser are described in `docs/TESTING.md`.

## Rules that protect the user

- **Never disturb the user's running MicForge** (it may be live in a call). Use the test instance:
  `MICFORGE_TEST=1` gives separate settings, lock, log, recordings and virtual mic. Add
  `QT_QPA_PLATFORM=offscreen` for headless runs. Don't kill/restart the real instance without asking.
- **Screenshots**: use `MICFORGE_TEST=1 MICFORGE_PAGE=<page|stage> MICFORGE_SNAPSHOT=<png>` (optional
  `MICFORGE_THEME=dark|light`, `MICFORGE_SIZE=WxH`)
  (captures MicForge's own window only). Never use desktop screenshot tools (they captured a private window once).
- **Never restart the system PipeWire** for testing; `tests/reconnect_test.sh` runs a private daemon.
- Settings (`~/.config/MicForge/MicForge.conf`) and calibration recordings
  (`~/.local/share/MicForge/MicForge/calib/`) are user data: never delete or overwrite them silently.
  Daily backups live in `~/.local/state/MicForge/MicForge/backups/`.
- No pushing, tagging, releases or AUR uploads without explicit approval. Commit on a branch, not `main`.
- Pause for approval before a major architectural change or a new roadmap phase.

## Code map

| Path | Role |
|---|---|
| `src/dsp.h`, `src/effects.h` | Real-time processing chain (header-only, no Qt, platform-independent) |
| `src/separation.h` | Voice-separation analysis used by Setup (platform-independent) |
| `src/params.h` | Every parameter: enum + table (key, default, min, max) — single source of truth |
| `src/audio/audiocore.*` | Platform-independent processing core: params, meters, chain, plugin rack, rings |
| `src/audio/audiobackend.h`, `pipewirebackend.*` | `IAudioBackend` + PipeWire implementation: capture, virtual mic, monitor, default device, reconnection |
| `src/platform/` | `IAutostart` (XDG), run-time library loading (`dynlib.h`) |
| `src/controller.*` | QML-facing state: params, presets, calibration, app state/mute, engine lifecycle |
| `src/diagnostics.*`, `src/privacy.h` | Diagnostics page data and the privacy-scrubbed report |
| `src/mictest.*`, `src/levels.h` | Mic test session; level analysis (shared with Setup) |
| `src/calibprofiles.*` | Per-microphone Setup profiles |
| `src/spectrum.h`, `src/detectorspectrum.*` | FFT/log bands; the gate's spectrum view |
| `src/presetfile.*`, `src/undostack.h` | Versioned preset files; undo/redo history |
| `src/platform/globalshortcuts*` | System-wide shortcuts (XDG GlobalShortcuts portal) |
| `src/startup.*` | Logging, single instance, crash-loop guard / safe mode, settings backups, signals |
| `src/optional/` | Run-time loading of optional libraries (rnnoise, lilv) via `src/platform/dynlib.h` |
| `src/plugins/`, `src/pluginmanager.*` | VST3 + LV2 hosting, plugin chain management; `bridge.*` + `remoteplugin.*` run plugins in a separate `--plugin-host` process (docs/PLUGIN_HOSTING.md) |
| `qml/` | UI. `Main.qml` = header + sidebar + pages (`*Page.qml`); `Stage*.qml` are effect panels; `Theme.qml` holds design tokens and effect presets |

## Real-time rules (audio thread = PipeWire `process` callbacks, `VoiceChain::process`, plugin `process`)

No allocation, locks, file/console I/O, Qt signals or blocking calls. UI → audio uses atomics
(`Params`) or lock-free queues (`ParamQueue`, `Ring`). Audio → UI uses atomics (`Meters`).

## Conventions

C++20, Qt 6. CMake (`cmake -B build-cmake -G Ninja`, `ctest`) and qmake both build; keep both source
lists in sync until qmake is retired. QML: take every colour, spacing and font size from `Theme` (no hex
literals); use `MfComboBox`/`MfTextField`/`MfMenu*` rather than raw Controls; make custom controls
focusable (`activeFocusOnTab`, Space/Enter, `FocusRing`, `Accessible.name`). Plain-English, neutral UI wording (no
assumptions about who else is in the room). Keep DSP free of Qt/PipeWire types.
