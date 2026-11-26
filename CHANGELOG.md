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
- The login entry is no longer re-pointed at development builds.

### Added
- **Safe mode** (`--safe-mode`, Settings → Troubleshooting): starts without plugins and with default sound
  settings and saves nothing. Entered automatically after two failed starts in a row.
- Rotating log file (`~/.local/state/MicForge/MicForge/micforge.log`) and journal messages.
- Daily settings backups (14 kept); a damaged settings file is kept rather than overwritten.
- Tray menu: "Restart audio engine". Settings: Troubleshooting section.
- Command-line options: `--help`, `--version`, `--minimized`, `--safe-mode`, `--replace`.
- Tests: `tests/startup_test.sh`, `tests/reconnect_test.sh`.
- Project docs: CLAUDE.md, ROADMAP, ARCHITECTURE, CROSS_PLATFORM, WINDOWS_RESEARCH, TESTING, TROUBLESHOOTING.

### Changed
- `install.sh` warns when run-time packages are missing or could be removed as orphans.

## [0.1.0] - 2026-11-25
First release: voice gate with closeness check and guided setup, 15 effects, voice changer, VST3/LV2
hosting, tray/autostart, AUR packaging.
