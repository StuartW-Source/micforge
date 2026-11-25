#!/usr/bin/env bash
# Fetch the parts of Steinberg's VST3 SDK (MIT licence) that MicForge needs for hosting VST3 plugins.
# In a git checkout this initialises the submodules; otherwise (e.g. a release tarball) it clones them.
set -euo pipefail
cd "$(dirname "$0")/.."

TAG=v3.8.1_build_84
DEST=third_party/vst3sdk

if [ -d .git ] && git config -f .gitmodules --get-regexp '^submodule\..*\.path$' >/dev/null 2>&1; then
    git submodule update --init --depth 1
    exit 0
fi

for part in base pluginterfaces public.sdk; do
    repo="vst3_${part/public.sdk/public_sdk}"
    if [ ! -d "$DEST/$part/.git" ] && [ ! -f "$DEST/$part/LICENSE.txt" ]; then
        git clone --depth 1 --branch "$TAG" "https://github.com/steinbergmedia/$repo.git" "$DEST/$part"
    fi
done
echo "VST3 SDK ready in $DEST"
