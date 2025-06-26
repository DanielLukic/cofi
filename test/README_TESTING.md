# COFI Test Suite

## Overview

The COFI test suite includes both unit tests and integration tests. Some tests modify the user's configuration file at `~/.config/cofi.json`, so we have implemented a backup mechanism to protect user data.

## Config Backup System

### How It Works

1. **Before tests run**: The original config is backed up to `~/.config/cofi.json.test_backup`
2. **During tests**: Tests can freely modify `~/.config/cofi.json`
3. **After tests complete**: The test config is deleted and the backup is restored
4. **Safety feature**: If a backup already exists, it won't be overwritten (preserves original data from failed tests)

### Running Tests Safely

```bash
# Run all tests with automatic backup/restore
./test/run_all_tests.sh

# Run individual test scripts (they also backup/restore)
./test/test_harpoon_assignment.sh
./test/test_reassignment.sh
```

### Manual Config Recovery

If tests crash or are interrupted, you may need to manually restore your config:

```bash
# Check if a backup exists
./test/test_config_backup.sh check

# Interactive restore
./test/restore_config.sh

# Manual restore (if scripts don't work)
mv ~/.config/cofi.json.test_backup ~/.config/cofi.json
```

## Test Categories

### Unit Tests
- `test_filter` - Multi-stage filtering tests
- `test_history` - MRU and Alt-Tab logic tests
- `test_fuzzy` - Fuzzy matching algorithm tests
- `test_window_matcher` - Window matching logic tests
- `test_harpoon_integration` - Harpoon assignment tests

### Integration Tests
- `test_harpoon_assignment.sh` - Tests harpoon slot assignment with real windows
- `test_reassignment.sh` - Tests automatic window reassignment
- `test_display_id_bug.sh` - Tests for display ID issues

### Manual Tests
Some integration tests require manual interaction:
- They start the actual cofi application
- They may create test windows (like gnome-terminal)
- They require user input (pressing keys, etc.)

## Best Practices

1. **Always run tests through the test runners** - They handle backup/restore
2. **Check for existing backups** - If you see `cofi.json.test_backup`, a previous test may have failed
3. **Don't run tests in production** - Tests modify your real config
4. **Review logs** - Tests create log files like `cofi_test.log` for debugging

## Troubleshooting

### "Backup already exists" Warning
This means a previous test didn't complete cleanup. Options:
1. Manually restore: `./test/restore_config.sh`
2. Delete both files and start fresh (loses all config)
3. Manually inspect and merge if needed

### Tests Failing
- Check if cofi is already running: `pkill cofi`
- Check for lock files: `rm -f /tmp/cofi.lock`
- Review test logs in the test output

### Config Corruption
The backup system prevents overwriting good backups with bad data:
- First test run: Creates backup of your config
- If test fails: Backup remains intact
- Next test run: Won't overwrite the backup
- This ensures you can always recover your original config