#!/usr/bin/env bash
set -euo pipefail

if [[ -n "${CI:-}" ]]; then
    echo "Skipping window-state integration tests (CI=${CI})"
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

for tool in xvfb-run xdotool xterm wmctrl metacity xprop; do
    require_tool "$tool"
done

if [[ ! -x "$REPO_ROOT/cofi" ]]; then
    fail "missing executable: $REPO_ROOT/cofi"
fi

if [[ "${1:-}" != "--inside-xvfb" ]]; then
    exec xvfb-run -a \
        --server-args="-screen 0 1280x800x24 -extension GLX" \
        "$SCRIPT_DIR/run_window_state.sh" --inside-xvfb
fi

cd "$REPO_ROOT"

setup_common_config() {
    mkdir -p "$HOME_DIR/.config/cofi" "$RUNTIME_DIR" "$PATH_BIN_DIR"
    chmod 700 "$RUNTIME_DIR"

    export LC_ALL=C
    export LANG=C
    export GTK_THEME=Adwaita
    export NO_AT_BRIDGE=1
    export HOME="$HOME_DIR"
    export XDG_RUNTIME_DIR="$RUNTIME_DIR"
    export COFI_DISABLE_SYSTEMD_RUN=1

    cat > "$HOME_DIR/.config/cofi/options.json" <<'JSON'
{
  "options": {
    "close_on_focus_loss": false
  }
}
JSON

    cat > "$HOME_DIR/.config/cofi/hotkeys.json" <<'JSON'
{
  "hotkeys": []
}
JSON
}

start_window_manager() {
    metacity --sm-disable --replace --no-composite --display "$DISPLAY" \
        >"$TEST_ROOT/metacity.log" 2>&1 &
    pids+=("$!")

    local wm_ready=0
    for _ in {1..100}; do
        if wmctrl -m >/dev/null 2>&1; then
            wm_ready=1
            break
        fi
        sleep 0.1
    done
    [[ "$wm_ready" -eq 1 ]] || fail "$CASE_NAME: window manager did not become ready"
}

spawn_target_window() {
    local title="$1"
    local wid=""

    xterm -T "$title" -n "$title" >"$TEST_ROOT/xterm-$title.log" 2>&1 &
    pids+=("$!")

    for _ in {1..100}; do
        wid="$(wmctrl -l 2>/dev/null | awk -v title="$title" 'index($0, title) { print $1; exit }')"
        if [[ -n "$wid" ]]; then
            printf '%s\n' "$wid"
            return 0
        fi
        sleep 0.1
    done

    fail "$CASE_NAME: expected target window '$title' in the WM client list"
}

launch_cofi_windows() {
    env PATH="$PATH_BIN_DIR:/usr/bin:/bin" \
        ./cofi --windows --no-auto-close --log-file "$LOG_FILE" --log-level debug \
        >"$TEST_ROOT/cofi.stderr" 2>&1 &
    cofi_pid="$!"
    pids+=("$cofi_pid")

    cofi_window=""
    for _ in {1..100}; do
        local best_area=0
        local candidate
        for candidate in $(xdotool search --name "cofi" 2>/dev/null || true); do
            WIDTH=0
            HEIGHT=0
            eval "$(xdotool getwindowgeometry --shell "$candidate" 2>/dev/null || true)"
            local area=$((WIDTH * HEIGHT))
            if [[ "$area" -gt "$best_area" ]]; then
                best_area="$area"
                cofi_window="$candidate"
            fi
        done
        [[ -n "$cofi_window" ]] && return 0
        sleep 0.1
    done

    fail "$CASE_NAME: cofi window did not appear"
}

stop_cofi() {
    if [[ -n "${cofi_pid:-}" ]]; then
        kill "$cofi_pid" >/dev/null 2>&1 || true
        wait "$cofi_pid" >/dev/null 2>&1 || true
        cofi_pid=""
        cofi_window=""
    fi
}

wait_for_window_enumeration() {
    for _ in {1..100}; do
        if [[ -f "$LOG_FILE" ]] &&
           grep -Eq "Window enumeration completed .*\\(([1-9][0-9]*) windows\\)|Total windows stored: ([1-9][0-9]*)" "$LOG_FILE"; then
            return 0
        fi
        sleep 0.1
    done
    fail "$CASE_NAME: cofi did not enumerate the target window"
}

wait_for_log_line() {
    local pattern="$1"
    local description="$2"

    for _ in {1..100}; do
        if [[ -f "$LOG_FILE" ]] && grep -Eq "$pattern" "$LOG_FILE"; then
            return 0
        fi
        sleep 0.1
    done
    fail "$CASE_NAME: timed out waiting for $description"
}

wait_for_log_literal() {
    local text="$1"
    local description="$2"

    for _ in {1..100}; do
        if [[ -f "$LOG_FILE" ]] && grep -Fq "$text" "$LOG_FILE"; then
            return 0
        fi
        sleep 0.1
    done
    fail "$CASE_NAME: timed out waiting for $description"
}

focus_cofi() {
    wmctrl -a cofi >/dev/null 2>&1 || true
    xdotool windowactivate --sync "$cofi_window"
}

query_window_state() {
    local wid="$1"
    local atom="$2"

    xprop -id "$wid" _NET_WM_STATE 2>/dev/null | grep -Fq "$atom"
}

wait_for_window_state() {
    local wid="$1"
    local atom="$2"
    local expected="$3"
    local timeout="${4:-5}"
    local attempts=$((timeout * 10))

    for _ in $(seq 1 "$attempts"); do
        if query_window_state "$wid" "$atom"; then
            [[ "$expected" == "1" ]] && return 0
        else
            [[ "$expected" == "0" ]] && return 0
        fi
        sleep 0.1
    done

    return 1
}

dump_state_failure() {
    local wid="$1"
    local atom="$2"
    local expected="$3"

    {
        echo "window id: $wid"
        echo "atom: $atom"
        echo "expected present: $expected"
        printf "actual present: "
        if query_window_state "$wid" "$atom"; then
            echo 1
        else
            echo 0
        fi
        echo "raw _NET_WM_STATE:"
        xprop -id "$wid" _NET_WM_STATE 2>&1 || true
        echo "cofi.log tail:"
        tail -30 "$LOG_FILE" 2>/dev/null || true
    } >&2
}

assert_window_state() {
    local wid="$1"
    local atom="$2"
    local expected="$3"

    if ! wait_for_window_state "$wid" "$atom" "$expected" 5; then
        dump_state_failure "$wid" "$atom" "$expected"
        fail "$CASE_NAME: expected $atom present=$expected for window $wid"
    fi
}

run_window_command_against_title() {
    local title="$1"
    local command="$2"

    : > "$LOG_FILE"
    launch_cofi_windows
    wait_for_window_enumeration
    focus_cofi

    xdotool type --clearmodifiers "$title"
    wait_for_log_line "USER: Filter text changed -> '$title'" "filter '$title'"

    xdotool key colon
    wait_for_log_line "USER: Entered command mode" "command mode"
    xdotool type --clearmodifiers "$command"
    xdotool key Return
    wait_for_log_literal "USER: Executing command: '$command'" "command '$command'"
    stop_cofi
}

run_mw_plus_sets_both_atoms() {
    setup_common_config
    start_window_manager
    local wid
    wid="$(spawn_target_window "TestMaximizePlus")"

    run_window_command_against_title "TestMaximizePlus" "mw+"

    assert_window_state "$wid" "_NET_WM_STATE_MAXIMIZED_HORZ" 1
    assert_window_state "$wid" "_NET_WM_STATE_MAXIMIZED_VERT" 1
}

run_mw_minus_clears_both_atoms() {
    setup_common_config
    start_window_manager
    local wid
    wid="$(spawn_target_window "TestMaximizeMinus")"

    run_window_command_against_title "TestMaximizeMinus" "mw+"
    assert_window_state "$wid" "_NET_WM_STATE_MAXIMIZED_HORZ" 1
    assert_window_state "$wid" "_NET_WM_STATE_MAXIMIZED_VERT" 1

    run_window_command_against_title "TestMaximizeMinus" "mw-"
    assert_window_state "$wid" "_NET_WM_STATE_MAXIMIZED_HORZ" 0
    assert_window_state "$wid" "_NET_WM_STATE_MAXIMIZED_VERT" 0
}

run_mw_toggle() {
    setup_common_config
    start_window_manager
    local wid
    wid="$(spawn_target_window "TestMaximizeToggle")"

    assert_window_state "$wid" "_NET_WM_STATE_MAXIMIZED_HORZ" 0
    assert_window_state "$wid" "_NET_WM_STATE_MAXIMIZED_VERT" 0

    run_window_command_against_title "TestMaximizeToggle" "mw"
    assert_window_state "$wid" "_NET_WM_STATE_MAXIMIZED_HORZ" 1
    assert_window_state "$wid" "_NET_WM_STATE_MAXIMIZED_VERT" 1

    run_window_command_against_title "TestMaximizeToggle" "mw"
    assert_window_state "$wid" "_NET_WM_STATE_MAXIMIZED_HORZ" 0
    assert_window_state "$wid" "_NET_WM_STATE_MAXIMIZED_VERT" 0
}

run_case() (
    CASE_NAME="$1"
    TEST_ROOT="$(mktemp -d)"
    HOME_DIR="$TEST_ROOT/home"
    RUNTIME_DIR="$TEST_ROOT/run"
    PATH_BIN_DIR="$TEST_ROOT/path-bin"
    LOG_FILE="$TEST_ROOT/cofi.log"
    pids=()
    cofi_pid=""
    cofi_window=""

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
            echo "integration: kept $CASE_NAME artifacts in $TEST_ROOT" >&2
        else
            rm -rf "$TEST_ROOT"
        fi
    }
    trap cleanup EXIT

    "run_$CASE_NAME"
    echo "integration: PASS: window_state_$CASE_NAME"
)

run_selected_case() {
    local case_name="$1"
    local public_name="window_state_$case_name"
    if [[ -n "${COFI_INTEGRATION_CASES:-}" ]]; then
        case " $COFI_INTEGRATION_CASES " in
            *" $case_name "*|*" $public_name "*) ;;
            *) return 0 ;;
        esac
    fi
    run_case "$case_name"
}

run_selected_case mw_plus_sets_both_atoms
run_selected_case mw_minus_clears_both_atoms
run_selected_case mw_toggle
