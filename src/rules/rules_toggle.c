#include "rules/rules.h"

void rule_toggle_once(Rule *rule) {
    if (!rule) return;
    rule->once = !rule->once;
    rule->applied = 0;
}

void rule_toggle_new_only(Rule *rule) {
    if (!rule) return;
    rule->new_only = !rule->new_only;
}
