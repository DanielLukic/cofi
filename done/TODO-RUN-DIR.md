# TODO: Switch to XDG_RUNTIME_DIR for Lock Files

## Overview

Switch from using `/tmp/cofi.lock` to `$XDG_RUNTIME_DIR/cofi.lock` for better security, automatic cleanup, and proper multi-user support.

## Current Situation

- Lock file: `/tmp/cofi.lock`
- Issues:
  - World-readable in `/tmp` (security concern)
  - No automatic cleanup on logout
  - Potential conflicts in multi-user systems
  - Not following XDG Base Directory specification

## Proposed Changes

### 1. Update Lock File Path

**File**: `src/instance.c`

Replace hardcoded `/tmp/cofi.lock` with a function that:
1. Checks for `$XDG_RUNTIME_DIR` environment variable
2. Falls back to `/tmp/cofi-$UID.lock` if not available
3. Returns the appropriate path

```c
const char* get_lock_file_path() {
    static char path[PATH_MAX];
    const char* runtime_dir = getenv("XDG_RUNTIME_DIR");
    
    if (runtime_dir && access(runtime_dir, W_OK) == 0) {
        // Preferred: Use XDG_RUNTIME_DIR
        snprintf(path, sizeof(path), "%s/cofi.lock", runtime_dir);
    } else {
        // Fallback: Use /tmp with user-specific name
        snprintf(path, sizeof(path), "/tmp/cofi-%d.lock", getuid());
    }
    
    return path;
}
```

### 2. Update All Lock File References

Search and replace all occurrences of:
- `"/tmp/cofi.lock"` → `get_lock_file_path()`

Affected functions in `src/instance.c`:
- `acquire_lock()`
- `release_lock()`  
- `get_running_pid()`
- `is_instance_running()`

### 3. Add Required Headers

Add to `src/instance.c`:
```c
#include <unistd.h>    // for getuid(), access()
#include <limits.h>    // for PATH_MAX
```

## Benefits

1. **Security**: Lock file only readable by owner (700 permissions in runtime dir)
2. **Cleanup**: Automatically removed when user logs out
3. **Multi-user**: Each user gets their own lock file
4. **Standards**: Follows XDG Base Directory specification
5. **Performance**: Runtime dir uses tmpfs (in-memory)

## Testing

1. Test with `XDG_RUNTIME_DIR` set (normal case)
2. Test with `XDG_RUNTIME_DIR` unset (fallback)
3. Test multi-user scenario:
   - User A runs cofi
   - User B runs cofi
   - Verify no conflicts
4. Test cleanup:
   - Run cofi
   - Log out
   - Verify lock file is gone
5. Test single instance behavior still works:
   - Run cofi
   - Try to run second instance
   - Verify SIGUSR1 is sent to first

## Implementation Order

1. Implement this change first (before workspace switching)
2. Test thoroughly
3. Commit with message like: "Use XDG_RUNTIME_DIR for lock files"
4. Then proceed with workspace switching feature

## Future Considerations

When implementing workspace switching (IPC file), use the same approach:
- `$XDG_RUNTIME_DIR/cofi.mode` instead of `/tmp/cofi.mode`
- Same fallback pattern with UID

## Notes

- Keep change minimal - only touch instance.c
- Preserve all existing functionality
- Small, focused commit
- This is a foundation for better IPC in workspace switching feature