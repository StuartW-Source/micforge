#!/usr/bin/env bash
# Build MicForge and install it for the current user into ~/.local (no root needed).
# On Arch-based systems you can instead install the AUR package: micforge or micforge-git.
set -euo pipefail
cd "$(dirname "$0")"

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
