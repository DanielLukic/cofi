#!/usr/bin/env bash
set -euo pipefail

if [[ -n "${CI:-}" ]]; then
    echo "Skipping rules-flags integration tests (CI=${CI})"
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
        "$SCRIPT_DIR/run_rules_flags.sh" --inside-xvfb
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

write_rules_fixture() {
    cat > "$HOME_DIR/.config/cofi/matching.json" <<'JSON'
{
  "next_match_id": 103,
  "match_entries": [
    {
      "match_id": 101,
      "bound_x11_id": 0,
      "custom_name": "",
      "original_title": "*RuleFlagOne*",
      "class_name": "",
      "instance": "",
      "type": "",
      "assigned": true
    },
    {
      "match_id": 102,
      "bound_x11_id": 0,
      "custom_name": "",
      "original_title": "*RuleFlagTwo*",
      "class_name": "",
      "instance": "",
      "type": "",
      "assigned": true
    }
  ]
}
JSON

    cat > "$HOME_DIR/.config/cofi/rules.json" <<'JSON'
{
  "rules": [
    {
      "match_id": 101,
      "pattern": "*RuleFlagOne*",
      "commands": "sb on",
      "run_at_start": false,
      "once": false,
      "new_only": false
    },
    {
      "match_id": 102,
      "pattern": "*RuleFlagTwo*",
      "commands": "ew off",
      "run_at_start": false,
      "once": false,
      "new_only": false
    }
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
    xterm -T "RulesFlags::One" >"$TEST_ROOT/xterm-one.log" 2>&1 &
    pids+=("$!")
    wait_for_window_count 1

    xterm -T "RulesFlags::Two" >"$TEST_ROOT/xterm-two.log" 2>&1 &
    pids+=("$!")
    wait_for_window_count 2
}

wait_for_window_count() {
    local expected="$1"

    for _ in {1..100}; do
        local count
        count="$(wmctrl -l 2>/dev/null | grep -c "RulesFlags::" || true)"
        [[ "$count" -ge "$expected" ]] && return 0
        sleep 0.1
    done
    fail "expected $expected RulesFlags windows in the WM client list"
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

assert_rule_flag_json() {
    local rule_index="$1"
    local field="$2"
    local expected="$3"

    python3 - "$HOME_DIR/.config/cofi/rules.json" "$rule_index" "$field" "$expected" <<'PY'
import json
import sys

path, index, field, expected = sys.argv[1], int(sys.argv[2]), sys.argv[3], sys.argv[4] == "true"
with open(path, encoding="utf-8") as f:
    data = json.load(f)
actual = data["rules"][index].get(field)
if actual is not expected:
    raise SystemExit(f"rule[{index}].{field}: expected {expected}, got {actual}")
PY
}

wait_for_rule_flag_json() {
    local rule_index="$1"
    local field="$2"
    local expected="$3"

    for _ in {1..100}; do
        if assert_rule_flag_json "$rule_index" "$field" "$expected" 2>/dev/null; then
            return 0
        fi
        sleep 0.1
    done
    assert_rule_flag_json "$rule_index" "$field" "$expected"
}

capture_window_image() {
    local output="$1"

    wait_for_window_geometry_stable
    sleep 0.05
    import -window "$cofi_window" "$output"
}

crop_rules_list_region() {
    local input="$1"
    local output="$2"
    local width height crop_height

    read -r width height < <(gm identify -format '%w %h' "$input")
    crop_height=$((height - 90))
    [[ "$width" -gt 0 && "$crop_height" -gt 40 ]] \
        || fail "invalid rules-list crop dimensions for $input: ${width}x${crop_height}"

    # Compare only the rules rows/flag column. The bottom entry caret blinks and
    # would make full-window screenshots nondeterministic.
    gm convert "$input" -crop "${width}x${crop_height}+0+0" +repage "$output"
}

assert_images_equal() {
    local expected="$1"
    local actual="$2"
    local description="$3"
    local expected_crop="$TEST_ROOT/$description-expected-crop.png"
    local actual_crop="$TEST_ROOT/$description-actual-crop.png"

    crop_rules_list_region "$expected" "$expected_crop"
    crop_rules_list_region "$actual" "$actual_crop"

    gm compare -metric MSE -maximum-error 0 "$expected_crop" "$actual_crop" \
        >"$TEST_ROOT/$description-compare.txt" 2>&1 \
        || fail "expected identical rules-list screenshots for $description"
}

assert_images_differ() {
    local before="$1"
    local after="$2"
    local description="$3"
    local before_crop="$TEST_ROOT/$description-before-crop.png"
    local after_crop="$TEST_ROOT/$description-after-crop.png"

    crop_rules_list_region "$before" "$before_crop"
    crop_rules_list_region "$after" "$after_crop"

    if gm compare -metric MSE -maximum-error 0 "$before_crop" "$after_crop" \
        >"$TEST_ROOT/$description-compare.txt" 2>&1; then
        fail "expected rules-list screenshot change for $description"
    fi
}

run_rules_flags_toggle() {
    setup_common_config
    write_rules_fixture
    start_window_manager
    start_test_windows
    launch_cofi
    wait_for_window_enumeration
    wait_for_log_line "USER: Entered command mode" "command mode"
    focus_cofi

    xdotool type --clearmodifiers "rules"
    xdotool key Return
    wait_for_log_line "Switched to Rules tab|Switched to RULES tab" "rules tab"
    wait_for_window_geometry_stable

    # Select the second fixture rule. If refresh resets selection later, the
    # JSON assertions below show the first rule changed instead.
    xdotool key Up
    wait_for_log_line "Selection UP -> provider\\[1\\]" "select second rule"

    local baseline="$TEST_ROOT/rules-flags-baseline.png"
    local once_on="$TEST_ROOT/rules-flags-once-on.png"
    local once_off="$TEST_ROOT/rules-flags-once-off.png"
    local new_only_on="$TEST_ROOT/rules-flags-new-only-on.png"

    capture_window_image "$baseline"

    xdotool key ctrl+o
    wait_for_rule_flag_json 1 once true
    assert_rule_flag_json 0 once false
    capture_window_image "$once_on"
    assert_images_differ "$baseline" "$once_on" "ctrl-o-shows-once-flag"

    xdotool key ctrl+o
    wait_for_rule_flag_json 1 once false
    assert_rule_flag_json 0 once false
    capture_window_image "$once_off"
    assert_images_equal "$baseline" "$once_off" "ctrl-o-clears-once-flag"

    xdotool key ctrl+n
    wait_for_rule_flag_json 1 new_only true
    assert_rule_flag_json 0 new_only false
    capture_window_image "$new_only_on"
    assert_images_differ "$once_off" "$new_only_on" "ctrl-n-shows-new-only-flag"
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
        echo "integration: kept rules_flags artifacts in $TEST_ROOT" >&2
    else
        rm -rf "$TEST_ROOT"
    fi
}
trap cleanup EXIT

run_rules_flags_toggle
echo "integration: PASS: rules_flags_toggle"
