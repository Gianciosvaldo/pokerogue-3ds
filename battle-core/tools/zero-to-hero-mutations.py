#!/usr/bin/env python3
"""Mutate the Zero to Hero port, rebuild and replay the source evidence; every mutant must be caught.
Usage (TURNTEST_LOCALES set): zero-to-hero-mutations.py API_TRACE TURN_TRACE"""
from pathlib import Path
import shutil, subprocess, sys, tempfile
root = Path(__file__).resolve().parent.parent
api, turns = (Path(a).resolve() for a in sys.argv[1:3])
mutants = {
    'faint-ignored': ('src/closures_abilities.c', 'return a->hp <= 0 ? a->form : 1;', 'return 1;'),
    'switch-out-gate-dropped': ('src/ab_attrs.c', 'GATE(pr_closure_call(bt, P(PreSwitchOutFormChangeAbAttr, a)->formFunc, a->cls, p->form, p, NULL, -1) != p->form);', 'GATE(1);'),
    'simulated-form-change': ('src/ab_attrs.c', 'NULL, -1) != p->form);\n        if (!pp->simulated) pr_form_change(bt, p, PR_FCT_SpeciesFormChangeAbilityTrigger);\n        return 1;\n    case PR_AC_IncreasePpUsedAbAttr:',
                              'NULL, -1) != p->form);\n        pr_form_change(bt, p, PR_FCT_SpeciesFormChangeAbilityTrigger);\n        return 1;\n    case PR_AC_IncreasePpUsedAbAttr:'),
    'palafin-route-removed': ('src/phases.c', '} else if (mon->species == SPECIES_ID_PALAFIN &&', '} else if (false && mon->species == SPECIES_ID_PALAFIN &&'),
    'post-battle-init-hero': ('src/closures_abilities.c', '{"ability:278", "5e264af5663d", form_zero},', '{"ability:278", "5e264af5663d", hero_on_switch_out},'),
    'mark-not-set': ('src/ab_attrs.c', '((pr_pokemon *)p)->zero_to_hero_shown = true;', '(void)p;'),
    'mark-not-cleared': ('src/modifiers.c', 'p->zero_to_hero_shown = false;', ''),
    'message-ignores-mark': ('src/closures_abilities.c', '!a->switch_out_status && !a->zero_to_hero_shown;', '!a->switch_out_status;'),
    'message-ignores-field': ('src/closures_abilities.c', 'return a->form != 0 && a->on_field && !a->switch_out_status', 'return a->form != 0'),
    'message-in-zero-form': ('src/closures_abilities.c', 'return a->form != 0 && a->on_field', 'return a->on_field'),
}

def turn_summary(text):
    return [l for l in text.splitlines() if l.startswith('z2hturn') or l.startswith(turns.name)]

with tempfile.TemporaryDirectory(prefix='zero-to-hero-mutation-') as temp:
    intact = None
    for name, edit in [('intact', None), *mutants.items()]:
        dest = Path(temp) / name
        shutil.copytree(root, dest, ignore=shutil.ignore_patterns('build'))
        if edit:
            f, old, new = edit
            p = dest / f; s = p.read_text()
            assert s.count(old) == 1, (name, old)
            p.write_text(s.replace(old, new, 1))
        b = subprocess.run(['make', '-s', '-j4', '-C', str(dest), 'build/zero-to-hero-test', 'build/turntest'], capture_output=True, text=True)
        if b.returncode:
            raise SystemExit(f'{name}: build failed\n{b.stdout}{b.stderr}')
        a = subprocess.run([str(dest / 'build/zero-to-hero-test'), str(api)], capture_output=True, text=True)
        t = subprocess.run([sys.executable, str(dest / 'tools/check-zero-to-hero-turns.py'), str(turns), str(dest / 'build/turntest')], capture_output=True, text=True)
        api_line = next((l for l in a.stdout.splitlines() if l.startswith('Zero to Hero boundary')), 'no summary')
        turn_line = (turn_summary(t.stdout) or ['no summary'])[0]
        print(f'{name}: api exit {a.returncode} ({api_line}); turns exit {t.returncode} ({turn_line})')
        if name == 'intact':
            if a.returncode or t.returncode:
                raise SystemExit('intact build does not pass')
            continue
        if not (a.returncode or t.returncode):
            raise SystemExit(f'{name}: not caught')
    print(f'all {len(mutants)} mutants caught')
