#!/usr/bin/env bash
set -euo pipefail

if [[ -n "${CI:-}" ]]; then
    echo "Skipping provider-height integration tests (CI=${CI})"
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
        "$SCRIPT_DIR/run_provider_height.sh" --inside-xvfb
fi

cd "$REPO_ROOT"

setup_common_config() {
    mkdir -p "$HOME_DIR/.config/cofi" "$HOME_DIR/.config/google-chrome/Default" "$RUNTIME_DIR"
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

    {
        printf '{\n  "next_match_id": 37,\n  "match_entries": [\n'
        local i
        for i in $(seq 1 36); do
            if [[ "$i" -gt 1 ]]; then
                printf ',\n'
            fi
            printf '    {"match_id": %d, "bound_x11_id": 0, "custom_name": "", "original_title": "*ProviderHeightSlot%02d*", "class_name": "XTerm", "instance": "", "type": "normal", "assigned": true}' "$i" "$i"
        done
        printf '\n  ]\n}\n'
    } > "$HOME_DIR/.config/cofi/matching.json"

    {
        printf '{\n  "slots": [\n'
        local i
        for i in $(seq 1 36); do
            if [[ "$i" -gt 1 ]]; then
                printf ',\n'
            fi
            printf '    {"slot": "%d", "tab": "windows", "payload": "%d"}' "$i" "$i"
        done
        printf '\n  ]\n}\n'
    } > "$HOME_DIR/.config/cofi/harpoon.json"

    cat > "$HOME_DIR/.config/google-chrome/Local State" <<'JSON'
{
  "profile": {
    "info_cache": {
      "Default": {
        "name": "Default",
        "user_name": "default@example.test",
        "active_time": 1
      }
    }
  }
}
JSON

    {
        printf '{\n  "roots": {\n'
        printf '    "bookmark_bar": {"type": "folder", "children": [\n'
        local i
        for i in $(seq 1 48); do
            if [[ "$i" -gt 1 ]]; then
                printf ',\n'
            fi
            printf '      {"type": "url", "name": "Provider Height Bookmark %02d", "url": "https://example.test/bookmark-%02d"}' "$i" "$i"
        done
        printf '\n    ]},\n'
        printf '    "other": {"type": "folder", "children": []},\n'
        printf '    "synced": {"type": "folder", "children": []}\n'
        printf '  }\n}\n'
    } > "$HOME_DIR/.config/google-chrome/Default/Bookmarks"
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

start_test_windows() {
    local i
    for i in $(seq 1 28); do
        xterm -T "ProviderHeight::Window$i" -n "ProviderHeight::Window$i" \
            >"$TEST_ROOT/xterm-$i.log" 2>&1 &
        pids+=("$!")
        wait_for_test_window_count "$i"
    done
}

wait_for_test_window_count() {
    local expected="$1"

    for _ in {1..100}; do
        local count
        count="$(wmctrl -l 2>/dev/null | grep -c "ProviderHeight::Window" || true)"
        if [[ "$count" -ge "$expected" ]]; then
            return 0
        fi
        sleep 0.1
    done
    fail "$CASE_NAME: expected $expected ProviderHeight test windows in the WM client list"
}

launch_cofi_windows() {
    COFI_TEST_EXPORT_DISPLAY_METRICS=1 \
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

wait_for_window_geometry_stable() {
    local stable_reads=0
    local prev_dims=""

    for _ in {1..80}; do
        WIDTH=0
        HEIGHT=0
        eval "$(xdotool getwindowgeometry --shell "$cofi_window" 2>/dev/null || true)"
        local dims="${WIDTH}x${HEIGHT}"
        if [[ "$WIDTH" -gt 0 && "$dims" == "$prev_dims" ]]; then
            stable_reads=$((stable_reads + 1))
            [[ "$stable_reads" -ge 5 ]] && return 0
        else
            prev_dims="$dims"
            stable_reads=1
        fi
        sleep 0.1
    done
    fail "$CASE_NAME: cofi window geometry did not stabilize"
}

window_height() {
    WIDTH=0
    HEIGHT=0
    eval "$(xdotool getwindowgeometry --shell "$cofi_window" 2>/dev/null || true)"
    [[ "$HEIGHT" -gt 0 ]] || fail "$CASE_NAME: failed to read cofi window height"
    printf '%s\n' "$HEIGHT"
}

display_metric() {
    local prop="$1"
    local value=""

    for _ in {1..50}; do
        value="$(xprop -id "$cofi_window" "$prop" 2>/dev/null | grep -Eo '[0-9]+$' || true)"
        if [[ -n "$value" ]]; then
            printf '%s\n' "$value"
            return 0
        fi
        sleep 0.1
    done
    fail "$CASE_NAME: missing display metric $prop"
}

focus_cofi() {
    wmctrl -a cofi >/dev/null 2>&1 || true
    xdotool windowactivate --sync "$cofi_window"
}

open_bookmarks_tab() {
    focus_cofi
    xdotool key colon
    wait_for_log_line "USER: Entered command mode" "command mode"
    xdotool type --clearmodifiers "bookmarks"
    xdotool key Return
    wait_for_log_line "Switched to Bookmarks tab|Switched to BOOKMARKS tab" "bookmarks tab"
    wait_for_window_geometry_stable
}

cleanup() {
    local status=$?
    for pid in "${pids[@]:-}"; do
        kill "$pid" >/dev/null 2>&1 || true
    done
    for pid in "${pids[@]:-}"; do
        wait "$pid" >/dev/null 2>&1 || true
    done
    if [[ -z "${COFI_INTEGRATION_KEEP:-}" && -n "${TEST_ROOT:-}" ]]; then
        rm -rf "$TEST_ROOT"
    elif [[ -n "${TEST_ROOT:-}" ]]; then
        echo "integration: kept $TEST_ROOT"
    fi
    exit "$status"
}

run_provider_height_case() {
    CASE_NAME="provider_height"
    TEST_ROOT="$(mktemp -d)"
    HOME_DIR="$TEST_ROOT/home"
    RUNTIME_DIR="$TEST_ROOT/runtime"
    LOG_FILE="$TEST_ROOT/cofi.log"
    pids=()
    trap cleanup EXIT

    setup_common_config
    start_window_manager
    start_test_windows
    launch_cofi_windows
    wait_for_window_enumeration
    wait_for_window_geometry_stable

    local windows_height
    windows_height="$(window_height)"
    local windows_body_lines
    windows_body_lines="$(display_metric "_COFI_TEST_BODY_LINE_COUNT")"
    local windows_body_max
    windows_body_max="$(display_metric "_COFI_TEST_BODY_MAX_LINES")"
    open_bookmarks_tab
    local bookmarks_height
    bookmarks_height="$(window_height)"
    local bookmarks_body_lines
    bookmarks_body_lines="$(display_metric "_COFI_TEST_BODY_LINE_COUNT")"
    local bookmarks_body_max
    bookmarks_body_max="$(display_metric "_COFI_TEST_BODY_MAX_LINES")"

    local delta=$((bookmarks_height - windows_height))
    if (( delta < 0 )); then
        delta=$((0 - delta))
    fi

    echo "provider-height: windows=${windows_height}px bookmarks=${bookmarks_height}px delta=${delta}px"
    echo "provider-height: body lines windows=${windows_body_lines}/${windows_body_max} bookmarks=${bookmarks_body_lines}/${bookmarks_body_max}"
    if (( windows_body_lines > windows_body_max )); then
        fail "$CASE_NAME: Windows body emitted ${windows_body_lines} lines for ${windows_body_max}-line budget"
    fi
    if (( bookmarks_body_lines > bookmarks_body_max )); then
        fail "$CASE_NAME: Bookmarks body emitted ${bookmarks_body_lines} lines for ${bookmarks_body_max}-line budget"
    fi
    if (( delta > 2 )); then
        fail "$CASE_NAME: provider tab height changed by ${delta}px"
    fi
}

run_provider_height_case
