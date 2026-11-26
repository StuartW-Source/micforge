# Troubleshooting

## Where to look first
- **Log:** `~/.local/state/MicForge/MicForge/micforge.log` (Settings → Troubleshooting → Open log folder).
- **Journal:** `journalctl --user -b | grep -i micforge` (login and menu launches are logged here too).
- **Start-up status:** `systemctl --user status app-micforge@autostart.service`

## MicForge doesn't start at all
1. Run `micforge` from a terminal and read the first lines.
2. `error while loading shared libraries: …`: a required library is missing.
   - Older versions (before 2026-11-26) needed **rnnoise** and **lilv** to start. Update MicForge, or
     reinstall them: `sudo pacman -S rnnoise lilv`.
   - Qt or PipeWire libraries: `sudo pacman -S --needed qt6-base qt6-declarative qt6-svg pipewire`.
   - If you installed with `install.sh`, pacman doesn't know MicForge needs these packages. Mark them as
     wanted so they aren't removed as orphans: `sudo pacman -D --asexplicit rnnoise lilv`, or install the
     AUR package instead.
3. "MicForge's window couldn't be loaded": part of Qt is missing (usually `qt6-declarative`). The dialog and
   log show details.

## Starting it does nothing (it's already running)
A second launch brings the running window forward. If the running copy is frozen, a manual launch asks to
replace it; from a terminal: `micforge --replace`.

## It keeps crashing, or a plugin breaks it
- After two failed starts in a row, MicForge starts in **safe mode** automatically.
- Start it yourself in safe mode: `micforge --safe-mode` (or Settings → Restart in safe mode).
- Safe mode uses default sound settings, loads no plugins and **saves nothing**, so your configuration is
  untouched. Use "Restart normally" when done. A plugin that crashed while loading is switched off
  automatically next time; remove it from the Plugins stage.

## "Connecting to PipeWire…"
PipeWire isn't reachable yet (early in login, or restarting). MicForge retries automatically. If it stays
like this: `systemctl --user status pipewire wireplumber`.

## Settings look reset
If the settings file couldn't be read, a banner says where a copy was kept. Daily backups are in
`~/.local/state/MicForge/MicForge/backups/`. Quit MicForge, copy a backup over
`~/.config/MicForge/MicForge.conf`, then start it again.

## AI Noise Removal / LV2 plugins "unavailable"
Install the missing package shown in the notice (`rnnoise` or `lilv`) and restart MicForge.
