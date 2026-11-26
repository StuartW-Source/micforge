#!/usr/bin/env bash
# Build MicForge and install it for the current user into ~/.local (no root needed).
#
# Prefer the AUR package (micforge / micforge-git) on Arch-based systems: a ~/.local install is NOT
# tracked by pacman, so pacman doesn't know MicForge needs rnnoise, lilv, Qt etc. and may remove them
# as "orphans" when you uninstall other software. MicForge still starts if that happens (those features
# are optional at run time), but they stop working. This script warns about that below.
set -euo pipefail
cd "$(dirname "$0")"

if [ -x /usr/bin/micforge ]; then
    echo "Note: a system-wide MicForge (/usr/bin/micforge, e.g. from the AUR) is also installed."
    echo "      The ~/.local copy takes priority in your menu and PATH. Remove one of them to avoid confusion."
fi

./scripts/fetch-vst3sdk.sh

mkdir -p build
cd build
qmake6 PREFIX="$HOME/.local" ../micforge.pro
make -j"$(nproc)"
make install
cd ..

# Let KDE / GNOME pick up the new menu entry and icon.
command -v update-desktop-database >/dev/null && update-desktop-database "$HOME/.local/share/applications" || true
command -v kbuildsycoca6 >/dev/null && kbuildsycoca6 >/dev/null 2>&1 || true

echo
echo "Installed. Launch “MicForge” from the application menu, or run: ~/.local/bin/micforge"

# On pacman systems, check the run-time packages and how they're recorded.
if command -v pacman >/dev/null; then
    missing=() asdep=()
    for p in pipewire qt6-base qt6-declarative qt6-svg rnnoise lilv; do
        if ! pacman -Q "$p" >/dev/null 2>&1; then missing+=("$p")
        elif pacman -Qi "$p" | grep -q "Installed as a dependency"; then asdep+=("$p"); fi
    done
    if [ ${#missing[@]} -gt 0 ]; then
        echo
        echo "Missing packages (features that need them are disabled until installed):"
        echo "    sudo pacman -S --needed ${missing[*]}"
    fi
    if [ ${#asdep[@]} -gt 0 ]; then
        echo
        echo "These packages are only installed as dependencies of other software, so pacman may remove them"
        echo "when that software is uninstalled. To keep them for MicForge:"
        echo "    sudo pacman -D --asexplicit ${asdep[*]}"
    fi
fi
