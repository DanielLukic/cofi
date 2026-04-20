#!/bin/bash
set -e

BINDIR="${BINDIR:-$HOME/.local/bin}"
UNIT_DIR="$HOME/.config/systemd/user"
UNIT_FILE="$UNIT_DIR/cofi.service"

mkdir -p "$UNIT_DIR"
sed "s|@BINDIR@|$BINDIR|g" "$(dirname "$0")/cofi.service" > "$UNIT_FILE"

systemctl --user daemon-reload
systemctl --user reenable cofi.service
systemctl --user restart cofi.service

echo "cofi systemd user service installed and started (ExecStart=$BINDIR/cofi)."
echo "Check status with: systemctl --user status cofi"
