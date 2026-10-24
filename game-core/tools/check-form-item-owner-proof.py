#!/usr/bin/env python3
"""Replay all retained native owners and prove compiled faults are detected."""
from pathlib import Path
import gzip
import hashlib
import os
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
gc = root / 'game-core'
fixtures = gc / 'tests/fixtures/form-item-owner'
environment = dict(os.environ, TURNTEST_LOCALES=str(fixtures / 'en'))
libs = [gc / p for p in ['build/libgc.a', 'build/reward-core/libreward.a',
        'build/ai-core/libai.a', 'build/run-core/librun.a', 'build/run-core/battle-core/libprcore.a']]
includes = [gc / 'include', gc / 'src', gc / 'tests', root / 'run-core/include',
            root / 'ai-core/include', root / 'reward-core/include', root / 'reward-core/gen/out',
            root / 'battle-core/include', root / 'battle-core/gen/out']
faults = []
def fault(path, runner, name, old, new):
    source = (gc / path).read_text()
    assert source.count(old) == 1, (name, source.count(old))
    faults.append((runner, name, source.replace(old, new)))

fault('src/gc_form_items.c', 'menu', 'omit-bench', 'const pr_pokemon *pokemon = &run->party[party].b;', 'if (party == 2) return 0; const pr_pokemon *pokemon = &run->party[party].b;')
fault('src/gc_form_items.c', 'menu', 'ignore-active', '|| item->form_inactive) continue;', ') continue;')
fault('src/gc_form_items.c', 'menu', 'lose-ultra-priority', 'ultra |= item->arg == FORM_CHANGE_ITEM_ULTRANECROZIUM_Z;', 'ultra = false;')
fault('src/gc_form_items.c', 'menu', 'admit-idle-ultra', 'else if (pokemon->species == SPECIES_ID_NECROZMA && item->arg == FORM_CHANGE_ITEM_ULTRANECROZIUM_Z) continue;', 'else if (false) continue;')
fault('src/gc_form_items.c', 'menu', 'ignore-owner', 'item->pokemon_id == holder', 'true')
fault('src/gc_form_items.c', 'phase', 'omit-toggle', 'stock->form_inactive = !old;', 'stock->form_inactive = old;')
fault('src/gc_form_items.c', 'phase', 'omit-form-owner', 'if (rule >= 0) gc_reward_change_form(run, party, pr_item_form_rules[rule].to);', 'if (false) gc_reward_change_form(run, party, pr_item_form_rules[rule].to);')
fault('src/gc_form_items.c', 'phase', 'consume-modal-reward', 'if (rule >= 0) gc_reward_change_form(run, party, pr_item_form_rules[rule].to);\n    return true;', 'if (rule >= 0) gc_reward_change_form(run, party, pr_item_form_rules[rule].to);\n    gc_int(run)->rq.waiting = false; return true;')
fault('src/gc_reward_lifecycle.c', 'acquisition', 'only-griseous-core', '&& pr_form_item_known(o->arg);', '&& o->arg == FORM_CHANGE_ITEM_GRISEOUS_CORE;')
fault('src/gc_reward_lifecycle.c', 'acquisition', 'wrong-fallback-tier', 'const int tier = o->tier < 5 ? o->tier : 0;', 'const int tier = 0;')
fault('src/gc_reward_lifecycle.c', 'acquisition', 'lose-rejected-activation-item', 'gc_item_add(run, pr_item_keys[item], o->arg, m->b.id, 1);', 'if (false) gc_item_add(run, pr_item_keys[item], o->arg, m->b.id, 1);')
fault('src/gc_reward_lifecycle.c', 'acquisition', 'omit-acquisition-phase', 'if (rule >= 0) gc_rq_unshift(run, gc_rp(RP_REWARD_FORM, party, pr_item_form_rules[rule].to, 0));', 'if (false) gc_rq_unshift(run, gc_rp(RP_REWARD_FORM, party, pr_item_form_rules[rule].to, 0));')
fault('src/gc_reward_lifecycle.c', 'phase', 'lose-mega-message', 'if (strstr(f->form_key, "mega"))', 'if (false)')
fault('src/gc_reward_lifecycle.c', 'phase', 'retain-autotomized', 'pr_remove_tag(&run->bt, &run->eng, &m->b, BATTLER_TAG_TYPE_AUTOTOMIZED);', '(void)0;')
fault('src/gc_reward_lifecycle.c', 'phase', 'lose-profile-update', 'gc_system_set_pokemon_seen(run->sys, m, false, false);', '(void)0;')

with tempfile.TemporaryDirectory(prefix='form-owner-') as folder:
    folder = Path(folder)
    texts, inputs = {}, {}
    for line in (fixtures / 'SHA256SUMS').read_text().splitlines():
        expected, name = line.split('  ')
        data = gzip.decompress((fixtures / (name + '.gz')).read_bytes()) if name.endswith('.txt') else (fixtures / name).read_bytes()
        assert hashlib.sha256(data).hexdigest() == expected, name
        if name.endswith('.txt'):
            kind = name.split('.')[0]
            inputs[kind] = folder / name
            inputs[kind].write_bytes(data)
            texts[kind] = data.decode()
    for kind, path in inputs.items():
        subprocess.run([gc / f'build/form_item_{kind}_host_test', path], check=True, env=environment)
    subprocess.run([gc / 'build/form_item_owner_guards_test'], check=True)
    for runner, name, content in faults:
        src, obj, exe = folder / (name + '.c'), folder / (name + '.o'), folder / name
        src.write_text(content)
        args = ['cc', '-O1', '-std=c99', *['-I' + str(p) for p in includes]]
        subprocess.run([*args, '-c', src, '-o', obj], check=True)
        subprocess.run([*args, gc / f'tests/form_item_{runner}_host_test.c', obj, *libs, '-lm', '-o', exe], check=True)
        result = subprocess.run([exe, inputs[runner]], capture_output=True, env=environment)
        assert result.returncode == 1, (name, result.returncode, result.stdout, result.stderr)
        print('caught compiled fault:', name, flush=True)
    malformed = []
    for kind, text in texts.items():
        malformed.extend([(kind, text.replace(' 1 ', ' 2 ', 1)),
                          (kind, text.replace('C 0 ', 'C 1 ', 1)),
                          (kind, text.replace('U ', 'X ', 1) if kind != 'phase' else text.replace('G ', 'X ', 1))])
    # Reader bounds must reject a uint32 overflow, not silently truncate it.
    text = texts['acquisition']
    start = text.index('\nM ')
    end = text.index('\n', start + 1)
    fields = text[start + 1:end].split(); fields[3] = '4294967296'
    malformed.append(('acquisition', text[:start + 1] + ' '.join(fields) + text[end:]))
    malformed.append(('menu', texts['menu'].rsplit('END', 1)[0]))
    for i, (kind, bad) in enumerate(malformed):
        assert bad != texts[kind]
        path = folder / f'bad-{i}.txt'; path.write_text(bad)
        result = subprocess.run([gc / f'build/form_item_{kind}_host_test', path], capture_output=True, env=environment)
        assert result.returncode == 1, (i, kind, result.returncode)
    print(f'Form owner controls: {len(faults)} compiled faults, {len(malformed)} malformed transports rejected')
