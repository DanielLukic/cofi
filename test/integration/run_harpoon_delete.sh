#!/usr/bin/env bash
set -euo pipefail

if [[ -n "${CI:-}" ]]; then
    echo "Skipping harpoon-delete integration tests (CI=${CI})"
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

for tool in xvfb-run xdotool xterm wmctrl metacity import gm python3; do
    require_tool "$tool"
done

if [[ ! -x "$REPO_ROOT/cofi" ]]; then
    fail "missing executable: $REPO_ROOT/cofi"
fi

if [[ "${1:-}" != "--inside-xvfb" ]]; then
    exec xvfb-run -a \
        --server-args="-screen 0 1280x800x24 -extension GLX" \
        "$SCRIPT_DIR/run_harpoon_delete.sh" --inside-xvfb
fi

cd "$REPO_ROOT"

setup_common_config() {
    mkdir -p "$HOME_DIR/.config/cofi" "$RUNTIME_DIR"
    chmod 700 "$RUNTIME_DIR"

    export LC_ALL=C.utf8
    export LANG=C.utf8
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

write_harpoon_fixture() {
    cat > "$HOME_DIR/.config/cofi/matching.json" <<'JSON'
{
  "next_match_id": 203,
  "match_entries": [
    {
      "match_id": 201,
      "bound_x11_id": 0,
      "custom_name": "",
      "original_title": "*HarpoonDeleteOne*",
      "class_name": "XTerm",
      "instance": "",
      "type": "normal",
      "assigned": true
    },
    {
      "match_id": 202,
      "bound_x11_id": 0,
      "custom_name": "",
      "original_title": "*HarpoonDeleteTwo*",
      "class_name": "XTerm",
      "instance": "",
      "type": "normal",
      "assigned": true
    }
  ]
}
JSON

    cat > "$HOME_DIR/.config/cofi/harpoon.json" <<'JSON'
{
  "slots": [
    {"slot": "1", "tab": "windows", "payload": "201"},
    {"slot": "2", "tab": "windows", "payload": "202"}
  ]
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
    [[ "$wm_ready" -eq 1 ]] || fail "window manager did not become ready"
}

start_test_windows() {
    xterm -T "HarpoonDeleteOne" >"$TEST_ROOT/xterm-one.log" 2>&1 &
    pids+=("$!")
    wait_for_window_count 1

    xterm -T "HarpoonDeleteTwo" >"$TEST_ROOT/xterm-two.log" 2>&1 &
    pids+=("$!")
    wait_for_window_count 2
}

wait_for_window_count() {
    local expected="$1"

    for _ in {1..100}; do
        local count
        count="$(wmctrl -l 2>/dev/null | grep -c "HarpoonDelete" || true)"
        [[ "$count" -ge "$expected" ]] && return 0
        sleep 0.1
    done
    fail "expected $expected HarpoonDelete windows in the WM client list"
}

launch_cofi() {
    ./cofi --command --no-auto-close --log-file "$LOG_FILE" --log-level debug \
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
    fail "cofi window did not appear"
}

stop_cofi() {
    if [[ -n "${cofi_pid:-}" ]]; then
        kill "$cofi_pid" >/dev/null 2>&1 || true
        wait "$cofi_pid" >/dev/null 2>&1 || true
    fi
    cofi_pid=""
    cofi_window=""
}

wait_for_window_enumeration() {
    for _ in {1..100}; do
        if [[ -f "$LOG_FILE" ]] &&
           grep -Eq "Window enumeration completed .*\\(([2-9]|[1-9][0-9]+) windows\\)|Total windows stored: ([2-9]|[1-9][0-9]+)" "$LOG_FILE"; then
            return 0
        fi
        sleep 0.1
    done
    fail "cofi did not enumerate the test windows"
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
    fail "timed out waiting for $description"
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
    fail "cofi window geometry did not stabilize"
}

focus_cofi() {
    wmctrl -a cofi >/dev/null 2>&1 || true
    xdotool windowactivate --sync "$cofi_window"
}

open_harpoon_tab() {
    wait_for_log_line "USER: Entered command mode" "command mode"
    focus_cofi
    xdotool type --clearmodifiers "harpoon"
    xdotool key Return
    wait_for_log_line "Switched to Harpoon tab|Switched to HARPOON tab" "harpoon tab"
    wait_for_window_geometry_stable
}

assert_harpoon_slots() {
    local expected_count="$1"
    local deleted_present="$2"
    local survivor_present="$3"

    python3 - "$HOME_DIR/.config/cofi/harpoon.json" "$expected_count" \
        "$deleted_present" "$survivor_present" <<'PY'
import json
import sys

path, expected_count = sys.argv[1], int(sys.argv[2])
deleted_present, survivor_present = sys.argv[3] == "true", sys.argv[4] == "true"
with open(path, encoding="utf-8") as f:
    data = json.load(f)
slots = [s for s in data.get("slots", []) if s.get("tab") == "windows"]
payloads = {s.get("payload") for s in slots}
if len(slots) != expected_count:
    raise SystemExit(f"expected {expected_count} windows slots, got {len(slots)}")
if ("202" in payloads) is not deleted_present:
    raise SystemExit(f"deleted payload 202 presence mismatch: {payloads}")
if ("201" in payloads) is not survivor_present:
    raise SystemExit(f"survivor payload 201 presence mismatch: {payloads}")
PY
}

assert_matching_ids() {
    local deleted_present="$1"
    local survivor_present="$2"

    python3 - "$HOME_DIR/.config/cofi/matching.json" "$deleted_present" "$survivor_present" <<'PY'
import json
import sys

path = sys.argv[1]
deleted_present, survivor_present = sys.argv[2] == "true", sys.argv[3] == "true"
with open(path, encoding="utf-8") as f:
    data = json.load(f)
ids = {entry.get("match_id") for entry in data.get("match_entries", [])}
if (202 in ids) is not deleted_present:
    raise SystemExit(f"deleted match_id 202 presence mismatch: {ids}")
if (201 in ids) is not survivor_present:
    raise SystemExit(f"survivor match_id 201 presence mismatch: {ids}")
PY
}

wait_for_deleted_state() {
    for _ in {1..100}; do
        if assert_harpoon_slots 1 false true 2>/dev/null &&
           assert_matching_ids false true 2>/dev/null; then
            return 0
        fi
        sleep 0.1
    done
    assert_harpoon_slots 1 false true
    assert_matching_ids false true
}

capture_window_image() {
    local output="$1"

    wait_for_window_geometry_stable
    sleep 0.05
    import -window "$cofi_window" "$output"
}

crop_harpoon_list_region() {
    local input="$1"
    local output="$2"
    local width height crop_height

    read -r width height < <(gm identify -format '%w %h' "$input")
    crop_height=$((height - 90))
    [[ "$width" -gt 0 && "$crop_height" -gt 40 ]] \
        || fail "invalid harpoon-list crop dimensions for $input: ${width}x${crop_height}"

    # Compare only Harpoon rows; the bottom entry caret blinks independently.
    gm convert "$input" -crop "${width}x${crop_height}+0+0" +repage "$output"
}

assert_images_equal() {
    local expected="$1"
    local actual="$2"
    local description="$3"
    local expected_crop="$TEST_ROOT/$description-expected-crop.png"
    local actual_crop="$TEST_ROOT/$description-actual-crop.png"

    crop_harpoon_list_region "$expected" "$expected_crop"
    crop_harpoon_list_region "$actual" "$actual_crop"

    gm compare -metric MSE -maximum-error 0 "$expected_crop" "$actual_crop" \
        >"$TEST_ROOT/$description-compare.txt" 2>&1 \
        || fail "expected identical harpoon-list screenshots for $description"
}

assert_images_differ() {
    local before="$1"
    local after="$2"
    local description="$3"
    local before_crop="$TEST_ROOT/$description-before-crop.png"
    local after_crop="$TEST_ROOT/$description-after-crop.png"

    crop_harpoon_list_region "$before" "$before_crop"
    crop_harpoon_list_region "$after" "$after_crop"

    if gm compare -metric MSE -maximum-error 0 "$before_crop" "$after_crop" \
        >"$TEST_ROOT/$description-compare.txt" 2>&1; then
        fail "expected harpoon-list screenshot change for $description"
    fi
}

run_harpoon_delete_refresh() {
    setup_common_config
    write_harpoon_fixture
    assert_harpoon_slots 2 true true
    assert_matching_ids true true
    start_window_manager
    start_test_windows
    launch_cofi
    wait_for_window_enumeration
    open_harpoon_tab

    # Select the second fixture slot. If delete resets selection to the first
    # row, the JSON assertions below will show the wrong slot survived.
    xdotool key Up
    wait_for_log_line "Selection UP -> provider\\[1\\]" "select second harpoon slot"

    local before_delete="$TEST_ROOT/harpoon-delete-before.png"
    local after_delete="$TEST_ROOT/harpoon-delete-after.png"
    local fresh_after_delete="$TEST_ROOT/harpoon-delete-fresh.png"

    capture_window_image "$before_delete"

    xdotool key ctrl+d
    sleep 0.2
    xdotool key y
    wait_for_log_line "USER: Deleted harpoon assignment for slot 2" "harpoon delete action"
    wait_for_deleted_state
    capture_window_image "$after_delete"
    assert_images_differ "$before_delete" "$after_delete" "deleted-row-disappears"

    stop_cofi
    : > "$LOG_FILE"
    launch_cofi
    wait_for_window_enumeration
    open_harpoon_tab
    capture_window_image "$fresh_after_delete"

    # A stale filtered row renders "(missing)" until a rebuild. Comparing the
    # immediate post-delete view to a fresh rebuilt Harpoon tab catches that.
    assert_images_equal "$fresh_after_delete" "$after_delete" "post-delete-is-rebuilt"
}

TEST_ROOT="$(mktemp -d)"
HOME_DIR="$TEST_ROOT/home"
RUNTIME_DIR="$TEST_ROOT/run"
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
        echo "integration: kept harpoon_delete artifacts in $TEST_ROOT" >&2
    else
        rm -rf "$TEST_ROOT"
    fi
}
trap cleanup EXIT

run_harpoon_delete_refresh
echo "integration: PASS: harpoon_delete_refresh"
