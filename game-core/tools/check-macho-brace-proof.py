#!/usr/bin/env python3
"""Compile actual implementation faults; native observations must reject each."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
gc = root / 'game-core'
bc = root / 'battle-core'
fixture = gc / 'tests/fixtures/macho-brace.native.txt'
runner = gc / 'build/macho_brace_host_test'
subprocess.run([runner, fixture], check=True)
libs = [gc/'build/libgc.a', gc/'build/reward-core/libreward.a', gc/'build/ai-core/libai.a',
        gc/'build/run-core/librun.a', gc/'build/run-core/battle-core/libprcore.a']
includes = [gc/'include', gc/'src', root/'run-core/include', root/'ai-core/include',
            root/'reward-core/include', root/'reward-core/gen/out', bc/'include', bc/'gen/out']
faults = []

def fault(path, name, old, new, count=1):
    text = (root/path).read_text()
    assert text.count(old) == count, (name, text.count(old))
    faults.append((name, text.replace(old, new)))

fault('battle-core/src/modifiers.c', 'lose-max-stack-bonus', 'if (m->stack_count == 50)', 'if (false)', 2)
fault('battle-core/src/modifiers.c', 'count-virtual-as-real', '*value += 2 * m->stack_count;', '*value += 2 * (m->stack_count + m->virtual_stack_count);')
fault('game-core/src/gc_phases.c', 'cap-blocks-growth', 'if (it->stack < 50) {', 'if (it->stack < 50 && m->b.level < pp.max_exp_level) {')
fault('game-core/src/gc_phases.c', 'grow-fainted-holder', 'm->b.hp > 0 && pp.party[k].participated', 'pp.party[k].participated')
fault('game-core/src/gc_phases.c', 'grow-bench-nonparticipant', 'm->b.hp > 0 && pp.party[k].participated', 'm->b.hp > 0')
fault('game-core/src/gc_phases.c', 'increment-after-max', 'if (it->stack < 50) {', 'if (it->stack <= 50) {')
fault('game-core/src/gc_phases.c', 'choose-last-brace', 'for (int j = 0; j < run->n_items; j++) {', 'for (int j = run->n_items - 1; j >= 0; j--) {')
fault('game-core/src/gc_phases.c', 'retain-virtual-after-growth', 'run->items[q].virtual_stack = 0;', 'run->items[q].virtual_stack = run->items[q].virtual_stack;')
fault('game-core/src/gc_items.c', 'block-virtual-bridge', '&& strcmp(it->key, "MYSTERY_ENCOUNTER_MACHO_BRACE")) return;', ') return;')
fault('game-core/src/gc_items.c', 'brace-transferable',
      'm->cls = PR_MOD_INCREMENTING_STAT; m->virtual_stack_count = it->virtual_stack;\n        m->is_transferable = false;',
      'm->cls = PR_MOD_INCREMENTING_STAT; m->virtual_stack_count = it->virtual_stack;\n        m->is_transferable = true;')
fault('game-core/src/gc_soothe.c', 'virtual-brace-disables-bell', 'const bool brace=battle_gain&&', 'const bool brace=false&&battle_gain&&')
fault('game-core/src/gc_soothe.c', 'fusion-disables-bell',
      '/* Friendship selects by stable holder ID; fusion does not alter the Bell gain. */',
      'for(int j=0;j<run->n_party;j++)if(run->party[j].b.fusion_species)return false;')

with tempfile.TemporaryDirectory(prefix='macho-proof-') as directory:
    directory = Path(directory)
    for name, source in faults:
        src, obj, exe = directory/(name+'.c'), directory/(name+'.o'), directory/name
        src.write_text(source)
        args = ['cc', '-O1', '-std=c99', *[f'-I{x}' for x in includes]]
        subprocess.run([*args, '-c', src, '-o', obj], check=True)
        subprocess.run([*args, gc/'tests/macho_brace_host_test.c', obj, *libs, '-lm', '-o', exe], check=True)
        result = subprocess.run([exe, fixture], capture_output=True, text=True)
        assert result.returncode == 1, (name, result.returncode, result.stdout, result.stderr)
        print('caught compiled fault:', name, flush=True)
    text = fixture.read_text()
    malformed = [text.rsplit('END', 1)[0], text+'EXTRA\n',
                 text.replace('MACHO 1 32 7500 1152 12', 'MACHO 1 32 7499 1152 12', 1),
                 text.replace('S 0 ', 'S 1 ', 1), text.replace('O 0 ', 'O 1 ', 1),
                 text.replace('C 0 ', 'C 1 ', 1), text.replace('A 0 ', 'A 1 ', 1),
                 text.replace('I 0 0 1 0', 'I 0 3 1 0', 1),
                 text.replace('S 0 151 0 1 0 1 0', 'S 0 151 0 1 25 1 0', 1),
                 text.replace('C 0 1 5 0 1 1', 'C 0 1 5 0 0 1', 1)]
    for i, bad in enumerate(malformed):
        assert bad != text
        path = directory/f'bad-{i}.txt'
        path.write_text(bad)
        assert subprocess.run([runner, path], capture_output=True).returncode != 0, i
    print(f'Macho controls: {len(faults)} compiled faults, {len(malformed)} malformed inputs rejected')
