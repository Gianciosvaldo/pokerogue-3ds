#!/usr/bin/env python3
"""Mutate the Aftermath / Innards Out port, rebuild and replay the source turns; every mutant must be caught.
Usage (TURNTEST_LOCALES set): aftermath-mutations.py TURN_TRACE [TURN_TRACE...]"""
from pathlib import Path
import shutil, subprocess, sys, tempfile
root = Path(__file__).resolve().parent.parent
traces = [Path(a).resolve() for a in sys.argv[1:]]
AM = 'case PR_AC_PostFaintContactDamageAbAttr: {                          /* Aftermath */'
mutants = {
    'contact-ignored': ('src/ab_attrs.c', 'if (move < 0 || !o || !pr_move_does_flag_effect_apply(bt, move, MOVE_FLAGS_MAKES_CONTACT, o, p)) return 0;',
                        'if (move < 0 || !o) return 0;'),
    'damp-ignored': ('src/ab_attrs.c', '        if (cancelled) return 0;\n        pr_ab_params bp = {.pokemon = o',
                     '        cancelled = false;\n        pr_ab_params bp = {.pokemon = o'),
    'damp-simulated': ('src/ab_attrs.c', 'pr_ab_params dp = {.pokemon = q, .move = -1, .flag = &cancelled, .simulated = pp->simulated};',
                       'pr_ab_params dp = {.pokemon = q, .move = -1, .flag = &cancelled, .simulated = true};'),
    'magic-guard-ignored': ('src/ab_attrs.c', 'GATE(!cancelled);\n        if (pp->simulated) return 1;', 'GATE(true);\n        if (pp->simulated) return 1;'),
    'aftermath-eighth': ('src/ab_attrs.c', 'pr_damage_and_update(bt, c->e, w, to_dmg_value(pr_get_stat(w, STAT_HP, true) * (1.0 / x->damageRatio)),\n                             HIT_RESULT_INDIRECT, false, NULL);\n        w->damage_taken += (int32_t)to_dmg_value(pr_get_stat(w, STAT_HP, true) * (1.0 / x->damageRatio));\n        return 1;\n    }\n    case PR_AC_PostFaintHPDamageAbAttr',
                         'pr_damage_and_update(bt, c->e, w, to_dmg_value(pr_get_stat(w, STAT_HP, true) * (1.0 / 8)),\n                             HIT_RESULT_INDIRECT, false, NULL);\n        w->damage_taken += (int32_t)to_dmg_value(pr_get_stat(w, STAT_HP, true) * (1.0 / x->damageRatio));\n        return 1;\n    }\n    case PR_AC_PostFaintHPDamageAbAttr'),
    'aftermath-damage-taken-dropped': ('src/ab_attrs.c', '        w->damage_taken += (int32_t)to_dmg_value(pr_get_stat(w, STAT_HP, true) * (1.0 / x->damageRatio));\n        return 1;\n    }\n    case PR_AC_PostFaintHPDamageAbAttr',
                                       '        return 1;\n    }\n    case PR_AC_PostFaintHPDamageAbAttr'),
    'innards-shown-only-on-move': ('src/ab_attrs.c', 'GATE(true);\n        if (move < 0 || !o || pp->simulated', 'GATE(move >= 0 && o);\n        if (move < 0 || !o || pp->simulated'),
    'innards-magic-guard-ignored': ('src/ab_attrs.c', '        if (cancelled) return 1;\n        double damage', '        double damage'),
    'innards-half-damage': ('src/ab_attrs.c', 'double damage = p->attacks_received[0].damage;', 'double damage = floor(p->attacks_received[0].damage / 2);'),
    'no-attacker-passed': ('src/phases.c', 'faint_params.opponent = pr_pokemon_by_id(bt, p->attacks_received[0].source_id);', ''),
}

with tempfile.TemporaryDirectory(prefix='aftermath-mutation-') as temp:
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
            r = subprocess.run([sys.executable, str(dest / 'tools/check-aftermath-turns.py'), str(t), str(dest / 'build/turntest')], capture_output=True, text=True)
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
