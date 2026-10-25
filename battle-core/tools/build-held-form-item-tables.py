#!/usr/bin/env python3
"""Generate item-form registration facts from pinned native objects, not results."""
import json
from pathlib import Path
import re
import sys

data = json.load(open(sys.argv[1]))
assert data['version'] == 1 and data['pin'] == '8555c08c823b856cbec4eb99ca84ea52a955836d'
rules, enums, caught = data['rules'], data['enums'], data['caught']
assert len(rules) == 416 and len({r['species'] for r in rules}) == 137
assert len(enums) == 159 and len({x['id'] for x in enums}) == 159
assert len(caught) == 6 and caught == sorted(set(caught))
names = {}
for e in enums:
    assert isinstance(e['id'], int) and 0 <= e['id'] <= 199 and re.fullmatch(r'[A-Z0-9_]+', e['key'])
    names[e['id']] = 'modifierType:FormChangeItem.' + e['key']
out = Path(sys.argv[2])
out.mkdir(parents=True, exist_ok=True)
header = '''/* Generated from pinned native item-form registrations. */
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
'''
rows = []
previous = -1
for r in rules:
    assert r['species'] >= previous and 0 < r['species'] <= 65535
    previous = r['species']
    assert r['item'] in names and 0 <= r['from'] <= 255 and 0 <= r['to'] <= 255
    assert r['from'] != r['to'] and -1 <= r['defaultForm'] <= 254
    assert len(r['conditions']) <= 1
    c = r['conditions'][0] if r['conditions'] else {}
    assert not set(c) - {'ability', 'caught'} and len(c) <= 1
    ability, dex = c.get('ability', 0), c.get('caught', 0)
    assert ability in (0, 121, 225) and (not dex or dex in caught)
    flags = int(r['active']) | (int(r['quiet']) << 1)
    rows.append('    {' + ', '.join(map(str, [r['species'], r['item'], ability, dex,
        r['from'], r['to'], flags, 255 if r['defaultForm'] < 0 else r['defaultForm']])) + '},')
source = ['/* Generated at upstream ' + data['pin'] + '. */', '#include "pr_item_forms.h"',
          'const pr_item_form_rule pr_item_form_rules[PR_ITEM_FORM_RULE_COUNT] = {', *rows, '};',
          'const uint16_t pr_form_item_caught_species[PR_ITEM_FORM_CAUGHT_COUNT] = {' + ', '.join(map(str, caught)) + '};',
          'const char *const pr_form_item_name_keys[PR_ITEM_FORM_ENUM_BOUND] = {']
source.extend(f'    [{key}] = {json.dumps(value)},' for key, value in sorted(names.items()))
source += ['};', '']
(out/'pr_item_forms.h').write_text(header)
(out/'pr_item_forms.c').write_text('\n'.join(source))
