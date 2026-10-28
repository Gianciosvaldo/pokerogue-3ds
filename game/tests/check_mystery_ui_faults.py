#!/usr/bin/env python3
"""Compile real UI/core faults against the normal libraries; only assertion failures count.

The ordinary app test separately instruments all sources. This small fault runner uses
normal supporting archives so each fault only needs its changed translation unit rebuilt.
"""
from pathlib import Path
import os
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
CC = os.environ.get('CC', 'cc')
INCLUDES = ['game/tests', 'game/include', 'game/menus', 'assets-pipeline/app-src/include',
            'game-core/include', 'game-core/src', 'game-core/gen/out',
            'run-core/include', 'run-core/gen/out', 'reward-core/include', 'reward-core/gen/out',
            'battle-core/include', 'battle-core/gen/out', 'ai-core/include', 'ai-core/gen/out']
FLAGS = ['-O1', '-g', '-std=gnu11', '-ffp-contract=off', *['-I'+str(ROOT/p) for p in INCLUDES]]
UI = ['game/source/me_flow.c', 'game/source/me_screen.c']
COMMON = ['game/tests/me_screen_test.c', 'game/menus/item_ui.c', 'game/gen/gm_item_ui.c']
LIBS = ['game-core/build/libgc.a', 'game-core/build/reward-core/libreward.a',
        'game-core/build/ai-core/libai.a', 'game-core/build/run-core/librun.a',
        'game-core/build/run-core/battle-core/libprcore.a']
FAULTS = [
 ('first-trade-offer', UI[0], 'finish(F.option, F.member, idx);', 'finish(F.option, F.member, 0);'),
 ('first-held-item', UI[0], 'else finish(F.option, F.member, item);', 'else finish(F.option, F.member, 0);'),
 ('compressed-move-slot', UI[0], 'finish(F.option, F.member, F.pick_row[idx]);', 'finish(F.option, F.member, idx);'),
 ('party-storage-zero', UI[0], 'F.member = slot_of(idx);', 'F.member = 0;'),
 ('wrong-disabled-tooltip', UI[0], 'if (o->ok || !fmt(sub, NULL, 0, false, o->tooltip, sizeof o->tooltip))',
     'if (true || !fmt(sub, NULL, 0, false, o->tooltip, sizeof o->tooltip))'),
 ('raw-markup', UI[1], "if (p[0] == '@' && p[1] == '[')", "if (false && p[0] == '@' && p[1] == '[')"),
 ('no-details-navigation', UI[1], 'if (M.detail_pages > 1) {\n        M.detail_page', 'if (false && M.detail_pages > 1) {\n        M.detail_page'),
 ('silent-vitamin-rewards', 'game-core/src/gc_mystery.c', '"mysteryEncounters/shadyVitaminDealer:option.selected"', '"mysteryEncounters/shadyVitaminDealer:missingSelected"'),
 ('missing-side-effect-pokemon', 'game-core/src/gc_mystery.c', 'pr_arg_pokemon_name(&run->bt, "selectedPokemon", &m->b), pr_arg_key("newNature"',
     'pr_arg_pokemon_name(&run->bt, "wrongPokemon", &m->b), pr_arg_key("newNature"'),
 ('silent-dark-deal', 'game-core/src/gc_darkdeal.c', '"mysteryEncounters/darkDeal:option.1.selectedMessage"', '"mysteryEncounters/darkDeal:missingMessage"'),
 ('generic-generator-name', 'game-core/src/gc_run.c', 'if (item >= 0) {\n        const int id', 'if (false && item >= 0) {\n        const int id'),
 ('missing-minigame-name', 'game-core/src/gc_mystery.c', 'pr_arg_key("wobbuffetName", "pokemon:wobbuffet")', 'pr_arg_key("wrongName", "pokemon:wobbuffet")'),
 ('missing-trade-picker', 'game-core/src/gc_mystery.c', 's->option_pick[0] = GC_PICK_MEMBER_TRADE;', 's->option_pick[0] = GC_PICK_MEMBER;'),
 ('exhausted-item-name-cache', 'game-core/src/gc_run.c', 'if (item >= 0) {\n        static char keys', 'if (item >= 0 && item < 48) {\n        static char keys'),
]

def run(args):
    p = subprocess.run(args, cwd=ROOT, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    if p.returncode:
        raise RuntimeError('command failed: '+' '.join(map(str,args))+'\n'+p.stdout[-5000:])
    return p

run(['make', '-s', '-C', 'game-core', 'build/mystery_texts_test'])
with tempfile.TemporaryDirectory(prefix='mystery-ui-faults-') as temp:
    work = Path(temp)
    common = []
    for i, name in enumerate(COMMON):
        obj = work/f'common-{i}.o'
        run([CC, *FLAGS, '-c', str(ROOT/name), '-o', str(obj)])
        common.append(str(obj))
    plain_ui = {}
    for i, name in enumerate(UI):
        obj = work/f'ui-{i}.o'
        run([CC, *FLAGS, '-c', str(ROOT/name), '-o', str(obj)])
        plain_ui[name] = str(obj)
    executable = work/'test'
    def link(extra, ui):
        run([CC, *common, *ui, *extra, *[str(ROOT/p) for p in LIBS], '-lm', '-o', str(executable)])
    link([], list(plain_ui.values()))
    run([str(executable)])
    for name, file, old, new in FAULTS:
        source = (ROOT/file).read_text()
        if source.count(old) != 1:
            raise RuntimeError(f'{name}: expected exactly one fault site, found {source.count(old)}')
        mutated = work/'mutated.c'
        mutated.write_text(source.replace(old,new))
        obj = work/'mutated.o'
        # Quoted includes resolve just as they do beside the original source.
        run([CC, *FLAGS, '-I'+str((ROOT/file).parent), '-c', str(mutated), '-o', str(obj)])
        ui = [str(obj) if p == file else plain_ui[p] for p in UI]
        link([] if file in UI else [str(obj)], ui)
        p = subprocess.run([str(executable)], cwd=ROOT, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
        if p.returncode != 1 or 'FAIL line' not in p.stdout:
            raise RuntimeError(f'{name}: expected assertion failure, got {p.returncode}\n{p.stdout[-3000:]}')
        print(name+': rejected', flush=True)
print(f'Mystery UI: {len(FAULTS)} compiled faults rejected; unmodified baseline passed.')
