#!/bin/bash
set -e

AUTOSTART_DIR="$HOME/.config/autostart"
BINDIR="${BINDIR:-$HOME/.local/bin}"
mkdir -p "$AUTOSTART_DIR"
sed "s|@BINDIR@|$BINDIR|g" "$(dirname "$0")/cofi.desktop" > "$AUTOSTART_DIR/cofi.desktop"

echo "cofi XDG autostart entry installed."
echo "cofi will start automatically on next login."
