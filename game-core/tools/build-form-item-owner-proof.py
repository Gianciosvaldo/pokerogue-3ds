#!/usr/bin/env python3
"""Lossless input/native result transport; no expected menu policy is computed."""
import itertools
import json
import sys

r = json.load(open(sys.argv[1]))
assert r['version'] == 1 and r['pin'] == '8555c08c823b856cbec4eb99ca84ea52a955836d'
assert len(r['ids']) == len(set(r['ids'])) == 3 and all(0 < n <= 0xffffffff for n in r['ids'])
assert len(r['enums']) == len(set(r['enums'])) == 159
assert len(r['rows']) == 416 and len(r['acquisition']) == 66560 and len(r['menu']) == 122112
keys = {'FORM_CHANGE_ITEM': 0, 'RARE_FORM_CHANGE_ITEM': 1}
with open(sys.argv[2], 'w') as output:
    def row(tag, values):
        assert all(isinstance(v, (int, bool)) for v in values)
        output.write(tag+' '+' '.join(str(int(v)) for v in values)+'\n')
    output.write('FORMMENU 1 159 122112\n')
    row('U', r['ids'])
    for item in r['enums']:
        row('E', [item])
    seen = set()
    for i, case in enumerate(r['menu']):
        fields = [case[k] for k in ('species', 'item', 'flags', 'owner', 'enemy', 'foreign', 'reverse')]
        assert tuple(fields) not in seen
        seen.add(tuple(fields))
        row('C', [i, *fields])
        assert len(case['stock']) == 2 and all(len(s) <= 4 for s in case['stock'])
        row('B', [len(s) for s in case['stock']])
        for side in case['stock']:
            for key, item, owner, active, stack, virtual, locked in side:
                row('M', [keys[key], item, owner, active, stack, virtual, locked])
        assert len(case['selected']) <= 4
        row('O', [len(case['selected']), *case['selected']])
    assert seen == set(itertools.product([151, 800], r['enums'], range(16), range(3), [False, True], [False, True], [False, True]))
    output.write('END 159 122112\n')
