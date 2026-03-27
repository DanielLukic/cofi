#!/bin/bash
set -e

UNIT_DIR="$HOME/.config/systemd/user"
UNIT_FILE="$UNIT_DIR/cofi.service"

mkdir -p "$UNIT_DIR"
cp "$(dirname "$0")/cofi.service" "$UNIT_FILE"

systemctl --user daemon-reload
systemctl --user enable cofi.service
systemctl --user start cofi.service

echo "cofi systemd user service installed and started."
echo "Check status with: systemctl --user status cofi"
