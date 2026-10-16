#!/usr/bin/env python3
"""Compile runtime faults against the source observations; reject incomplete traces."""
from pathlib import Path
import argparse
import subprocess

p = argparse.ArgumentParser()
p.add_argument('trace', type=Path)
a = p.parse_args()
root = Path(__file__).resolve().parents[2]
out = root / 'battle-core/build/acupressure-guards'
out.mkdir(parents=True, exist_ok=True)
source = (root / 'battle-core/src/move_effects.c').read_text()
start = source.index('    case PR_AC_AcupressureStatStageChangeAttr:')
end = source.index('    case PR_AC_StatStageChangeAttr:', start)
leaf = source[start:end]
mutants = {
    'missing-handler': '        pr_note_unported(bt, a->cls); return;\n',
    'hp-gate': leaf.replace('        int32_t eligible[7];', '        if (!effect_can_apply(ef, user, target)) return;\n        int32_t eligible[7];'),
    'chance-gate': leaf.replace('        int32_t eligible[7];', '        if (!chance_roll(bt, get_move_chance(bt, ef, user, target, move, false, true))) return;\n        int32_t eligible[7];'),
    'capped-eligible': leaf.replace('< 6)', '<= 6)'),
    'missing-evasion': leaf.replace('stat <= STAT_EVA', 'stat < STAT_EVA'),
    'reversed-order': leaf.replace('stat = STAT_ATK; stat <= STAT_EVA; stat++', 'stat = STAT_EVA; stat >= STAT_ATK; stat--'),
    'global-rng': leaf.replace('pr_battle_rand_seed_int(bt, n, 0)', 'pr_rng_seed_int(&bt->rng, n, 0)'),
    'wrong-source': leaf.replace('&stat, 1, 2, user)', '&stat, 1, 2, target)'),
    'wrong-target': leaf.replace('pr_battler_index(bt, target)', 'pr_battler_index(bt, user)'),
    'wrong-amount': leaf.replace('&stat, 1, 2, user)', '&stat, 1, 1, user)'),
}
flags = ['-O2', '-std=c99', '-ffp-contract=off', '-I'+str(root/'battle-core/include'), '-I'+str(root/'battle-core/gen/out')]
trace = str(a.trace.resolve())
for name, replacement in mutants.items():
    assert replacement != leaf
    src, obj, exe = (out/(name+suffix) for suffix in ['.c', '.o', ''])
    src.write_text(source[:start]+replacement+source[end:])
    subprocess.run(['gcc', *flags, '-c', str(src), '-o', str(obj)], check=True)
    subprocess.run(['gcc', *flags, str(root/'battle-core/tests/acupressure-test.c'), str(obj), str(root/'battle-core/build/libprcore.a'), '-lm', '-o', str(exe)], check=True)
    r = subprocess.run([str(exe), trace], capture_output=True, text=True)
    (out/(name+'.log')).write_text(r.stdout+r.stderr)
    assert r.returncode == 1, (name, r.returncode)
    print(name, 'caught', flush=True)

phase_source = (root/'battle-core/src/phases.c').read_text()
start = phase_source.index("    /* Meloetta's registered")
end = phase_source.index('    pr_pokemon *mon', start)
src, obj, exe = (out/('missing-meloetta-noop'+suffix) for suffix in ['.c', '.o', ''])
src.write_text(phase_source[:start]+phase_source[end:])
subprocess.run(['gcc', *flags, '-c', str(src), '-o', str(obj)], check=True)
subprocess.run(['gcc', *flags, str(root/'battle-core/tests/acupressure-test.c'), str(obj), str(root/'battle-core/build/libprcore.a'), '-lm', '-o', str(exe)], check=True)
r = subprocess.run([str(exe), trace], capture_output=True, text=True)
(out/'missing-meloetta-noop.log').write_text(r.stdout+r.stderr)
assert r.returncode == 1 and '32 failures' in r.stdout
print('missing-meloetta-noop caught', flush=True)

rows = a.trace.read_text().splitlines()
bad = {'missing-footer': rows[:-1], 'extra-tail': rows+['JUNK'], 'wrong-version': ['ACUPRESSURE 2 16384']+rows[1:], 'wrong-footer': rows[:-1]+['ACUPRESSURE_COMPLETE 1']}
for name, prefix, column, value in [
    ('fractional-case', 'C ', 1, '0.5'), ('invalid-user', 'C ', 2, '4'),
    ('invalid-target', 'C ', 3, '-1'), ('invalid-move', 'C ', 4, '65535'),
    ('invalid-hp', 'MON ', 1, '-1'), ('invalid-stage', 'MON ', 2, '7'),
    ('nonfinite-stage', 'MON ', 2, 'nan'), ('invalid-result', 'RESULT ', 1, '2'),
    ('invalid-levels', 'LEVELS ', 1, '0'), ('invalid-source', 'PH ', 2, '4'),
    ('invalid-stat', 'PH ', 3, '0'), ('invalid-amount', 'PH ', 4, '128'),
    ('invalid-self', 'PH ', 5, '2'), ('extra-dynamic', 'DYNAMIC ', 1, '2'),
]:
    index = next(i for i, row in enumerate(rows) if row.startswith(prefix))
    parts = rows[index].split(); parts[column] = value
    mutated = rows.copy(); mutated[index] = ' '.join(parts); bad[name] = mutated
for name, mutated in bad.items():
    path = out/(name+'.txt'); path.write_text('\n'.join(mutated)+'\n')
    r = subprocess.run([str(root/'battle-core/build/acupressure-test'), str(path)], capture_output=True, text=True)
    assert r.returncode == 2, (name, r.returncode)
    print(name, 'rejected', flush=True)
print(f'{len(mutants)+1} compiled mutants caught; {len(bad)} malformed captures rejected')
