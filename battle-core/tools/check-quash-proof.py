#!/usr/bin/env python3
"""Compile real Quash faults and reject malformed native observations."""
from pathlib import Path
import argparse
import os
import re
import subprocess

p = argparse.ArgumentParser()
p.add_argument('trace', type=Path)
p.add_argument('turns', type=Path)
a = p.parse_args()
root = Path(__file__).resolve().parents[2]
out = root/'battle-core/build/quash-guards'
out.mkdir(parents=True, exist_ok=True)
assert os.environ.get('TURNTEST_LOCALES')
effect = (root/'battle-core/src/move_effects.c').read_text()
start = effect.index('    case PR_AC_ForceLastAttr:')
end = effect.index('    case PR_AC_SketchAttr:', start)
leaf = effect[start:end]
engine = (root/'battle-core/src/engine.c').read_text()
start_engine = engine.index('void pr_force_move_last(')
end_engine = engine.index('\n/* tryRemovePhase', start_engine)
helper = engine[start_engine:end_engine]
message = '        pr_queue_msg(bt, e, pr_msg1("moveTriggers:forceLast", pr_arg_pokemon(bt, "targetPokemonName", target)));\n'
force = '        pr_force_move_last(e, target);'
faults = {
    'missing-handler': ('move_effects', effect[:start]+effect[end:]),
    'hp-gate': ('move_effects', effect[:start]+leaf.replace('    case PR_AC_ForceLastAttr:', '    case PR_AC_ForceLastAttr:\n        if (!effect_can_apply(ef, user, target)) return;')+effect[end:]),
    'acted-gate': ('move_effects', effect[:start]+leaf.replace('    case PR_AC_ForceLastAttr:', '    case PR_AC_ForceLastAttr:\n        if (target->acted) return;')+effect[end:]),
    'missing-message': ('move_effects', effect[:start]+leaf.replace(message, '')+effect[end:]),
    'wrong-message-target': ('move_effects', effect[:start]+leaf.replace('"targetPokemonName", target', '"targetPokemonName", user')+effect[end:]),
    'wrong-affix': ('move_effects', effect[:start]+leaf.replace('pr_arg_pokemon(', 'pr_arg_pokemon_name(')+effect[end:]),
    'missing-force': ('move_effects', effect[:start]+leaf.replace(force, '')+effect[end:]),
    'force-user': ('move_effects', effect[:start]+leaf.replace(force, '        pr_force_move_last(e, user);')+effect[end:]),
    'current-slot-identity': ('move_effects', effect[:start]+leaf.replace(force, '        for (int j=0;j<e->n_dyn[PH_MOVE];j++) { pr_phase *p=&e->pool[e->dyn[PH_MOVE][j]]; if(p->battler==pr_battler_index(bt,target)) { p->timing=MOVE_PHASE_TIMING_MODIFIER_LAST; break; } }')+effect[end:]),
    'force-first': ('engine', engine[:start_engine]+helper.replace('MOVE_PHASE_TIMING_MODIFIER_LAST', 'MOVE_PHASE_TIMING_MODIFIER_FIRST')+engine[end_engine:]),
    'all-matching-moves': ('engine', engine[:start_engine]+helper.replace('            return;', '            continue;')+engine[end_engine:]),
}
flags = ['-O2', '-std=c99', '-ffp-contract=off', '-I'+str(root/'battle-core/include'), '-I'+str(root/'battle-core/gen/out')]
for name, (file, code) in faults.items():
    assert code != {'engine': engine, 'move_effects': effect}[file]
    src, obj, exe = (out/(name+suffix) for suffix in ['.c', '.o', ''])
    src.write_text(code)
    subprocess.run(['gcc', *flags, '-c', str(src), '-o', str(obj)], check=True)
    subprocess.run(['gcc', *flags, str(root/'battle-core/tests/quash-test.c'), str(obj), str(root/'battle-core/build/libprcore.a'), '-lm', '-o', str(exe)], check=True)
    r = subprocess.run([str(exe), str(a.trace.resolve())], capture_output=True, text=True)
    (out/(name+'.log')).write_text(r.stdout+r.stderr)
    assert r.returncode == 1, (name, r.returncode)
    print(name, 'caught', flush=True)
    if name == 'missing-force':
        turn_exe = out/'missing-force-turns'
        subprocess.run(['gcc', *flags, str(root/'battle-core/tests/doublestest.c'), str(obj), str(root/'battle-core/build/libprcore.a'), '-lm', '-o', str(turn_exe)], check=True)
        r = subprocess.run([str(turn_exe), str(a.turns.resolve())], capture_output=True, text=True)
        (out/'missing-force-turns.log').write_text(r.stdout+r.stderr)
        m = re.search(r'^covered \d+: pass \d+, fail (\d+)', r.stdout, re.M)
        assert r.returncode == 1 and m and int(m[1]) > 0
        print('missing-force owning-turn failures', m[1], flush=True)

rows = a.trace.read_text().splitlines()
bad = {'missing-footer': rows[:-1], 'extra-tail': rows+['JUNK'], 'wrong-version': ['QUASH 2 8192']+rows[1:], 'wrong-footer': rows[:-1]+['QUASH_COMPLETE 1']}
for name, prefix, column, value in [
    ('fractional-case', 'C ', 1, '0.5'), ('invalid-user', 'C ', 2, '4'),
    ('invalid-target', 'C ', 3, '-1'), ('invalid-move-count', 'C ', 4, '9'),
    ('invalid-op-count', 'C ', 5, '0'), ('invalid-swap', 'C ', 6, '4'),
    ('invalid-species', 'MON ', 1, '65535'), ('invalid-form', 'MON ', 2, '255'),
    ('invalid-hp', 'MON ', 3, '-1'), ('invalid-owner', 'MOVE ', 1, '4'),
    ('invalid-acted', 'MON ', 4, '2'),
    ('invalid-timing', 'MOVE ', 2, '3'), ('nonfinite-timing', 'MOVE ', 2, 'nan'),
    ('invalid-result', 'OP ', 2, '0'), ('invalid-levels', 'LEVELS ', 1, '0'),
    ('invalid-dynamic-owner', 'D ', 1, '4'), ('invalid-dynamic-timing', 'D ', 2, '3'),
]:
    index = next(i for i, row in enumerate(rows) if row.startswith(prefix))
    parts = rows[index].split(); parts[column] = value
    mutated = rows.copy(); mutated[index] = ' '.join(parts); bad[name] = mutated
for name, mutated in bad.items():
    path = out/(name+'.txt'); path.write_text('\n'.join(mutated)+'\n')
    r = subprocess.run([str(root/'battle-core/build/quash-test'), str(path)], capture_output=True, text=True)
    assert r.returncode == 2, (name, r.returncode)
    print(name, 'rejected', flush=True)
print(f'{len(faults)} compiled mutants caught; {len(bad)} malformed captures rejected')
