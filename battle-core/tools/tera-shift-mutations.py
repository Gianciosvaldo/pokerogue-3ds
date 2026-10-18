#!/usr/bin/env python3
"""Mutate the Tera Shift port, rebuild and replay the source turns; every mutant must be caught.
Usage (TURNTEST_LOCALES set): tera-shift-mutations.py TURN_TRACE [TURN_TRACE...]"""
from pathlib import Path
import shutil, subprocess, sys, tempfile
root = Path(__file__).resolve().parent.parent
traces = [Path(a).resolve() for a in sys.argv[1:]]
mutants = {
    'closure-missing': ('src/closures_abilities.c', '    {"ability:307", "55c07decbbff", hunger_toggle},                 /* Tera Shift: the same formFunc */\n', ''),
    'formfunc-inverted': ('src/closures_abilities.c', '{"ability:307", "55c07decbbff", hunger_toggle},', '{"ability:307", "55c07decbbff", hunger_stay},'),
    'route-removed': ('src/phases.c', '} else if (mon->species == SPECIES_ID_TERAPAGOS && (', '} else if (false && mon->species == SPECIES_ID_TERAPAGOS && ('),
    'lapse-dropped': ('src/phases.c', "? !key[0] : !strcmp(key, \"stellar\");", "? !key[0] : false;"),
    'ability-trigger-any-form': ('src/phases.c', "? !key[0] : !strcmp(key, \"stellar\");", "? true : !strcmp(key, \"stellar\");"),
    'lapse-from-any-form': ('src/phases.c', "? !key[0] : !strcmp(key, \"stellar\");", "? !key[0] : true;"),
    'to-stellar': ('src/phases.c', 'form_key, "terastal")) target_form = i;', 'form_key, "stellar")) target_form = i;'),
}

with tempfile.TemporaryDirectory(prefix='tera-shift-mutation-') as temp:
    for name, edit in [('intact', None), *mutants.items()]:
        dest = Path(temp) / name
        shutil.copytree(root, dest, ignore=shutil.ignore_patterns('build'))
        if edit:
            f, old, new = edit
            p = dest / f; s = p.read_text()
            assert s.count(old) == 1, (name, old)
            p.write_text(s.replace(old, new, 1))
        b = subprocess.run(['make', '-s', '-j4', '-C', str(dest), 'build/turntest'], capture_output=True, text=True)
        if b.returncode:
            raise SystemExit(f'{name}: build failed\n{b.stdout}{b.stderr}')
        codes = []
        for t in traces:
            r = subprocess.run([sys.executable, str(dest / 'tools/check-tera-shift-turns.py'), str(t), str(dest / 'build/turntest')], capture_output=True, text=True)
            line = next((l for l in r.stdout.splitlines() if l.startswith(t.name)), 'no summary')
            codes.append(r.returncode)
            print(f'{name}: {t.name} exit {r.returncode} ({line})')
        if name == 'intact':
            if any(codes):
                raise SystemExit('intact build does not pass')
            continue
        if not any(codes):
            raise SystemExit(f'{name}: not caught')
    print(f'all {len(mutants)} mutants caught')
