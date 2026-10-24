#!/usr/bin/env python3
"""Transport actual Scene.addModifier observations; compute no expected behavior."""
import itertools
import json
import sys

r = json.load(open(sys.argv[1]))
assert r['version'] == 1 and r['pin'] == '8555c08c823b856cbec4eb99ca84ea52a955836d'
assert len(r['rows']) == 416 and len(r['acquisition']) == 66560
assert len(set(r['ids'])) == 3 and all(0 < n <= 0xffffffff for n in r['ids'])
keys = {'FORM_CHANGE_ITEM': 0, 'RARE_FORM_CHANGE_ITEM': 1}
with open(sys.argv[2], 'w') as out:
    def emit(tag, *values):
        assert all(isinstance(v, (int, bool)) for v in values)
        out.write(tag + ' ' + ' '.join(str(int(v)) for v in values) + '\n')
    out.write('FORMACQUIRE 1 416 66560\n')
    emit('U', *r['ids'])
    for i, row in enumerate(r['rows']):
        emit('R', i, *[row[k] for k in ('species', 'from', 'to', 'item')])
    seen = set()
    for i, case in enumerate(r['acquisition']):
        dimensions = tuple(case[k] for k in ('rule', 'owner', 'rare', 'gate', 'duplicate', 'tier'))
        assert dimensions not in seen
        seen.add(dimensions)
        assert case['filter'] is None
        emit('C', i, *dimensions, case['result'])
        emit('B', *[case[k] for k in ('onField', 'ability', 'baseAbility', 'level', 'nature', 'gender', 'hp')])
        assert len(case['stock']) == 2 and not case['stock'][1] and len(case['stock'][0]) == 1
        for key, item, owner, active, stack, virtual, locked in case['stock'][0]:
            emit('M', keys[key], item, owner, active, stack, virtual, locked)
        assert len(case['balls']) == 5
        emit('P', *case['balls'])
        queue = case['queue']
        assert queue[0] == [] and (len(queue) == 1 or len(queue) == 2 and len(queue[1]) == 1)
        phases = [p for level in queue for p in level]
        assert len(phases) <= 1
        selected = phases[0] if phases else None
        kind = {'FormChangePhase': 1, 'MessagePhase': 2}[selected['kind']] if selected else 0
        emit('Q', kind, selected['rule'] if selected else -1)
        text = selected['text'] if selected else ''
        assert (kind == 2) == bool(text)
        out.write('T ' + (text.encode().hex() or '-') + '\n')
    assert seen == set(itertools.product(range(416), [0, 2], [False, True], range(4), [False, True], range(5)))
    out.write('END 416 66560\n')
