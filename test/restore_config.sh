#!/bin/bash

# Manual config restore utility
# Use this if tests crashed and left a backup file

CONFIG_FILE="$HOME/.config/cofi.json"
BACKUP_FILE="$HOME/.config/cofi.json.test_backup"

echo "=== COFI Config Restore Utility ==="
echo

if [ -f "$BACKUP_FILE" ]; then
    echo "Found backup at: $BACKUP_FILE"
    echo
    
    if [ -f "$CONFIG_FILE" ]; then
        echo "Current config exists at: $CONFIG_FILE"
        echo "This will be replaced with the backup."
    fi
    
    echo
    read -p "Restore config from backup? (y/N) " -n 1 -r
    echo
    
    if [[ $REPLY =~ ^[Yy]$ ]]; then
        rm -f "$CONFIG_FILE"
        mv "$BACKUP_FILE" "$CONFIG_FILE"
        echo "✓ Config restored successfully"
    else
        echo "Restore cancelled"
    fi
else
    echo "No backup found at: $BACKUP_FILE"
    echo "Nothing to restore"
fi