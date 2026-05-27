#include "rules.h"

void rule_toggle_once(Rule *rule) {
    if (!rule) return;
    rule->once = !rule->once;
    rule->applied = 0;
}
