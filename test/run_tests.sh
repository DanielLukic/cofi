#!/bin/bash
set -u

if [ -z "${TEST_BINARIES:-}" ]; then
    echo "ERROR: TEST_BINARIES is not set"
    exit 1
fi

overall_exit=0

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

    if [ $? -ne 0 ]; then
        echo "ERROR: $name exited non-zero"
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
