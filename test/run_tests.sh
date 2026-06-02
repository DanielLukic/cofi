#!/bin/bash
set -u

UNIT_TEST_HOME="$(mktemp -d /tmp/cofi-unit-tests-home.XXXXXX)"
cleanup_unit_test_home() {
    rm -rf "$UNIT_TEST_HOME"
}
trap cleanup_unit_test_home EXIT
export HOME="$UNIT_TEST_HOME"
export TMPDIR="$UNIT_TEST_HOME/tmp"
export XDG_RUNTIME_DIR="$UNIT_TEST_HOME/run"
mkdir -p "$TMPDIR" "$XDG_RUNTIME_DIR"
chmod 700 "$XDG_RUNTIME_DIR"

if [ -z "${TEST_BINARIES:-}" ]; then
    echo "ERROR: TEST_BINARIES is not set"
    exit 1
fi

overall_exit=0

test_needs_host_privileges() {
    case "$1" in
        test_daemon_socket|process-group\ survival\ test)
            return 0
            ;;
        *)
            return 1
            ;;
    esac
}

print_host_privilege_hint() {
    local name="$1"
    if test_needs_host_privileges "$name"; then
        echo "HINT: $name exercises host-level sockets/processes."
        echo "HINT: If this is running under a sandboxed agent, rerun this test unsandboxed before treating it as a code failure."
    fi
}

require_file() {
    local path="$1"
    if [ ! -f "$path" ]; then
        echo "ERROR: expected test artifact is missing: $path"
        exit 1
    fi
}

run_with_optional_xvfb() {
    local cmd="$1"
    local test_exit=0

    if command -v xvfb-run >/dev/null 2>&1; then
        xvfb-run -a "$cmd"
        test_exit=$?
        if [ $test_exit -ne 0 ]; then
            "$cmd"
            test_exit=$?
        fi
    else
        "$cmd"
        test_exit=$?
    fi

    return $test_exit
}

run_test() {
    local name="$1"
    local path="$2"
    local mode="${3:-direct}"

    require_file "$path"
    echo ""
    echo "Running $name..."

    if [ "$mode" = "xvfb" ]; then
        run_with_optional_xvfb "$path"
    else
        "$path"
    fi

    local test_status=$?
    if [ $test_status -eq 77 ]; then
        echo "SKIP: $name reported an environment limitation"
    elif [ $test_status -ne 0 ]; then
        echo "ERROR: $name exited non-zero"
        print_host_privilege_hint "$name"
        overall_exit=1
    fi
}

for test_item in $TEST_BINARIES; do
    case "$test_item" in
        test_detach_survival.sh)
            run_test "process-group survival test" "./$test_item" "direct"
            ;;
        test_main_split_regression|test_key_handler_core|test_command_mode_targeting|test_run_mode|test_cofi_modal|test_run_provider)
            run_test "$test_item" "./$test_item" "xvfb"
            ;;
        *)
            run_test "$test_item" "./$test_item" "direct"
            ;;
    esac
done

exit $overall_exit
