#!/usr/bin/env bash
set -euo pipefail

if [[ "$#" -eq 0 ]]; then
    echo "usage: mise run test-target <make-test-target>..." >&2
    exit 2
fi

make "$@"

for target in "$@"; do
    case "$target" in
        *=*) continue ;;
    esac

    if [[ -x "test/$target" ]]; then
        "test/$target"
    elif [[ -x "$target" ]]; then
        "$target"
    else
        echo "built target '$target' but found no executable at test/$target or $target" >&2
        exit 1
    fi
done
