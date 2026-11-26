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
mkdir -p build && cd build && qmake6 ../micforge.pro && make -j$(nproc) && cd ..
g++ -std=c++20 -O2 -Isrc tests/dsp_test.cpp src/optional/rnnoise_api.cpp -ldl -o /tmp/dsp_test && /tmp/dsp_test
tests/startup_test.sh build/micforge          # start-up / single-instance / safe-mode lifecycle
tests/reconnect_test.sh build/micforge        # PipeWire loss + recovery (private PipeWire daemon)
```

Plugin-host tests and the recordings analyser are described in `docs/TESTING.md`.

## Rules that protect the user

- **Never disturb the user's running MicForge** (it may be live in a call). Use the test instance:
  `MICFORGE_TEST=1` gives separate settings, lock, log, recordings and virtual mic. Add
  `QT_QPA_PLATFORM=offscreen` for headless runs. Don't kill/restart the real instance without asking.
- **Screenshots**: use `MICFORGE_TEST=1 MICFORGE_PAGE=<stage> MICFORGE_SNAPSHOT=<png>`
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
| `src/audioengine.*` | PipeWire backend: capture, virtual mic, monitor, reconnection |
| `src/controller.*` | QML-facing state: params, presets, calibration, autostart, engine lifecycle |
| `src/startup.*` | Logging, single instance, crash-loop guard / safe mode, settings backups, signals |
| `src/optional/` | Run-time loading of optional libraries (rnnoise, lilv) via `src/platform/dynlib.h` |
| `src/plugins/`, `src/pluginmanager.*` | VST3 + LV2 hosting, plugin chain management |
| `qml/` | UI. `Theme.qml` holds colours/presets; `Stage*.qml` are effect panels |

## Real-time rules (audio thread = PipeWire `process` callbacks, `VoiceChain::process`, plugin `process`)

No allocation, locks, file/console I/O, Qt signals or blocking calls. UI → audio uses atomics
(`Params`) or lock-free queues (`ParamQueue`, `Ring`). Audio → UI uses atomics (`Meters`).

## Conventions

C++20, Qt 6, qmake (CMake migration planned, see ROADMAP). Plain-English, neutral UI wording (no
assumptions about who else is in the room). Keep DSP free of Qt/PipeWire types.
