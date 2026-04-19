#!/bin/bash
# Test: a process launched via fork+setsid+double-fork survives in a separate process group

set -e
TMPDIR_LOCAL=$(mktemp -d)
trap 'rm -rf "$TMPDIR_LOCAL"' EXIT

SENTINEL="$TMPDIR_LOCAL/launched.pid"

# Use the pre-built test binary
cd "$(dirname "$0")/.."
BINARY="./test/test_detach_survival_bin"

if [ ! -f "$BINARY" ]; then
    echo "SKIP: test_detach_survival_bin not built"
    exit 0
fi

"$BINARY" "$SENTINEL"
sleep 0.2

# Check process launched and sentinel written
if [ ! -f "$SENTINEL" ]; then
    echo "FAIL: sentinel not written — process was not launched"
    exit 1
fi

PID=$(cat "$SENTINEL")
if ! kill -0 "$PID" 2>/dev/null; then
    echo "FAIL: process $PID is not running"
    exit 1
fi

MY_PGID=$(ps -o pgid= -p $$ | tr -d ' ')
PROC_PGID=$(ps -o pgid= -p "$PID" 2>/dev/null | tr -d ' ')

if [ "$PROC_PGID" = "$MY_PGID" ]; then
    echo "FAIL: process $PID is in our process group ($MY_PGID) — not detached"
    kill "$PID" 2>/dev/null
    exit 1
fi

echo "PASS: process $PID running in separate pgroup $PROC_PGID (our pgroup: $MY_PGID)"
kill "$PID" 2>/dev/null || true
exit 0
