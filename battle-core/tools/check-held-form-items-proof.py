#!/usr/bin/env python3
"""Replay retained native results and require compiled faults to fail."""
from pathlib import Path
import gzip
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
runner = root / 'build/held-form-items-test'
source = (root / 'src/form_items.c').read_text()
faults = []

def fault(name, old, new):
    assert source.count(old) == 1, (name, source.count(old))
    faults.append((name, source.replace(old, new)))

# Both scans must move together; native player lookup is intentional for foes too.
faults.append(('enemy-inventory', source.replace('bt->mods[0][i]', 'bt->mods[1][i]').replace('bt->n_mods[0]', 'bt->n_mods[1]')))
fault('ignore-owner', 'm->pokemon_id == p->id', 'true')
fault('ignore-active', 'm->form_item_active == !!(r->flags & 1)', 'true')
fault('ignore-ability', 'if (r->ability &&', 'if (false && r->ability &&')
fault('ignore-dex', 'if (!(bt->form_item_dex_mask & (1u << bit))) return false;', 'if (false) return false;')
fault('wrong-first-rule', 'if (!count) first = i;', 'first = i; if (!count) first = i;')
fault('wrong-necrozma-priority', 'return lunar ? first : solar ? second : -1;', 'return solar ? second : lunar ? first : -1;')
fault('never-deactivate', 'if (switch_active) m->form_item_active = false;', 'if (false) m->form_item_active = false;')
fault('forget-restore', 'if (switch_active) m->form_item_active = true;', 'if (false) m->form_item_active = true;')
fault('add-parent-owner-gate', 'bool switch_active =', 'if (m->pokemon_id != p->id) return false; bool switch_active =')
fault('wrong-female-default', 'p->gender == 1 /* Gender.FEMALE */', 'false')
fault('missing-lowkey-gentle', 'case NATURE_BASHFUL: case NATURE_CALM: case NATURE_GENTLE: case NATURE_CAREFUL:', 'case NATURE_BASHFUL: case NATURE_CALM: case NATURE_CAREFUL:')
fault('always-quiet', 'p->is_player && !(r->flags & 2)', 'false')
fault('lose-pokemon-reference', 'e->pool[phase].pokemon = p;', 'e->pool[phase].pokemon = NULL;')
fault('consume-battle-rng', 'int rule = pr_item_form_rule_index(bt, p);', 'pr_battle_rand_seed_int(bt, 2, 0); int rule = pr_item_form_rule_index(bt, p);')
fault('always-delayed', 'if (delayed) pr_phase_push', 'if (true) pr_phase_push')
fault('never-delayed', 'if (delayed) pr_phase_push', 'if (false) pr_phase_push')

with tempfile.TemporaryDirectory(prefix='held-form-proof-') as directory:
    directory = Path(directory)
    fixture = directory / 'native.txt'
    text = gzip.decompress((root/'tests/fixtures/held-form-items.native.txt.gz').read_bytes()).decode()
    fixture.write_text(text)
    subprocess.run([runner, fixture], check=True)
    includes = ['-I'+str(root/'include'), '-I'+str(root/'gen/out')]
    for name, content in faults:
        src, obj, exe = directory/(name+'.c'), directory/(name+'.o'), directory/name
        src.write_text(content)
        subprocess.run(['cc', '-O1', '-std=c99', *includes, '-c', src, '-o', obj], check=True)
        subprocess.run(['cc', '-O1', '-std=c99', *includes, root/'tests/held-form-items-test.c', obj,
                        root/'build/libprcore.a', '-lm', '-o', exe], check=True)
        result = subprocess.run([exe, fixture], capture_output=True, text=True)
        assert result.returncode == 1, (name, result.returncode, result.stdout, result.stderr)
        print('caught compiled fault:', name, flush=True)
    # Reject omissions, duplicates, changed geometry, invalid fields and trailers.
    malformed = [text.rsplit('END', 1)[0], text+'EXTRA\n',
                 text.replace('124416', '124415', 1),
                 text.replace('C 0 ', 'C 1 ', 1),
                 text.replace('E 0\n', 'E 1\n', 1),
                 text.replace('R 0 ', 'R 1 ', 1),
                 text.replace('N 2\n', 'N 1\n', 1),
                 text.replace('Q 0 ', 'Q 2 ', 1)]
    for i, bad in enumerate(malformed):
        assert bad != text
        path = directory/f'bad-{i}.txt'
        path.write_text(bad)
        assert subprocess.run([runner, path], capture_output=True).returncode == 1, i
    print(f'Held form controls: {len(faults)} compiled faults, {len(malformed)} malformed inputs rejected')
