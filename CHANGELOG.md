# Changelog

All notable changes. Format: [Keep a Changelog](https://keepachangelog.com/); versions follow SemVer.

## [Unreleased]

### Fixed
- MicForge failed to start (at login and from the menu) when the optional **rnnoise** or **lilv** library
  had been removed, for example after uninstalling EasyEffects. Both are now loaded at run time: MicForge
  always starts, and the affected feature explains how to enable it.
- A frozen MicForge no longer silently blocks new launches: it's detected and can be replaced
  (`micforge --replace`, or a prompt on manual launch).
- The audio engine now retries when PipeWire isn't ready yet and **reconnects automatically after PipeWire
  restarts**, re-creating the virtual microphone.
- At login, if the system tray isn't ready yet, MicForge keeps running in the background and adds its
  tray icon when the tray appears, instead of opening its window.
- Logout/shutdown signals now quit cleanly (settings saved).
- On first start MicForge picked whichever microphone appeared first instead of the system default.
- A possible crash when quitting with plugins loaded (plugins were released after the audio engine).
- The "others" and "you" labels on the closeness check no longer overlap when the two are close.
- The login entry is no longer re-pointed at development builds.

### Added
- **Safe mode** (`--safe-mode`, Settings → Troubleshooting): starts without plugins and with default sound
  settings and saves nothing. Entered automatically after two failed starts in a row.
- Rotating log file (`~/.local/state/MicForge/MicForge/micforge.log`) and journal messages.
- Daily settings backups (14 kept); a damaged settings file is kept rather than overwritten.
- Tray menu: "Restart audio engine". Settings: Troubleshooting section.
- Command-line options: `--help`, `--version`, `--minimized`, `--safe-mode`, `--replace`.
- Tests: `tests/startup_test.sh`, `tests/reconnect_test.sh`, `tests/autostart_test.cpp`, mute checks in
  the DSP tests.
- CMake build (`CMakeLists.txt`) alongside qmake.
- Project docs: CLAUDE.md, ROADMAP, ARCHITECTURE, CROSS_PLATFORM, WINDOWS_RESEARCH, TESTING, TROUBLESHOOTING.

- **New layout** with a sidebar: Overview, Voice Isolation, Effects, Audio Devices, Presets and Settings
  (Ctrl+1 to Ctrl+6). The Overview shows what your apps are hearing, your mic, levels, voice-isolation
  status and quick controls. The effects chain and every effect panel are on the Effects page.
- **Mute** (header button, tray menu, Ctrl+M): apps receive silence, with a short fade. Never saved, so
  MicForge always starts unmuted.
- **Light theme**, and a System/Dark/Light choice in Settings → Appearance.
- Keyboard navigation: Tab moves between controls with a visible focus ring; Space/Enter activate; sliders
  take arrow keys, Page Up/Down and Home (reset).
- Presets page: save, replace, load and delete presets (deleting asks first).
- The status in the header and the tray icon show when MicForge is muted, bypassed, waiting for the mic
  or disconnected.
- README: MicForge is built with Claude (Anthropic).

### Changed
- Settings moved from a dialog to a page.
- The default microphone now follows PipeWire's default-device setting directly (no `pactl` call).
- `install.sh` warns when run-time packages are missing or could be removed as orphans.

## [0.1.0] - 2026-11-25
First release: voice gate with closeness check and guided setup, 15 effects, voice changer, VST3/LV2
hosting, tray/autostart, AUR packaging.
