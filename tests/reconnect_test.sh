#!/usr/bin/env bash
# Audio-engine recovery test: run a private PipeWire daemon on its own socket (the desktop's audio is
# not touched), connect the MICFORGE_TEST instance to it, then kill and restart that daemon.
# MicForge must notice the lost connection, keep running, and reconnect (virtual mic re-created).
#
#   tests/reconnect_test.sh [path/to/micforge]
set -u
BIN=$(readlink -f "${1:-build/micforge}")
SOCK=micforge-reconnect-test-$$
export MICFORGE_TEST=1 QT_QPA_PLATFORM=offscreen PIPEWIRE_REMOTE=$SOCK
LOG="$HOME/.local/state/MicForge/MicForgeTest/micforge.log"
echo 0 > "$HOME/.local/state/MicForge/MicForgeTest/launch-guard"
START=$(stat -c %s "$LOG" 2>/dev/null || echo 0)
newlog() { tail -c +$((START + 1)) "$LOG"; }
wait_log() { for _ in $(seq 1 $(( $2 * 10 ))); do newlog | grep -q "$1" && return 0; sleep 0.1; done; return 1; }
pass=0; fail=0
ok()  { echo "[ OK ] $*"; pass=$((pass + 1)); }
bad() { echo "[FAIL] $*"; fail=$((fail + 1)); }
daemon() { PIPEWIRE_CORE=$SOCK pipewire >/dev/null 2>&1 & PW=$!; sleep 1; }
vmic_present() { PIPEWIRE_REMOTE=$SOCK pw-cli ls Node 2>/dev/null | grep -q 'micforge_test_mic'; }
cleanup() { kill -TERM ${APP:-0} ${PW:-0} 2>/dev/null; sleep 0.5; kill -KILL ${APP:-0} ${PW:-0} 2>/dev/null; }
trap cleanup EXIT

daemon
"$BIN" >/dev/null 2>&1 & APP=$!
if wait_log "connected to PipeWire" 15 && sleep 1 && vmic_present; then ok "connects to PipeWire and creates the virtual mic"; else bad "initial connection"; fi

kill -KILL $PW; wait $PW 2>/dev/null
if wait_log "lost the connection to PipeWire" 10 && kill -0 $APP 2>/dev/null; then ok "notices PipeWire going away and keeps running"; else bad "lost-connection detection"; fi

sleep 2
daemon
if wait_log "connected to PipeWire after" 20 && sleep 1 && vmic_present; then ok "reconnects automatically and re-creates the virtual mic"; else bad "reconnection"; fi

echo
echo "reconnect tests: $pass passed, $fail failed"
newlog | grep -E "audio engine" | tail -6
[ $fail -eq 0 ]
