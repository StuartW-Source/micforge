# MicForge requirements

## Purpose

MicForge makes anyone's microphone sound clean and professional on Linux. It also sends **only
the user's voice** to voice chat, streaming and recording apps.

It's designed for people who aren't audio specialists. It sets itself up from a few simple,
guided questions and short recordings, and explains every control in plain English.

### Situations it must handle

- **Other people talking nearby.** Housemates, family, a partner or colleagues in the same room,
  sometimes on their own calls at the same time. Without processing, their voices bleed into the
  user's mic and everyone on the call hears them.
- **Background noise.** PC fans, air conditioning, mechanical keyboards, traffic and hum.
- **Inconsistent levels.** Leaning back, speaking softly, or suddenly laughing or shouting.
- **Wanting a better sound.** Streamers, podcasters and singers who want a polished,
  "broadcast" voice.
- **Having fun.** Silly or character voices for games and streams.

It must work with any common microphone: dynamic (e.g. SM57, SM58, PodMic), condenser
(e.g. Blue Yeti, AT2020), headset, or laptop/webcam built-in. It must also work with any input,
whether a USB mic, headset, or one channel of a multi-input audio interface.

## R1. Voice separation (keep other people off the mic)

- R1.1 The Voice Gate opens for the user's voice and stays closed for other voices, room noise and
  keyboard clicks.
- R1.2 The gate decides from a **filtered detector** (sidechain). It listens to the frequency band
  where the user's close-up voice is strongest compared with sounds further away. The band is
  **chosen automatically** from measurements, never guessed.
- R1.3 The gate keeps rejecting other voices **even when they're loud** (laughing, shouting).
  A level threshold alone can't guarantee that, so the gate also applies a **closeness check**:
  the balance of bass (100–300 Hz) to treble (1–4 kHz). A voice close to a directional mic gets a
  strong bass boost (the proximity effect), and distant voices don't, at any volume.
  - The closeness check only decides when the gate **opens**. Every syllable has a vowel that
    passes it, and level plus hold carry the brighter consonants (s, t, k), so the user's words
    are never chopped.
  - It works in either direction (user bassier or brighter than the other voices), whichever the
    measurements show.
  - It's disabled for laptop/webcam mics, where it isn't reliable.
- R1.4 The gate never clips the start of words, even with a high threshold (look-ahead).
- R1.5 The user can **see** the separation:
  - **Gate meter:** labelled zones for room noise, other voices and "you", plus the open/close lines.
  - **Closeness bar:** marks for the user and the other voices, plus the current sound.
  - **Loudness margin:** how much louder than normal other voices can get before they'd open the
    gate on volume alone.
- R1.6 A softer **expander** mode turns quiet sounds down gradually instead of muting them.

## R2. Guided setup

- R2.1 Setup starts with **"Your setup"**:
  - **Mic type:** dynamic, condenser, headset, or laptop/webcam.
  - **Situations (any combination):** *other people talk nearby*, *background noise*,
    *broadcast-quality sound*.

  These choices switch on the right effects and adapt the later steps and placement advice.
- R2.2 Mic placement coaching specific to the mic type: distance, which side to speak into, and
  pointing the mic's insensitive side towards other people.
- R2.3 Short recordings:
  - **Quiet room** (4 s).
  - **Other voices**, optional (10 s). The user stays silent while someone nearby talks naturally.
  - **User talking** (6 s).
- R2.4 Setup **never requires anyone else's cooperation**. Without an other-voices recording, the
  closeness check is set from the user's own voice alone.
- R2.5 The analysis tries many detector bands and picks the one with the largest gap between the
  user's voice and everything else. It then sets the input gain, gate threshold and close margin,
  detector band, closeness check, compressor threshold and makeup gain.
- R2.6 The analysis copes with a near-silent room (for example when AI noise removal runs before the gate).
- R2.7 Results are reported in plain English, with a quality rating, a live test ("when someone else
  talks the light should stay CLOSED") and concrete advice if separation is weak.
- R2.8 Any recording step can be **restarted** while it runs, and **redone on its own** afterwards.
  Each recording is checked automatically, and the results page says which one to redo. The checks
  cover noise during the quiet step, "other voices" that sound like the user, and a user recording
  with no clear speech.
- R2.9 **"Learn other voices"** is available at any time. It records 10 s of nearby voices, adds them
  to the saved recordings and re-tunes, without redoing the full setup.
- R2.10 When the analysis improves in a new version, saved setups are re-tuned automatically from
  the saved recordings.

## R3. Effects

- R3.1 Every effect is optional and **removable**, except Input and Output. Effects are added from an
  **Effect Library** grouped by purpose. Effects always run in a fixed, sensible order, whatever
  order they were added in.
- R3.2 Built-in effects:
  - **Clean up:** AI Noise Removal (RNNoise, adjustable strength), Low Cut, Voice Gate, De-esser.
  - **Shape:** Auto Level (slow level steering that pauses during silence), Compressor,
    Equaliser (parametric, draggable points on a frequency graph), Tone (Bass / Presence / Air),
    Warmth (saturation), Exciter (generated high harmonics).
  - **Space:** Doubler, Echo (with presets), Reverb (with presets).
  - **Fun:** Voice Changer. Pitch, character (formant-like tilt), robot, radio/phone, drive and
    wobble, with one-click presets (e.g. Chipmunk, Deep Voice, Robot, Old Radio, Ghost).
- R3.3 **Third-party plugins.** The user's own **VST3** and **LV2** effects load as a "Plugins" stage
  (up to 8, reorderable, with bypass).
  - Every plugin setting gets generated controls: sliders, dropdowns or switches.
  - VST3 bundles are scanned in a separate helper process, so a faulty plugin can't crash the app.
    A plugin that crashed while loading is disabled on the next start.
  - Plugins run in mono. Stereo plugins receive the voice on both channels and are folded back to mono.
- R3.4 Processing order:
  Input → Noise removal → Low cut → Voice gate → Auto level → Compressor → De-esser →
  Voice changer → Equaliser → Tone → Plugins → Warmth → Exciter → Doubler → Echo → Reverb →
  Output (safety limiter).

## R4. Usability

- R4.1 Plain-English names, summaries and help for every control. Rarely needed controls hide under
  "Advanced". All wording is neutral and makes no assumptions about the user or the people around them.
- R4.2 Live visual feedback for every stage: meters, activity lights and gain reduction.
- R4.3 One window that scales to many effects:
  - a **chain strip** for an overview,
  - a **detail panel** for editing,
  - an always-visible **Live** column with input/output meters, gate state and whether an app is
    using the virtual mic.
- R4.4 **Compare** (hear the raw mic) and **Listen** (hear yourself in headphones).
- R4.5 The user chooses which microphone and which interface channel is processed.
- R4.6 Settings persist automatically. Sound presets can be saved, and built-in presets never undo
  the user's setup or remove effects they've added.
- R4.7 Runs in the system tray. Optional start at login, optionally hidden.

## R5. Technical

- R5.1 Native PipeWire. Provides a virtual source named "MicForge (processed mic)" that any
  application can record from.
- R5.2 Added latency stays low enough for live conversation (target under 30 ms, excluding the
  Voice Changer's pitch shifter and any third-party plugins).
- R5.3 Realtime-safe audio thread: no allocation, locking or blocking. Parameter changes from the UI
  reach the audio thread through atomics or lock-free queues.
- R5.4 A test instance (`MICFORGE_TEST=1`) runs alongside the real one with its own settings,
  recordings and virtual mic, so development and screenshots never interrupt a live call.
- R5.5 Automated tests cover the DSP, the voice-separation analysis and the plugin hosts.

## R6. Distribution

- R6.1 Open source under **GPL-3.0-or-later**. Bundled parts of Steinberg's VST3 SDK are MIT-licensed
  (VST3 SDK 3.8 or later).
- R6.2 Builds with standard Qt 6 tooling. Installs a binary, desktop entry, icon, AppStream metainfo
  and licences.
- R6.3 Packaged for the Arch User Repository as `micforge` (tagged releases) and `micforge-git`
  (latest code). Also installable per-user without root (`install.sh`).
