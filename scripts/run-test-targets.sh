#!/usr/bin/env bash
set -euo pipefail

if [[ "$#" -eq 0 ]]; then
    echo "usage: mise run test-target <make-test-target>..." >&2
    exit 2
fi

make "$@"

test_needs_host_privileges() {
    case "$1" in
        test_daemon_socket|test_detach_survival.sh)
            return 0
            ;;
        *)
            return 1
            ;;
    esac
}

print_host_privilege_hint() {
    local target="$1"
    if test_needs_host_privileges "$target"; then
        echo "HINT: $target exercises host-level sockets/processes." >&2
        echo "HINT: If this is running under a sandboxed agent, rerun this test unsandboxed before treating it as a code failure." >&2
    fi
}

run_built_target() {
    local target="$1"
    local path="$2"

    set +e
    "$path"
    local status=$?
    set -e
    if [[ "$status" -eq 77 ]]; then
        echo "SKIP: $target reported an environment limitation"
        return 0
    fi
    if [[ "$status" -ne 0 ]]; then
        print_host_privilege_hint "$target"
        return "$status"
    fi
    return 0
}

for target in "$@"; do
    case "$target" in
        *=*) continue ;;
    esac

    if [[ -x "test/$target" ]]; then
        run_built_target "$target" "test/$target"
    elif [[ -x "$target" ]]; then
        run_built_target "$target" "$target"
    else
        echo "built target '$target' but found no executable at test/$target or $target" >&2
        exit 1
    fi
done
