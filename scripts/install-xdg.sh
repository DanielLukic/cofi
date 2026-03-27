#!/bin/bash
set -e

AUTOSTART_DIR="$HOME/.config/autostart"
mkdir -p "$AUTOSTART_DIR"
cp "$(dirname "$0")/cofi.desktop" "$AUTOSTART_DIR/cofi.desktop"

echo "cofi XDG autostart entry installed."
echo "cofi will start automatically on next login."
