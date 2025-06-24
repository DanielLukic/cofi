#!/bin/bash

echo "Test script for verifying that window IDs update correctly after reassignment"
echo "============================================================================"
echo
echo "To test this bug fix:"
echo "1. Run cofi with debug logging: ./cofi_debug.sh"
echo "2. Assign a window to a harpoon slot (press number key 1-9)"
echo "3. Close that window"
echo "4. Open a similar window (same class/instance/type)"
echo "5. The debug log should show 'Automatically reassigned slot X from window 0xOLD to 0xNEW'"
echo "6. The displayed window ID in the list should now show the NEW ID, not the OLD ID"
echo
echo "Before the fix: The displayed ID would still show the old window ID"
echo "After the fix: The displayed ID shows the new window ID from the harpoon slot"
echo
echo "You can also verify by comparing the window ID shown in the list"
echo "with the actual window ID shown in the debug logs."