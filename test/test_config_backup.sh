#!/bin/bash

# Test configuration backup and restore utilities
# This script provides functions to safely backup and restore the cofi config
# during test runs to prevent data loss

CONFIG_FILE="$HOME/.config/cofi.json"
BACKUP_FILE="$HOME/.config/cofi.json.test_backup"

# Backup the current config if it exists
# Never overwrites an existing backup (safety feature)
backup_config() {
    if [ -f "$CONFIG_FILE" ]; then
        if [ -f "$BACKUP_FILE" ]; then
            echo "WARNING: Backup already exists at $BACKUP_FILE"
            echo "Not overwriting to preserve original config"
            return 0
        fi
        
        echo "Backing up config: $CONFIG_FILE -> $BACKUP_FILE"
        cp "$CONFIG_FILE" "$BACKUP_FILE"
        if [ $? -eq 0 ]; then
            echo "✓ Config backed up successfully"
            return 0
        else
            echo "✗ Failed to backup config"
            return 1
        fi
    else
        echo "No config file to backup"
        return 0
    fi
}

# Restore the config from backup
# Deletes the test config and renames backup to original name
restore_config() {
    if [ -f "$BACKUP_FILE" ]; then
        echo "Restoring config from backup..."
        
        # Remove the test-modified config
        if [ -f "$CONFIG_FILE" ]; then
            rm -f "$CONFIG_FILE"
        fi
        
        # Rename backup to original name
        mv "$BACKUP_FILE" "$CONFIG_FILE"
        if [ $? -eq 0 ]; then
            echo "✓ Config restored successfully"
            return 0
        else
            echo "✗ Failed to restore config"
            return 1
        fi
    else
        echo "No backup to restore"
        return 0
    fi
}

# Check if a backup exists (for manual recovery)
check_backup() {
    if [ -f "$BACKUP_FILE" ]; then
        echo "Backup exists at: $BACKUP_FILE"
        echo "To manually restore, run:"
        echo "  mv $BACKUP_FILE $CONFIG_FILE"
        return 0
    else
        echo "No backup found"
        return 1
    fi
}

# Export functions for use in other scripts
export -f backup_config
export -f restore_config
export -f check_backup

# If called directly with an argument, execute that function
if [ $# -eq 1 ]; then
    case "$1" in
        backup)
            backup_config
            ;;
        restore)
            restore_config
            ;;
        check)
            check_backup
            ;;
        *)
            echo "Usage: $0 {backup|restore|check}"
            exit 1
            ;;
    esac
fi