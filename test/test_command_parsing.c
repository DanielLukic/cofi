#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "../src/command_parser.h"

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
    printf("Testing Command Parsing\n");
    printf("=======================\n\n");
    
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
        {"tr4", "t", "r4", "Direct tile right three quarters"},
        {"tl2", "t", "l2", "Direct tile left half"},
        {"tc1", "t", "c1", "Direct tile center narrow"},
        {"  tr4  ", "t", "r4", "Direct tiling with surrounding spaces"},
        
        // Commands without arguments
        {"tm", "tm", "", "Toggle monitor (no arg)"},
        {"sb", "sb", "", "Skip taskbar (no arg)"},
        {"help", "help", "", "Help command (no arg)"},
        {"c", "c", "", "Close window (no arg)"},
        {"mouse away", "mouse", "away", "Multi-word command with spaced arg"},
        {"m show", "m", "show", "Alias command with spaced arg"},
        
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
