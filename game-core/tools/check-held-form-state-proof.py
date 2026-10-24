#!/usr/bin/env python3
"""Native form DTO inventory/save replay and compiled failure controls."""
from pathlib import Path
import gzip
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
gc = root / 'game-core'
bc = root / 'battle-core'
faults = []
def fault(path, name, old, new):
    source = (gc/path).read_text()
    assert source.count(old) == 1, (name, source.count(old))
    faults.append((name, source.replace(old, new)))

fault('src/gc_items.c', 'retain-stale-inactive', 'it->form_inactive = false;', 'it->form_inactive = it->form_inactive;')
fault('src/gc_items.c', 'ignore-inactive-bridge', 'm->form_item_active = !it->form_inactive;', 'm->form_item_active = true;')
fault('src/gc_items.c', 'admit-orphan', 'if (!attached) bt->mods[side][i].unknown_subclass = true;', 'if (false) bt->mods[side][i].unknown_subclass = true;')
fault('src/gc_items.c', 'wrong-dex-bit', 'bt->form_item_dex_mask |= 1u << i;', 'bt->form_item_dex_mask |= 1u << ((i+1)%6);')
fault('src/gc_reward.c', 'ignore-inactive-reward', 'o->active = !gc_item_key_form_change(it->key) || !it->form_inactive;', 'o->active = true;')
fault('src/gc_save.c', 'omit-saved-inactive', 'sv[i].form_inactive = items[i].form_inactive;', 'sv[i].form_inactive = false;')
fault('src/gc_save.c', 'ignore-loaded-inactive', 'items[i].form_inactive=inactive!=0;', 'items[i].form_inactive=false;')
fault('src/gc_save.c', 'wrong-legacy-default', 'items[i].form_inactive=inactive!=0;', 'items[i].form_inactive=form_marker ? inactive!=0 : true;')
fault('src/gc_save.c', 'optional-prefix-regression', 'get_block(&r, expected, required,', 'get_block(&r, item_fields, NF(item_fields)-!class_marker-!form_marker,')
fault('src/gc_save.c', 'accept-nonboolean', 'if(inactive>1||(inactive&&', 'if(false||(inactive&&')
fault('src/gc_save.c', 'accept-duplicate-marker', 'if(form_marker||size!=sizeof sv[0].form_inactive)', 'if(size!=sizeof sv[0].form_inactive)')
fault('src/gc_save.c', 'write-wrong-family-marker', 'if (items[i].form_inactive && (', 'if (false && items[i].form_inactive && (')

libs = [gc/'build/libgc.a', gc/'build/reward-core/libreward.a', gc/'build/ai-core/libai.a',
        gc/'build/run-core/librun.a', gc/'build/run-core/battle-core/libprcore.a']
includes = [gc/'include', gc/'src', root/'run-core/include', root/'ai-core/include',
            root/'reward-core/include', root/'reward-core/gen/out', bc/'include', bc/'gen/out']
with tempfile.TemporaryDirectory(prefix='held-form-state-') as directory:
    directory = Path(directory)
    fixture = directory/'native.txt'
    text = gzip.decompress((gc/'tests/fixtures/held-form-constructors.native.txt.gz').read_bytes()).decode()
    fixture.write_text(text)
    legacy = gc/'tests/fixtures/held-form.pr399.save'
    runner = gc/'build/held_form_items_host_test'
    subprocess.run([runner, fixture, legacy], check=True)
    for name, content in faults:
        src, obj, exe = directory/(name+'.c'), directory/(name+'.o'), directory/name
        src.write_text(content)
        args = ['cc', '-O1', '-std=c99', *['-I'+str(p) for p in includes]]
        subprocess.run([*args, '-c', src, '-o', obj], check=True)
        subprocess.run([*args, gc/'tests/held_form_items_host_test.c', obj, *libs, '-lm', '-o', exe], check=True)
        result = subprocess.run([exe, fixture, legacy], capture_output=True, text=True)
        assert result.returncode == 1, (name, result.returncode, result.stdout, result.stderr)
        print('caught compiled fault:', name, flush=True)
    malformed = [text.rsplit('END', 1)[0], text+'EXTRA\n', text.replace('2544', '2543', 1),
                 text.replace('C 0 ', 'C 1 ', 1), text.replace('L 0 ', 'L 200 ', 1)]
    for i, bad in enumerate(malformed):
        assert bad != text
        path = directory/f'bad-{i}.txt'
        path.write_text(bad)
        assert subprocess.run([runner, path, legacy], capture_output=True).returncode == 1, i
    print(f'Held form state controls: {len(faults)} compiled faults, {len(malformed)} malformed transports rejected')
