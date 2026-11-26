# Testing

Always test with the **test instance** (`MICFORGE_TEST=1`): separate settings, lock, log, recordings and
virtual mic ("MicForge TEST (ignore)"), so the user's real MicForge is never disturbed.
Add `QT_QPA_PLATFORM=offscreen` for headless runs.

## Suites

| Suite | Command | What it covers |
|---|---|---|
| DSP + voice separation | `g++ -std=c++20 -O2 -Isrc tests/dsp_test.cpp src/optional/rnnoise_api.cpp -ldl -o /tmp/dsp_test && /tmp/dsp_test` | Every effect, gate timing/look-ahead, limiter ceiling, separation analysis, closeness check, step checks. AI noise checks are **skipped** (not failed) when rnnoise isn't installed |
| Start-up lifecycle | `tests/startup_test.sh build/micforge` | Single-instance hand-over, frozen instance + `--replace`, SIGTERM clean quit, stale lock, crash loop → safe mode, safe mode writes nothing, PipeWire unavailable at start |
| Engine recovery | `tests/reconnect_test.sh build/micforge` | Private PipeWire daemon (desktop audio untouched): connect, daemon killed, automatic reconnection + virtual mic re-created |
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

`MICFORGE_PAGE` takes a page (`overview`, `isolation`, `effects`, `devices`, `presets`, `settings`) or an
effect id (`gate`, `comp`, …, opens it on the Effects page). Review every page in both themes at the
minimum size (860×600), a mid size (~1200×760) and a large one (1920×1080), and check the log for QML
warnings.

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

## Gaps (planned)
CI (GitHub Actions), QML/controller tests (including keyboard navigation), settings-migration tests, measured latency/CPU/xruns,
voice-isolation false-accept/false-reject on licensed recorded fixtures, Windows runs of the DSP tests.
