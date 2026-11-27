# Changelog

All notable changes. Format: [Keep a Changelog](https://keepachangelog.com/); versions follow SemVer.

## [0.3.0] - 2026-11-27

### Added
- **New Voice Changer voices**, grouped into Everyday (Child, Woman, Man), Fantasy for tabletop and RPG games
  (Dragon, Orc, Goblin, Dwarf, Elf, Fairy, Demon, Lich, Golem, Old Wizard, alongside Giant and Monster) and Fun.
- **Voice size**: moves the formants separately from pitch, so a higher voice sounds like a child or a
  woman rather than a chipmunk, and a lower one sounds bigger. Uses a new formant shifter (about 37 ms of
  delay instead of 15 ms; when Voice size equals Pitch the classic shifter is used, as before).
- **Growl** (rough throat rattle) and **Whisper** (breathy to fully whispered) controls.
- **Keys for voices**: with system-wide shortcuts on, "Shortcuts for voices" in Settings gives every voice its
  own shortcut, plus one for your own voice. Choose the keys in the desktop's dialog ("Set keys for
  voices…"); a voice's key is shown on its tile. Press it again to go back to your own voice.

### Changed
- **Monster** rebuilt: a lower, bigger voice with growl, instead of a −10 semitone shift through heavy
  distortion that was 6 dB louder and mostly fuzz.
- **Drive** adds grit without adding volume: presets with Drive no longer jump 3–8 dB louder.
- "Character" is now called **Tone** (darker ↔ brighter); it was always a tone tilt.
- Settings and preset files from earlier versions keep their sound (their formants follow the pitch).

## [0.2.2] - 2026-11-27

### Fixed
- **Changing MicForge's system-wide shortcuts in the desktop settings wasn't noticed** until MicForge
  restarted (the shortcut list in Settings showed the old keys). MicForge couldn't subscribe to the
  portal's "shortcuts changed" signal and logged "Could not connect … ShortcutsChanged" at every start.

## [0.2.1] - 2026-11-27

### Fixed
- **The start of sentences could be cut off** when Noise Removal and the Voice Gate were both on. Noise
  Removal stays almost silent for the first moments of speech and removes low-frequency energy the gate
  often listens to, so the gate opened 20–40 ms late. The gate now listens to the microphone before Noise
  Removal (which also gives it 10 ms of look-ahead without extra delay). On a user's recording the worst
  phrase start went from 50 dB down to 19 dB down (first 60 ms), with no other phrase worse and other
  voices still kept out. Setup now measures the same sound the gate hears; a notice suggests re-running it.

### Added
- `tests/replay_mictest.cpp`: diagnose quiet sentence starts from a Mic Test export (local only).

## [0.2.0] - 2026-11-26

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
- **Presets you can share:** import and export preset files; presets now include your plugins and their
  settings, and never change your microphone or Setup (older presets did; fixed).
- **Undo / redo** for every sound setting (Ctrl+Z, Ctrl+Shift+Z, or the arrows at the top).
- **System-wide shortcuts:** mute and **push-to-talk** from any app (Settings → Keyboard shortcuts).
- Plugins: block list, a progress bar and Stop for plugin searches, CPU use per plugin, and presets per plugin.
- **Plugins can run separately** ("Run separately", on by default for VST3): if a plugin crashes or freezes,
  MicForge and your processed mic keep working and the plugin restarts by itself.
- **VST3 plugins can open their own window** (when running separately).
- Presets are included in the daily backup.
- **No more clicks** when you move a slider, switch an effect on or off, or use Compare: every change glides.
- **True-peak limiter:** the output never goes over the ceiling, even between samples.
- **Voice Gate** explains itself ("closed: background noise", "closed: not close enough"…), and ignores
  steady background noise (fans, air conditioning) that gets louder than its threshold.
- **Only open for speech** (optional): the gate also asks the AI voice detector, so typing and clicks stay out.
- **Hum Removal** effect: removes 50/60 Hz electrical buzz and its overtones; finds the frequency itself.
- **Tame "p" and "b" pops** (Low Cut, on by default).
- **Reduce breaths** (experimental, off by default).
- Voice Isolation shows **what the gate hears**: a live spectrum with its listening band and your Setup
  voices.
- **Diagnostics** page: problems in plain English, audio engine and devices, sample rate, block size,
  latency, processing load, glitch counters (late audio, dropouts, clipping), plugin errors and the log.
  **Copy or save a report** to share when asking for help: it never contains audio, and your home folder,
  user name, computer name, preset names and plugin locations are removed (device names and log lines
  only if you tick them).
- **Mic Test** page: record yourself once and compare the raw mic with MicForge's processed sound, played
  only to your speakers or headphones and matched in loudness, with waveforms, level readings, advice, and
  WAV export.
- **Each microphone keeps its own Setup.** Switching microphones loads that microphone's Setup.
- Setup now warns when your mic clipped while you talked, or when your voice is barely above the room noise.
- **Safe mode** (`--safe-mode`, Settings → Troubleshooting): starts without plugins and with default sound
  settings and saves nothing. Entered automatically after two failed starts in a row.
- Rotating log file (`~/.local/state/MicForge/MicForge/micforge.log`) and journal messages.
- Daily settings backups (14 kept); a damaged settings file is kept rather than overwritten.
- Tray menu: "Restart audio engine". Settings: Troubleshooting section.
- Command-line options: `--help`, `--version`, `--minimized`, `--safe-mode`, `--replace`.
- Tests: `tests/startup_test.sh`, `tests/reconnect_test.sh`, `tests/autostart_test.cpp`, mute checks in
  the DSP tests.
- CMake build (`CMakeLists.txt`) alongside qmake, with ctest. Tests: `audiocore`, `privacy`,
  `calibprofiles`, level analysis in `dsp`.
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
