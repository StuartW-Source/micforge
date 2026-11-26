#!/usr/bin/env bash
# Start-up and lifecycle tests. Runs the MICFORGE_TEST instance (its own settings, lock, log and
# virtual mic) off-screen, so it never disturbs a real MicForge or the desktop.
#
#   tests/startup_test.sh [path/to/micforge]     (default: build/micforge)
#
# Covers: single-instance hand-over, unresponsive instance detection and --replace, stale locks
# after a crash, clean quit on SIGTERM, crash-loop -> automatic safe mode, safe mode never writing
# settings, PipeWire unavailable at start (keeps running and retries).
set -u
BIN=$(readlink -f "${1:-build/micforge}")
export MICFORGE_TEST=1 QT_QPA_PLATFORM=offscreen
LOG="$HOME/.local/state/MicForge/MicForgeTest/micforge.log"
CONF="$HOME/.config/MicForge/MicForgeTest.conf"
GUARD="$HOME/.local/state/MicForge/MicForgeTest/launch-guard"
pass=0; fail=0
ok()   { echo "[ OK ] $*"; pass=$((pass + 1)); }
bad()  { echo "[FAIL] $*"; fail=$((fail + 1)); }
mark() { : > "$LOG.marker"; LOGSTART=$(stat -c %s "$LOG" 2>/dev/null || echo 0); }
newlog() { tail -c +$((LOGSTART + 1)) "$LOG" 2>/dev/null; }
wait_log() { # pattern timeout-seconds
    for _ in $(seq 1 $(( $2 * 10 ))); do newlog | grep -q "$1" && return 0; sleep 0.1; done; return 1
}
cleanup() { pkill -CONT -f "^$BIN" 2>/dev/null; pkill -KILL -f "^$BIN" 2>/dev/null; sleep 0.3; }
trap cleanup EXIT
cleanup
mkdir -p "$(dirname "$LOG")"; touch "$LOG"
echo 0 > "$GUARD"

# 1. Main instance starts; a second launch hands over to it and exits 0.
mark; "$BIN" >/dev/null 2>&1 & A=$!
if wait_log "startup: ready" 15; then ok "main instance starts"; else bad "main instance didn't start"; fi
mark; timeout 20 "$BIN" >/dev/null 2>&1; rc=$?
if [ $rc -eq 0 ] && newlog | grep -q "already running" && kill -0 $A 2>/dev/null; then ok "second launch hands over and exits (rc=0)"; else bad "second launch hand-over (rc=$rc)"; fi

# 2. Frozen main instance: a login launch gives up (rc=3); --replace takes over.
kill -STOP $A
mark; timeout 30 "$BIN" --minimized >/dev/null 2>&1; rc=$?
if [ $rc -eq 3 ] && newlog | grep -q "isn't responding"; then ok "frozen instance detected; login launch exits (rc=3)"; else bad "frozen instance detection (rc=$rc)"; fi
mark; "$BIN" --replace >/dev/null 2>&1 & B=$!
if wait_log "startup: ready" 30 && ! kill -0 $A 2>/dev/null; then ok "--replace terminates the frozen instance and takes over"; else bad "--replace"; fi

# 3. SIGTERM (logout/shutdown) quits cleanly.
mark; kill -TERM $B
if wait_log "MicForge quitting" 10; then ok "SIGTERM -> clean quit"; else bad "SIGTERM didn't quit cleanly"; fi
wait $B 2>/dev/null

# 4. A crashed instance (SIGKILL) leaves a stale lock; the next launch still becomes main.
echo 0 > "$GUARD"
mark; "$BIN" >/dev/null 2>&1 & C=$!; wait_log "startup: ready" 15; kill -KILL $C; wait $C 2>/dev/null
mark; "$BIN" >/dev/null 2>&1 & D=$!
if wait_log "this is the main instance" 15; then ok "stale lock after a crash is recovered"; else bad "stale lock recovery"; fi
wait_log "startup: ready" 15; kill -KILL $D; wait $D 2>/dev/null

# 5. Two launches that died early -> the third starts in safe mode automatically.
mark; "$BIN" >/dev/null 2>&1 & E=$!
if wait_log "starting in safe mode" 15 && wait_log "ready (SAFE MODE)" 15; then ok "crash loop -> automatic safe mode"; else bad "automatic safe mode"; fi

# 6. Safe mode never writes the settings file.
before=$(sha256sum "$CONF" 2>/dev/null | cut -c1-64)
kill -TERM $E; wait $E 2>/dev/null
after=$(sha256sum "$CONF" 2>/dev/null | cut -c1-64)
if [ "$before" = "$after" ]; then ok "safe mode left the settings file untouched"; else bad "safe mode changed the settings file"; fi

# 7. PipeWire unreachable at start: MicForge keeps running and retries instead of failing.
echo 0 > "$GUARD"
mark; PIPEWIRE_REMOTE=micforge-test-no-such-socket "$BIN" >/dev/null 2>&1 & F=$!
if wait_log "will retry" 15 && wait_log "startup: ready" 15 && sleep 3 && kill -0 $F 2>/dev/null; then ok "no PipeWire: starts anyway and keeps retrying"; else bad "no-PipeWire start"; fi
kill -TERM $F; wait $F 2>/dev/null

echo
echo "startup tests: $pass passed, $fail failed"
[ $fail -eq 0 ]
