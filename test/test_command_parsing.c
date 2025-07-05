#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>

// Mock the parse_command_and_arg function from command_mode.c
// This is a copy for testing purposes
static int parse_command_and_arg(const char *input, char *cmd_out, char *arg_out, size_t cmd_size, size_t arg_size) {
    if (!input || !cmd_out || !arg_out) return 0;
    
    // Clear output buffers
    cmd_out[0] = '\0';
    arg_out[0] = '\0';
    
    // Skip leading whitespace
    while (*input == ' ' || *input == '\t') input++;
    
    // Try to parse with space first (backward compatibility)
    if (sscanf(input, "%31s %31s", cmd_out, arg_out) == 2) {
        return 1;
    }
    
    // If no space found, try to parse known commands without spaces
    size_t len = strlen(input);
    
    // Check for 'cw' followed by number (change-workspace)
    if (len >= 3 && strncmp(input, "cw", 2) == 0 && isdigit(input[2])) {
        strncpy(cmd_out, "cw", cmd_size - 1);
        cmd_out[cmd_size - 1] = '\0';
        strncpy(arg_out, input + 2, arg_size - 1);
        arg_out[arg_size - 1] = '\0';
        return 1;
    }
    
    // Check for 'jw' followed by number (jump-workspace)
    if (len >= 3 && strncmp(input, "jw", 2) == 0 && isdigit(input[2])) {
        strncpy(cmd_out, "jw", cmd_size - 1);
        cmd_out[cmd_size - 1] = '\0';
        strncpy(arg_out, input + 2, arg_size - 1);
        arg_out[arg_size - 1] = '\0';
        return 1;
    }
    
    // Check for 'j' followed by number (jump shortcut)
    if (len >= 2 && input[0] == 'j' && isdigit(input[1])) {
        strncpy(cmd_out, "j", cmd_size - 1);
        cmd_out[cmd_size - 1] = '\0';
        strncpy(arg_out, input + 1, arg_size - 1);
        arg_out[arg_size - 1] = '\0';
        return 1;
    }
    
    // Check for 'tw' followed by tiling option
    if (len >= 3 && strncmp(input, "tw", 2) == 0 && 
        (isdigit(input[2]) || strchr("LRTBFClrtbfc", input[2]))) {
        strncpy(cmd_out, "tw", cmd_size - 1);
        cmd_out[cmd_size - 1] = '\0';
        strncpy(arg_out, input + 2, arg_size - 1);
        arg_out[arg_size - 1] = '\0';
        return 1;
    }
    
    // Check for 't' followed by tiling option
    if (len >= 2 && input[0] == 't' && 
        (isdigit(input[1]) || strchr("LRTBFClrtbfc", input[1]))) {
        strncpy(cmd_out, "t", cmd_size - 1);
        cmd_out[cmd_size - 1] = '\0';
        strncpy(arg_out, input + 1, arg_size - 1);
        arg_out[arg_size - 1] = '\0';
        return 1;
    }
    
    // No argument found, just copy the command
    strncpy(cmd_out, input, cmd_size - 1);
    cmd_out[cmd_size - 1] = '\0';
    return 1;
}

// Test structure
typedef struct {
    const char *input;
    const char *expected_cmd;
    const char *expected_arg;
    const char *description;
} TestCase;

// Run a single test
void run_test(const TestCase *test) {
    char cmd[32] = {0};
    char arg[32] = {0};
    
    int result = parse_command_and_arg(test->input, cmd, arg, sizeof(cmd), sizeof(arg));
    
    if (!result) {
        printf("FAIL: %s - Function returned failure\n", test->description);
        return;
    }
    
    if (strcmp(cmd, test->expected_cmd) != 0) {
        printf("FAIL: %s - Expected cmd '%s', got '%s'\n", 
               test->description, test->expected_cmd, cmd);
        return;
    }
    
    if (strcmp(arg, test->expected_arg) != 0) {
        printf("FAIL: %s - Expected arg '%s', got '%s'\n", 
               test->description, test->expected_arg, arg);
        return;
    }
    
    printf("PASS: %s\n", test->description);
}

int main() {
    printf("Testing Command Parsing Without Spaces\n");
    printf("=====================================\n\n");
    
    TestCase tests[] = {
        // Backward compatibility - commands with spaces
        {"cw 2", "cw", "2", "Change workspace with space"},
        {"j 5", "j", "5", "Jump workspace with space"},
        {"t L", "t", "L", "Tile left with space"},
        {"tw R", "tw", "R", "Tile right with space (long form)"},
        
        // New format - commands without spaces
        {"cw2", "cw", "2", "Change workspace without space"},
        {"cw12", "cw", "12", "Change workspace multi-digit"},
        {"j3", "j", "3", "Jump workspace without space"},
        {"j15", "j", "15", "Jump workspace multi-digit"},
        {"jw4", "jw", "4", "Jump workspace long form"},
        
        // Tiling commands without spaces
        {"tL", "t", "L", "Tile left without space"},
        {"tR", "t", "R", "Tile right without space"},
        {"tT", "t", "T", "Tile top without space"},
        {"tB", "t", "B", "Tile bottom without space"},
        {"tF", "t", "F", "Tile fullscreen without space"},
        {"tC", "t", "C", "Tile center without space"},
        {"t5", "t", "5", "Tile grid position 5"},
        {"t9", "t", "9", "Tile grid position 9"},
        
        // Lowercase tiling options
        {"tl", "t", "l", "Tile left lowercase"},
        {"tr", "t", "r", "Tile right lowercase"},
        
        // Long form tiling
        {"twL", "tw", "L", "Tile left long form"},
        {"tw7", "tw", "7", "Tile grid long form"},
        
        // Commands without arguments
        {"tm", "tm", "", "Toggle monitor (no arg)"},
        {"sb", "sb", "", "Skip taskbar (no arg)"},
        {"help", "help", "", "Help command (no arg)"},
        {"c", "c", "", "Close window (no arg)"},
        
        // Edge cases
        {"  cw 3  ", "cw", "3", "Command with leading/trailing spaces"},
        {"t", "t", "", "Tile without argument"},
        {"j", "j", "", "Jump without argument"},
        
        // Invalid cases that should just return the command
        {"cw", "cw", "", "Change workspace without number"},
        {"junk", "junk", "", "Unknown command"},
        {"t!", "t!", "", "Invalid tiling option"},
        
        {NULL, NULL, NULL, NULL} // Sentinel
    };
    
    // Run all tests
    int i = 0;
    int passed = 0;
    int total = 0;
    
    while (tests[i].input != NULL) {
        run_test(&tests[i]);
        
        // Count results
        char cmd[32] = {0};
        char arg[32] = {0};
        parse_command_and_arg(tests[i].input, cmd, arg, sizeof(cmd), sizeof(arg));
        
        if (strcmp(cmd, tests[i].expected_cmd) == 0 && 
            strcmp(arg, tests[i].expected_arg) == 0) {
            passed++;
        }
        
        total++;
        i++;
    }
    
    printf("\n=====================================\n");
    printf("Results: %d/%d tests passed\n", passed, total);
    
    return (passed == total) ? 0 : 1;
}