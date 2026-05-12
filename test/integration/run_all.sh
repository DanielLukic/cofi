#!/usr/bin/env bash
set -euo pipefail

if [[ -n "${CI:-}" ]]; then
    echo "Skipping integration tests (CI=${CI})"
    exit 0
fi

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

fail() {
    echo "integration: FAIL: $*" >&2
    exit 1
}

require_tool() {
    command -v "$1" >/dev/null 2>&1 || fail "missing required tool: $1"
}

for tool in xvfb-run xdotool xterm wmctrl metacity import gm; do
    require_tool "$tool"
done

if [[ ! -x "$REPO_ROOT/cofi" ]]; then
    fail "missing executable: $REPO_ROOT/cofi"
fi

if [[ "${1:-}" != "--inside-xvfb" ]]; then
    exec xvfb-run -a \
        --server-args="-screen 0 1280x800x24 -extension GLX" \
        "$SCRIPT_DIR/run_all.sh" --inside-xvfb
fi

cd "$REPO_ROOT"

TEST_ROOT="$(mktemp -d)"
HOME_DIR="$TEST_ROOT/home"
RUNTIME_DIR="$TEST_ROOT/run"
PATH_BIN_DIR="$TEST_ROOT/path-bin"
LOG_FILE="$TEST_ROOT/cofi.log"
WINDOW_SCREENSHOT_FILE="$TEST_ROOT/cofi-window.png"
ROOT_SCREENSHOT_FILE="$TEST_ROOT/cofi-root.png"
DIFF_SCREENSHOT_FILE="$TEST_ROOT/cofi-window-diff.png"
FIXTURE_DIR="$REPO_ROOT/test/integration/fixtures"
EXPECTED_SCREENSHOT_FILE="$FIXTURE_DIR/apps-path-bin.png"
MAX_SCREENSHOT_ERROR="${COFI_INTEGRATION_MAX_ERROR:-0}"

pids=()

cleanup() {
    local pid
    set +e
    for pid in "${pids[@]:-}"; do
        kill "$pid" >/dev/null 2>&1 || true
    done
    for pid in "${pids[@]:-}"; do
        wait "$pid" >/dev/null 2>&1 || true
    done
    if [[ -n "${COFI_INTEGRATION_KEEP:-}" ]]; then
        echo "integration: kept artifacts in $TEST_ROOT" >&2
    else
        rm -rf "$TEST_ROOT"
    fi
}
trap cleanup EXIT

mkdir -p "$HOME_DIR/.config/cofi" "$RUNTIME_DIR" "$PATH_BIN_DIR"
chmod 700 "$RUNTIME_DIR"

export LC_ALL=C
export LANG=C
export GTK_THEME=Adwaita
export NO_AT_BRIDGE=1

cat > "$HOME_DIR/.config/cofi/options.json" <<'JSON'
{
  "options": {
    "close_on_focus_loss": false
  }
}
JSON

# Avoid test-WM hotkey conflicts while keeping all state under the temp HOME.
cat > "$HOME_DIR/.config/cofi/hotkeys.json" <<'JSON'
{
  "hotkeys": []
}
JSON

for i in $(seq -w 1 700); do
    bin="$PATH_BIN_DIR/cofi-path-bin-$i"
    printf '#!/usr/bin/env sh\nexit 0\n' > "$bin"
    chmod +x "$bin"
done

export HOME="$HOME_DIR"
export XDG_RUNTIME_DIR="$RUNTIME_DIR"

metacity --sm-disable --replace --no-composite --display "$DISPLAY" \
    >"$TEST_ROOT/metacity.log" 2>&1 &
pids+=("$!")

wm_ready=0
for _ in {1..100}; do
    if wmctrl -m >/dev/null 2>&1; then
        wm_ready=1
        break
    fi
    sleep 0.1
done
[[ "$wm_ready" -eq 1 ]] || fail "window manager did not become ready"

xterm -T "TestWindow::One" >"$TEST_ROOT/xterm-one.log" 2>&1 &
pids+=("$!")
xterm -T "TestWindow::Two" >"$TEST_ROOT/xterm-two.log" 2>&1 &
pids+=("$!")

windows_ready=0
for _ in {1..100}; do
    count="$(wmctrl -l 2>/dev/null | grep -c "TestWindow::" || true)"
    if [[ "$count" -ge 2 ]]; then
        windows_ready=1
        break
    fi
    sleep 0.1
done
[[ "$windows_ready" -eq 1 ]] || fail "test windows did not appear in the WM client list"

env PATH="$PATH_BIN_DIR" \
    ./cofi --windows --no-auto-close --log-file "$LOG_FILE" --log-level debug \
    >"$TEST_ROOT/cofi.stderr" 2>&1 &
cofi_pid="$!"
pids+=("$cofi_pid")

cofi_window=""
for _ in {1..100}; do
    best_area=0
    for candidate in $(xdotool search --name "cofi" 2>/dev/null || true); do
        WIDTH=0
        HEIGHT=0
        eval "$(xdotool getwindowgeometry --shell "$candidate" 2>/dev/null || true)"
        area=$((WIDTH * HEIGHT))
        if [[ "$area" -gt "$best_area" ]]; then
            best_area="$area"
            cofi_window="$candidate"
        fi
    done
    if [[ -n "$cofi_window" ]]; then
        break
    fi
    sleep 0.1
done
[[ -n "$cofi_window" ]] || fail "cofi window did not appear"

for _ in {1..100}; do
    if [[ -f "$LOG_FILE" ]] &&
       grep -Eq "Window enumeration completed .*\\([2-9][0-9]* windows\\)|Total windows stored: [2-9][0-9]*" "$LOG_FILE"; then
        break
    fi
    sleep 0.1
done
grep -Eq "Window enumeration completed .*\\([2-9][0-9]* windows\\)|Total windows stored: [2-9][0-9]*" "$LOG_FILE" \
    || fail "cofi did not enumerate the test windows"

wmctrl -a cofi >/dev/null 2>&1 || true
xdotool windowactivate --sync "$cofi_window"
{
    echo "cofi_window=$cofi_window"
    xdotool getwindowgeometry "$cofi_window" || true
    printf "focus_before="
    xdotool getwindowfocus getwindowname || true
} > "$TEST_ROOT/xdotool.log" 2>&1
xdotool type --clearmodifiers '$bin'
{
    printf "focus_after="
    xdotool getwindowfocus getwindowname || true
} >> "$TEST_ROOT/xdotool.log" 2>&1

path_mode_ready=0
for _ in {1..150}; do
    if [[ -f "$LOG_FILE" ]] &&
       grep -q "path_binaries_filter 'bin'" "$LOG_FILE" &&
       grep -q "PATH scan:" "$LOG_FILE"; then
        path_mode_ready=1
        break
    fi
    sleep 0.1
done
[[ "$path_mode_ready" -eq 1 ]] || fail "typing '$' did not trigger Apps PATH filtering"

kill -0 "$cofi_pid" >/dev/null 2>&1 || fail "cofi exited during PATH filtering"

import -window "$cofi_window" "$WINDOW_SCREENSHOT_FILE"

if [[ -n "${COFI_INTEGRATION_BLESS:-}" ]]; then
    mkdir -p "$FIXTURE_DIR"
    cp "$WINDOW_SCREENSHOT_FILE" "$EXPECTED_SCREENSHOT_FILE"
    echo "integration: blessed $EXPECTED_SCREENSHOT_FILE"
elif [[ ! -f "$EXPECTED_SCREENSHOT_FILE" ]]; then
    fail "missing screenshot fixture: $EXPECTED_SCREENSHOT_FILE (run COFI_INTEGRATION_BLESS=1 make test-integration)"
else
    expected_size="$(gm identify -format '%wx%h' "$EXPECTED_SCREENSHOT_FILE")"
    actual_size="$(gm identify -format '%wx%h' "$WINDOW_SCREENSHOT_FILE")"
    [[ "$actual_size" == "$expected_size" ]] \
        || fail "screenshot size mismatch: expected $expected_size, got $actual_size"

    if ! gm compare -metric MSE -maximum-error "$MAX_SCREENSHOT_ERROR" \
        "$EXPECTED_SCREENSHOT_FILE" "$WINDOW_SCREENSHOT_FILE" \
        >"$TEST_ROOT/cofi-window-compare.txt" 2>&1; then
        gm compare -metric MSE -file "$DIFF_SCREENSHOT_FILE" \
            "$EXPECTED_SCREENSHOT_FILE" "$WINDOW_SCREENSHOT_FILE" \
            >"$TEST_ROOT/cofi-window-compare.txt" 2>&1 || true
        fail "screenshot differs from fixture (actual: $WINDOW_SCREENSHOT_FILE, diff: $DIFF_SCREENSHOT_FILE)"
    fi
fi

if [[ -n "${COFI_INTEGRATION_KEEP:-}" && -n "${COFI_INTEGRATION_CAPTURE_ROOT:-}" ]]; then
    import -window root "$ROOT_SCREENSHOT_FILE"
fi

echo "integration: PASS: wm-backed '$' PATH smoke"
