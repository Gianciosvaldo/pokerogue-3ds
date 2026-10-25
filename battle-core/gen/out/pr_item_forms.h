/* Generated from pinned native item-form registrations. */
#ifndef PR_ITEM_FORMS_H
#define PR_ITEM_FORMS_H
#include <stdint.h>
#define PR_ITEM_FORM_RULE_COUNT 416
#define PR_ITEM_FORM_ENUM_COUNT 159
#define PR_ITEM_FORM_ENUM_BOUND 200
#define PR_ITEM_FORM_CAUGHT_COUNT 6
typedef struct {
    uint16_t species, item, ability, caught_species;
    uint8_t from, to, flags, default_form;
} pr_item_form_rule;
/* flags: bit0 item-active predicate; bit1 quiet phase. default_form255=none. */
extern const pr_item_form_rule pr_item_form_rules[PR_ITEM_FORM_RULE_COUNT];
extern const char *const pr_form_item_name_keys[PR_ITEM_FORM_ENUM_BOUND];
extern const uint16_t pr_form_item_caught_species[PR_ITEM_FORM_CAUGHT_COUNT];
#endif
