# Testing

Always test with the **test instance** (`MICFORGE_TEST=1`): separate settings, lock, log, recordings and
virtual mic ("MicForge TEST (ignore)"), so the user's real MicForge is never disturbed.
Add `QT_QPA_PLATFORM=offscreen` for headless runs.

## Suites

With CMake (verified): `cmake -B build-cmake -G Ninja && cmake --build build-cmake && ctest --test-dir build-cmake --output-on-failure`
runs every suite below except the plugin hosts and the recordings analyser.

| Suite | Command | What it covers |
|---|---|---|
| DSP + voice separation | `g++ -std=c++20 -O2 -Isrc tests/dsp_test.cpp src/optional/rnnoise_api.cpp -ldl -o /tmp/dsp_test && /tmp/dsp_test` | Every effect, gate timing/look-ahead, limiter ceiling, separation analysis, closeness check, step checks. AI noise checks are **skipped** (not failed) when rnnoise isn't installed |
| Start-up lifecycle | `tests/startup_test.sh build/micforge` | Single-instance hand-over, frozen instance + `--replace`, SIGTERM clean quit, stale lock, crash loop → safe mode, safe mode writes nothing, PipeWire unavailable at start |
| Engine recovery | `tests/reconnect_test.sh build/micforge` | Private PipeWire daemon (desktop audio untouched): connect, daemon killed, automatic reconnection + virtual mic re-created |
| Audio core | ctest `audiocore` | Block/timing statistics, late-block detection (no false alarm after a stream restart), clipping before gain on the selected channel only, time-aligned raw/processed mic-test takes, playback reaches the monitor but never the virtual mic, underrun counting |
| Privacy | ctest `privacy` | Report scrubbing: home folder, user and computer names (any case, whole words, computer first), file paths, device names |
| Setup profiles | ctest `calibprofiles` | Save/restore per microphone, recordings and zones follow the mic, no leftover take between mics, sound-preset parameters excluded, nothing saved without a valid Setup |
| Preset files | ctest `presetfile` | Round trip, v1 migration, Setup keys never written/applied, plugins without paths, clamping, bad/newer files refused |
| Undo | ctest `undostack` | Drag merging, groups, redo cleared by new changes, back-to-start cancels, 100-step bound |
| Controller | ctest `controller` (private temporary XDG folders, no audio engine) | Undo/redo, presets keep Setup (also old files), preset load = one undo step, export/import with name collision, bad files, shortcuts + push-to-talk with a fake portal |
| Plugin bridge | ctest `bridge` | Two processes over shared memory: processed-or-dry never mixed, deadline misses during a stall, round-trip time, clean quit |
| Separate plugins | ctest `remoteplugin` (needs lilv + Calf) | Real helper process: identical output to in-process, settings delivered, cost per block, killed helper reported and audio untouched |
| Plugin window | `MICFORGE_HOST_BINARY=build-cmake/micforge build-cmake/plugin_editor_check` (manual; opens a window; needs lsp-plugins-vst3) | Window opens at the plugin's size, no missed blocks, closes on request |
| Autostart | `g++ -std=c++20 -fPIC -Isrc $(pkg-config --cflags Qt6Core) tests/autostart_test.cpp src/platform/autostart_xdg.cpp $(pkg-config --libs Qt6Core) -o /tmp/autostart_test && /tmp/autostart_test` | Login entry create/remove/hidden flag, refresh never re-points at a development build, test instance never touches the real entry |
| Plugin hosts | see "Plugin host test" below | LV2 scan/load/process/parameters; VST3 scan/load/parameters/processing |
| Recordings analyser | `g++ -std=c++20 -O2 -Isrc tests/analyse_recordings.cpp src/optional/rnnoise_api.cpp -ldl -o /tmp/analyse && /tmp/analyse ~/.local/share/MicForge/MicForge/calib` | Runs the gate on a user's saved setup recordings (local only) |

### Plugin host test
Builds against the VST3 SDK host sources and a test VST3 plugin (the SDK's ADelay example). The build
script currently lives outside the repository; moving it into CMake/CI is a Phase 7 task.

## UI screenshot review

Pictures of MicForge's own window only (never use desktop screenshot tools):

```bash
MICFORGE_TEST=1 QT_QPA_PLATFORM=offscreen MICFORGE_PAGE=overview MICFORGE_THEME=light \
  MICFORGE_SIZE=1200x760 MICFORGE_SNAPSHOT=/tmp/overview.png build/micforge
```

`MICFORGE_PAGE` takes a page (`overview`, `isolation`, `effects`, `devices`, `mictest`, `presets`,
`diagnostics`, `settings`) or an
effect id (`gate`, `comp`, …, opens it on the Effects page). Review every page in both themes at the
minimum size (860×600), a mid size (~1200×760) and a large one (1920×1080), and check the log for QML
warnings.

Test-instance-only aids: `MICFORGE_MICTEST_DEMO=1` (synthetic mic-test takes; the microphone isn't
recorded), `MICFORGE_REPORT=<file>` (writes a diagnostic report next to the snapshot; check it for
personal details), `MICFORGE_SWITCH_TO=<device node name>` (switches microphones 1.2 s after start, to
exercise Setup profiles).

## Results log
- 2026-11-26 (Phase 1): DSP 52 passed (rnnoise checks skipped: library not installed on the test machine);
  start-up 9/9; reconnect 3/3. Real launches verified: login autostart unit
  (`systemctl --user start app-micforge@autostart.service`) and menu launch (hand-over to the running
  instance). **Not yet verified:** an actual logout/login and reboot.

- 2026-11-26 (Phase 2): DSP all passed, including rnnoise and the new mute checks (rnnoise now installed);
  autostart 6/6; start-up 9/9; reconnect 3/3; the user's real login entry and running MicForge untouched.
  Screenshot review of all six pages in dark and light at 860×600, 1200×760, 1400×880, 1920×1080, no QML
  warnings. Found and fixed: empty header status pill, gate controls overflowing at 860 px, stretched meters
  at mid widths, overlapping closeness labels, text overflowing the loudness-margin box. **Not verified:**
  the CMake build (cmake not installed), keyboard traversal by hand, live desktop theme switching,
  fractional scaling.

- 2026-11-27 (fix: quiet sentence starts): diagnosed on the user's 30 s Mic Test export (local, read-only)
  with what became `tests/replay_mictest.cpp`. Two measurement mistakes on the way, corrected: averaging
  dB over 20 ms frames let room-hum frames count as "speech", and a peak envelope never registered pauses.
  Root cause: the gate listened after noise removal. dsp_test: gate + noise removal at phrase starts 6.2 dB
  on 0.2.0 (fail) -> 3.1 dB (noise removal alone 2.3). User recording: worst phrase 50.2 -> 18.6 dB, others
  unchanged or better, other voices 29.9 dB down, room noise -82.4 dB, both unchanged. Rejected: blending
  the un-denoised sound back in (fixed one phrase, made others 1-3 dB worse).
- 2026-11-26 (Phase 5, plugin host): ctest 12/12 (new: bridge, remoteplugin); plugin_editor_check with
  LSP Compressor Mono VST3 (installed by the user for this). Note: in the test binaries `qInfo` output
  is suppressed, so helper diagnostics are also delivered via `onLog`.
- 2026-11-26 (Phase 5): ctest 10/10 (new: presetfile, undostack, controller; plugin CPU in audiocore;
  preset backup in startup 10/10). Real GlobalShortcuts portal detected silently (Plasma 6.8, version 2);
  binding dialog and key presses left to the user. qmake build checked.
- 2026-11-26: reboot/login autostart confirmed from the user's log (boot 19:06:49, login launch 19:07:09,
  ready 19:07:11).
- 2026-11-26 (Phase 4): tests written first and run against a copy of the previous `src/` (baseline),
  then against the new code; results in ROADMAP Phase 4. ctest 7/7. Benchmarks (default chain, 60 s):
  0.23 % → 0.52 % of one core. Screenshots: Voice Gate panel (dark/light, 860×600, 1400×880), Hum
  Removal and Low Cut panels, Effect Library, spectrum with synthetic Setup recordings (removed after).
  The user's own Setup recordings were used locally, read-only, to check the voice detector and the
  plosive margin; nothing was copied or committed.
- 2026-11-26 (Phase 3): CMake build verified; ctest 7/7 (dsp incl. level analysis, audiocore 16,
  privacy 7, calibprofiles 13, autostart, startup 9/9, reconnect 3/3); qmake build still works. A real
  report from the test instance contained no user name, computer name, home path or device names.
  Mic-switch round trip on the test instance (AudioBox → Arctis → AudioBox) saved and restored the
  Setup. Screenshots of Diagnostics, Mic Test (demo takes) and Audio Devices at 860×600 and 1400×880 in
  both themes. Found and fixed: plugin paths in error messages, user name replaced inside the computer
  name, cut-off timing values, absurd figures for a fully gated background. **Not verified:** Setup
  results with the new level rows, file/folder dialogs, playback heard on real speakers.

## Gaps (planned)
CI (GitHub Actions), QML/controller tests (including keyboard navigation), settings-migration tests, measured latency/CPU/xruns,
voice-isolation false-accept/false-reject on licensed recorded fixtures, Windows runs of the DSP tests.
