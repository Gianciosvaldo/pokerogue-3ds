#!/usr/bin/env python3
"""Lossless native input/result transport. No expected form choice is computed."""
import collections
import itertools
import json
import sys

r = json.load(open(sys.argv[1]))
assert r['version'] == 1 and r['pin'] == '8555c08c823b856cbec4eb99ca84ea52a955836d'
assert (len(r['enums']), len(r['rules']), len(r['constructors']), len(r['cases'])) == (159, 416, 2544, 124416)
assert len(r['ids']) == len(set(r['ids'])) == 4 and all(0 < n <= 0xffffffff for n in r['ids'])
out = ['FORMITEMS 1 159 416 124416']

def row(tag, values):
    assert all(isinstance(v, (int, bool)) for v in values)
    out.append(tag + ' ' + ' '.join(str(int(v)) for v in values))

row('U', r['ids'])
row('D', [len(r['caught']), *r['caught']])
for e in r['enums']:
    row('E', [e['id']])
for i, v in enumerate(r['rules']):
    c = v['conditions'][0] if v['conditions'] else {}
    row('R', [i, v['species'], v['index'], v['from'], v['to'], v['item'], v['active'],
              v['quiet'], v['defaultForm'], c.get('ability', 0), c.get('caught', 0)])
row('S', [len(r['shadowed']), *r['shadowed']])
seen, counts = set(), collections.Counter()
for i, c in enumerate(r['cases']):
    key = (c['rule'], c['group'], c['shape'], c['mask'], c['override'], c['gender'], c['nature'])
    assert key not in seen
    seen.add(key)
    counts[c['group']] += 1
    assert c['phase'] in ('', 'FormChangePhase', 'QuietFormChangePhase')
    kind = {'': 0, 'FormChangePhase': 1, 'QuietFormChangePhase': 2}[c['phase']]
    row('C', [i, c['rule'], c['group'], c['shape'], c['mask'], c['override'], c['gender'], c['nature'],
              c['baseAbility'], c['ability'], c['onField'], c['shouldApply'], c['eligible'], c['duringActive'],
              c['result'], c['selected'], kind, c['active']])
    assert len(c['mods']) == 2 and all(0 <= len(side) <= 3 for side in c['mods'])
    row('B', [len(c['mods'][0]), len(c['mods'][1])])
    for side in c['mods']:
        for m in side:
            assert len(m) == 3
            row('M', m)
    assert len(c['routes']) in (0, 2)
    row('N', [len(c['routes'])])
    for j, route in enumerate(c['routes']):
        assert route['delayed'] == bool(j) and route['result'] == c['result']
        row('Q', [route['delayed'], route['result'], len(route['levels'])])
        for level in route['levels']:
            row('L', [len(level), *level])
assert counts == {0: 53248, 1: 1536, 2: 12800, 3: 3328, 4: 53248, 5: 256}
out.append('END 159 416 124416')
open(sys.argv[2], 'w').write('\n'.join(out) + '\n')

out = ['FORMCTOR 1 2544']
row('U', r['ids'])
seen = set()
for i, c in enumerate(r['constructors']):
    assert c['key'] in ('FORM_CHANGE_ITEM', 'RARE_FORM_CHANGE_ITEM')
    key = (c['item'], c['key'], c['owner'], c['active'])
    assert key not in seen
    seen.add(key)
    row('C', [i, c['key'] == 'RARE_FORM_CHANGE_ITEM', c['item'], c['owner'], c['active'],
              c['max'], c['matchesClone'], *c['args']])
    for tag, v in [('L', c['clone']), ('V', c['restored'])]:
        row(tag, [v['item'], v['owner'], v['active'], v['stack'], v['virtual'], v['locked']])
assert seen == set(itertools.product([e['id'] for e in r['enums']],
    ['FORM_CHANGE_ITEM', 'RARE_FORM_CHANGE_ITEM'], range(4), [False, True]))
out.append('END 2544')
open(sys.argv[3], 'w').write('\n'.join(out) + '\n')
