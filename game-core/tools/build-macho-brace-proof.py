#!/usr/bin/env python3
"""Transport native observations verbatim; calculate no expected battle behavior."""
import itertools
import json
import sys

r = json.load(open(sys.argv[1]))
assert r['version'] == 1 and r['pin'] == '8555c08c823b856cbec4eb99ca84ea52a955836d'
assert (len(r['rows']), len(r['statsRows']), len(r['owners'])) == (32, 7500, 1152)
capture = json.load(open(sys.argv[2]))
assert capture['version'] == 1 and capture['pin'] == r['pin'] and len(capture['rows']) == 12
out = ['MACHO 1 32 7500 1152 12']

def row(tag, values):
    assert all(isinstance(x, (int, bool)) for x in values), values
    out.append(tag + ' ' + ' '.join(str(int(x)) for x in values))

seen = set()
for i, x in enumerate(r['rows']):
    key = (x['input']['stack'], x['participated'], x['defeated'], x['fainted'])
    assert key not in seen
    seen.add(key)
    row('A', [i, *key, x['level'], x['maxExpLevel']])
    for tag, v in [('B', x['input']), ('E', x['output'])]:
        row(tag, [v['stack'], v['hp'], v['friendship'], *v['stats']])
assert seen == set(itertools.product([0, 1, 49, 50], [False, True], [False, True], [False, True]))
seen = set()
for i, x in enumerate(r['statsRows']):
    key = (x['species'], bool(x['fusion']), x['level'], x['nature'], x['stack'])
    assert key not in seen and x['fusion'] in (0, 151)
    seen.add(key)
    row('S', [i, x['species'], x['fusion'], x['level'], x['nature'], x['stack'], x['virtual'], x['baseAbility'], x['ability'], *x['ivs']])
    row('B', [x['before']['hp'], *x['before']['stats']])
    row('E', [x['after']['hp'], *x['after']['stats']])
assert seen == set(itertools.product([151, 292, 6], [False, True], [1, 5, 100, 1000, 65536], range(25), [1, 2, 3, 24, 25, 26, 47, 48, 49, 50]))

def mons(tag, entries):
    assert len(entries) == 3
    for m in entries:
        row(tag, [m['id'], m['species'], m['fusion'], m['level'], m['nature'], m['baseAbility'], m['ability'], m['onField'], m['hp'], m['friendship'], *m['ivs'], *m['stats']])

def stock(tag, entries):
    row(tag, [len(entries)])
    for m in entries:
        assert m['key'] in ('MYSTERY_ENCOUNTER_MACHO_BRACE', 'SOOTHE_BELL') and 0 <= m['owner'] < 3
        row('I', [m['key'] == 'SOOTHE_BELL', m['owner'], m['stack'], m['virtual']])

seen = set()
for i, x in enumerate(r['owners']):
    key = (x['fused'], x['scenario'], x['mask'], x['stack'], x['defeated'], x['level'])
    assert key not in seen
    seen.add(key)
    row('O', [i, *key, x['expValue'], x['maxExpLevel'], x['enemy']['species'], x['enemy']['form'], x['enemy']['level']])
    mons('B', x['input'])
    stock('J', x['stock'])
    mons('E', x['output'])
    stock('K', x['afterStock'])
    row('Q', [len(x['queue'])])
    for p in x['queue']:
        assert p['name'] in ('ExpPhase', 'ShowPartyExpBarPhase')
        row('P', [p['party'], p['name'] == 'ExpPhase', p['exp']])
assert seen == set(itertools.product([False, True], range(6), range(8), [1, 49, 50], [False, True], [5, 1000]))
seen = set()
for i, x in enumerate(capture['rows']):
    key = (x['stack'], x['level'], x['participated'])
    assert key not in seen and x['victoryQueued'] and x['partyExpUnchanged']
    seen.add(key)
    row('C', [i, *key, x['victoryQueued'], x['partyExpUnchanged']])
    for tag, v in [('B', x['before']), ('E', x['after'])]:
        row(tag, [v['stack'], v['hp'], v['friendship'], *v['stats']])
assert seen == set(itertools.product([1, 49, 50], [5, 1000], [False, True]))
out.append('END 32 7500 1152 12')
open(sys.argv[3], 'w').write('\n'.join(out) + '\n')
