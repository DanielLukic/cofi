#!/usr/bin/env bash
set -euo pipefail

PREFIX="${PREFIX:-/usr/local}"
BINDIR="$PREFIX/bin"
INSTALLED="$BINDIR/cofi"
LOCAL_BIN="$(pwd -P)/cofi"

make clean
make

if [[ -L "$INSTALLED" ]] && [[ "$(readlink -f "$INSTALLED")" == "$LOCAL_BIN" ]]; then
    systemctl --user restart cofi
else
    make install PREFIX="$PREFIX"
fi

journalctl --user -u cofi --no-pager -n 5
