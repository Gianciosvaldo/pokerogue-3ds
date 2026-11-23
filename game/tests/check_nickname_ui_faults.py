#!/usr/bin/env python3
"""Planted faults for the app's nickname code: each one is a one-line change to a source file, compiled on its own and linked with the
unchanged objects of the test that covers it. The test must stop on an assertion (or its CHECK) for every one, while the unchanged code
passes it. Usage, from the repository root after `make -C game-core build/libgc.a` and its other libraries (CI builds them in
"Run game-core tests"):  python3 game/tests/check_nickname_ui_faults.py

Four tests: party_item_test (the party screen's Rename: the option, the form's starting text, the member that gets the nickname, the
battle's info box), fused_name_test (the names the battle view is given: nickname, disguise, form change), name_entry_test (the text
entry's starting text and overlay) and starter_ui_test (the starter screen's Rename: the row, the form's starting text, saving and undoing,
the nickname in the panel and in the run's starter, the panel's cut of a long name)."""
from pathlib import Path
import os
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[2]
CC = os.environ.get('CC', 'cc')
INCLUDES = ['game/tests', 'game/include', 'game/menus', 'game/gen', 'assets-pipeline/app-src/include',
            'game-core/include', 'game-core/src', 'game-core/gen/out',
            'run-core/include', 'run-core/gen/out', 'reward-core/include', 'reward-core/gen/out', 'custom-core/include',
            'battle-core/include', 'battle-core/gen/out', 'ai-core/include', 'ai-core/gen/out']
FLAGS = ['-O1', '-g', '-std=gnu11', '-w', '-ffp-contract=off', *['-I' + str(ROOT / p) for p in INCLUDES]]
LIBS = ['game-core/build/libgc.a', 'game-core/build/reward-core/libreward.a', 'game-core/build/ai-core/libai.a',
        'game-core/build/run-core/librun.a', 'game-core/build/run-core/battle-core/libprcore.a']

PARTY = ['game/tests/party_item_test.c', 'game/tests/party_fake.c', 'game/menus/item_ui.c', 'game/menus/party.c', 'game/menus/menus_names.c',
         'game/menus/move_names.c', 'game/menus/menus_data.c', 'game/gen/gm_item_ui.c', 'game/source/party_screen.c']
FUSED = ['game/tests/fused_name_test.c', 'game/tests/battle_setup.c', 'game/source/battle_events.c']
ENTRY = ['game/tests/name_entry_test.c', 'game/source/name_entry.c']
STARTER = ['game/tests/starter_ui_test.c', 'game/menus/starter.c', 'game/menus/starter_model.c', 'game/menus/menus_data.c', 'game/menus/menus_names.c',
           'game/menus/move_names.c', 'game/source/starter_host.c', 'game/source/starter_filter.c', 'game/source/starter_data.c']
TESTS = {'party_item_test': (PARTY, True), 'fused_name_test': (FUSED, True), 'name_entry_test': (ENTRY, False), 'starter_ui_test': (STARTER, True)}

PS, PM, BE, NE = 'game/source/party_screen.c', 'game/menus/party.c', 'game/source/battle_events.c', 'game/source/name_entry.c'
SM, SH, SD = 'game/menus/starter.c', 'game/source/starter_host.c', 'game/source/starter_data.c'
FORM_OLD = 'if (!(p->nickname[0] && !pr_shows_illusion(bt, p))) snprintf((*sm).name'

# (name, test, file, original text (exactly once), replacement)
FAULTS = [
    ('rename-not-offered', 'party_item_test', PM, 'if (P.ev.on_rename) P.options[P.nopt++] = PO_RENAME;', 'if (0) P.options[P.nopt++] = PO_RENAME;'),
    ('rename-ignored-in-the-option-list', 'party_item_test', PM,
     '  if (option == PO_RENAME && P.ev.on_rename) {\n    rename_option();\n    return true;\n  }\n  if (option == PO_UNSPLICE) {',
     '  if (option == PO_UNSPLICE) {'),
    ('rename-gives-the-slot-not-the-member', 'party_item_test', PS, '&g_run_mut->party[storage_at(slot)].b : NULL;', '&g_run_mut->party[slot].b : NULL;'),
    ('battle-switch-uses-run-order', 'party_item_test', PS, 'if (S.bt) return slot < S.bt->n_party[0] ? S.bt->party[0][slot] : NULL;', ''),
    ('cancel-clears-the-nickname', 'party_item_test', PS, 'if (text && g) {', 'if (g) {'),
    ('bad-text-no-error-sound', 'party_item_test', PS, 'ui_play_error(); /* not text', '(void)0; /* not text'),
    ('party-not-resumed', 'party_item_test', PS, '  party_resume();\n}\n\nstatic void on_rename', '}\n\nstatic void on_rename'),
    ('entry-not-popped', 'party_item_test', PS, '  ui_pop();\n  pr_pokemon* p = slot_pokemon(slot);', '  pr_pokemon* p = slot_pokemon(slot);'),
    ('list-keeps-the-old-name', 'party_item_test', PS, '      party_refresh(S.party, S.n);\n    } else {', '    } else {'),
    ('form-starts-empty', 'party_item_test', PS, 'a.initial = now;', 'a.initial = NULL;'),
    ('form-is-not-an-overlay', 'party_item_test', PS, 'a.overlay = true;', 'a.overlay = false;'),
    ('info-box-not-renamed', 'party_item_test', PS, '      if (S.bt)\n        for (int k = 0; k < 4; k++) {', '      if (0)\n        for (int k = 0; k < 4; k++) {'),
    ('info-box-of-a-disguise-renamed', 'party_item_test', PS, 'v->pokemon_id == p->id && !v->illusion)', 'v->pokemon_id == p->id)'),
    ('info-box-of-anyone-renamed', 'party_item_test', PS, 'v->identity_known && v->pokemon_id == p->id && !v->illusion)', 'v->identity_known && !v->illusion)'),
    ('view-ignores-the-nickname', 'fused_name_test', BE,
     'look ? p->illusion.fusion_species : p->fusion_species, look ? NULL : p->nickname);', 'look ? p->illusion.fusion_species : p->fusion_species, NULL);'),
    ('disguise-shows-the-nickname', 'fused_name_test', BE, 'look ? NULL : p->nickname);', 'p->nickname);'),
    ('disguise-break-keeps-the-species-name', 'fused_name_test', BE, 'if (nick[0]) snprintf(mon->name, sizeof mon->name, "%s", nick);', ''),
    ('disguise-break-wrong-side', 'fused_name_test', BE, 'nickname_of(bt, b >= 2, ev->field[b].pokemon_id)', 'nickname_of(bt, 1, ev->field[b].pokemon_id)'),
    ('form-change-drops-the-nickname', 'fused_name_test', BE, FORM_OLD, 'if (1) snprintf((*sm).name'),
    ('form-change-keeps-the-nickname-under-a-disguise', 'fused_name_test', BE, FORM_OLD, 'if (!(p->nickname[0])) snprintf((*sm).name'),
    ('form-change-never-names-the-species', 'fused_name_test', BE, FORM_OLD, 'if (0) snprintf((*sm).name'),
    ('entry-initial-not-loaded', 'name_entry_test', NE, '  load_initial(a->initial);', ''),
    ('entry-initial-over-the-limit', 'name_entry_test', NE, 'while (*s && E.n_chars < E.a.max_chars) {', 'while (*s) {'),
    ('entry-initial-torn-character', 'name_entry_test', NE, '    if (!whole) break;', ''),
    ('entry-overlay-ignored', 'name_entry_test', NE, '  if (a->overlay)\n    ui_push(&g_screen);', '  if (0)\n    ui_push(&g_screen);'),
    ('starter-rename-not-offered', 'starter_ui_test', SM, 'if (S.ev.rename) S.menu.act[S.menu.n++] = 3;', 'if (0) S.menu.act[S.menu.n++] = 3;'),
    ('starter-rename-offered-without-the-event', 'starter_ui_test', SM, 'if (S.ev.rename) S.menu.act[S.menu.n++] = 3;', 'S.menu.act[S.menu.n++] = 3;'),
    ('starter-rename-row-does-nothing', 'starter_ui_test', SM, 'if (S.menu.act[row] == 3 && S.ev.rename) S.ev.rename(S.ev.ctx, sp);', ''),
    ('starter-rename-row-before-customize', 'starter_ui_test', SM,
     '  if (S.ev.configure) S.menu.act[S.menu.n++] = 2;\n  if (S.ev.rename) S.menu.act[S.menu.n++] = 3;',
     '  if (S.ev.rename) S.menu.act[S.menu.n++] = 3;\n  if (S.ev.configure) S.menu.act[S.menu.n++] = 2;'),
    ('starter-form-starts-with-the-species-name', 'starter_ui_test', SH, 'a.initial = gc_system_starter_nickname(H.sys, species);', 'a.initial = i18n_species_name(species);'),
    ('starter-form-is-not-an-overlay', 'starter_ui_test', SH, 'a.overlay = true;', 'a.overlay = false;'),
    ('starter-entry-not-popped', 'starter_ui_test', SH, '  const uint16_t species = (uint16_t)(long)arg;\n  ui_pop();', '  const uint16_t species = (uint16_t)(long)arg;'),
    ('starter-cancel-clears-the-nickname', 'starter_ui_test', SH, '  if (!text) return;\n  gc_system* base', '  if (!text) text = "";\n  gc_system* base'),
    ('starter-same-text-is-written-again', 'starter_ui_test', SH, 'if (!strcmp(old, text)) {ui_play_select();return;}', ''),
    ('starter-pending-write-not-settled', 'starter_ui_test', SH,
     '  if(!editor_save()) {starter_message("The profile could not be saved. Nothing was changed.");return;}\n  const int r = ss_set_nickname',
     '  const int r = ss_set_nickname'),
    ('starter-bad-write-keeps-the-nickname', 'starter_ui_test', SH, '    ss_set_nickname(base, species, old); /* what it was', '    (void)old; /* what it was'),
    ('starter-bad-write-not-reported', 'starter_ui_test', SH,
     '    editor_refresh();\n    starter_message("The profile could not be saved. Nothing was changed.");\n    return;\n  }\n  if (H.real) ss_set_nickname(H.sys, species, text);   /* the copy the screen reads shows it too */\n  ui_play_select();\n  editor_refresh(); /* the panel',
     '    editor_refresh();\n    return;\n  }\n  if (H.real) ss_set_nickname(H.sys, species, text);   /* the copy the screen reads shows it too */\n  ui_play_select();\n  editor_refresh(); /* the panel'),
    ('starter-nickname-not-saved', 'starter_ui_test', SH, '  if(!editor_save()) {\n    ss_set_nickname(base, species, old);', '  if(0) {\n    ss_set_nickname(base, species, old);'),
    ('starter-panel-not-refreshed', 'starter_ui_test', SH, '  ui_play_select();\n  editor_refresh(); /* the panel shows the new name */', '  ui_play_select();'),
    ('starter-refusal-silent', 'starter_ui_test', SH, '    ui_play_error();\n    if (r == GC_STARTER_NICK_FULL) {', '    if (r == GC_STARTER_NICK_FULL) {'),
    ('starter-full-table-not-explained', 'starter_ui_test', SH, 'if (r == GC_STARTER_NICK_FULL) {', 'if (0) {'),
    ('starter-uncaught-takes-a-nickname', 'starter_ui_test', SD, '  if (!caught(sys, species)) return GC_STARTER_NICK_NOT_A_STARTER;\n', ''),
    ('starter-nickname-not-handed-to-the-run', 'starter_ui_test', SD,
     '  snprintf(out->nickname, sizeof out->nickname, "%s", gc_system_starter_nickname(sys, species));', ''),
    ('starter-panel-ignores-the-nickname', 'starter_ui_test', SD, '*nickname ? nickname : i18n_species_name(species)', 'i18n_species_name(species)'),
    ('starter-panel-cut-counts-bytes', 'starter_ui_test', SM, '    units += bytes == 4 ? 2 : 1;', '    units += bytes;'),
    ('starter-panel-cut-counts-an-emoji-once', 'starter_ui_test', SM, '    units += bytes == 4 ? 2 : 1;', '    units += 1;'),
    ('starter-panel-cut-splits-a-character', 'starter_ui_test', SM, '    if (units <= 12) cut = (size_t)(p - (const unsigned char*)name);', '    if (units <= 12) cut = (size_t)units;'),
    ('starter-panel-never-cut', 'starter_ui_test', SM, '  if (units > 15) strcpy(name + cut, "...");', ''),
    ('starter-top-screen-cuts-the-name', 'starter_ui_test', SM,
     '  snprintf(name, sizeof name, "%s", m->name);\n  mui_text(TS_SUMMARY, 6, MUI_H - 53, name);', '  snprintf(name, sizeof name, "%.40s", m->name);\n  mui_text(TS_SUMMARY, 6, MUI_H - 53, name);'),
]


def run(args, **kw):
    p = subprocess.run(args, cwd=ROOT, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, errors='replace', **kw)
    if p.returncode:
        raise RuntimeError('command failed: ' + ' '.join(map(str, args)) + '\n' + p.stdout[-5000:])
    return p


def main():
    missing = [p for p in LIBS if not (ROOT / p).exists()]
    if missing:
        sys.exit('build the game-core libraries first (make -C game-core test): missing ' + ', '.join(missing))
    caught_all = True
    with tempfile.TemporaryDirectory(prefix='nickname-ui-faults-') as temp:
        work = Path(temp)
        objs = {}
        for t, (sources, _) in TESTS.items():
            for s in sources:
                if s not in objs:
                    objs[s] = work / (s.replace('/', '_') + '.o')
                    run([CC, *FLAGS, '-I' + str((ROOT / s).parent), '-c', str(ROOT / s), '-o', str(objs[s])])
        exe = work / 'test'

        def link(test, replace=None):
            sources, uses_libs = TESTS[test]
            parts = [str(replace[1] if replace and replace[0] == s else objs[s]) for s in sources]
            run([CC, *parts, *([str(ROOT / p) for p in LIBS] if uses_libs else []), '-lm', '-o', str(exe)])
            return subprocess.run([str(exe)], cwd=ROOT, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, errors='replace')

        for t in TESTS:
            p = link(t)
            if p.returncode != 0:
                sys.exit(f'the unchanged code fails {t}:\n' + p.stdout[-3000:])
        n = 0
        for name, test, file, old, new in FAULTS:
            source = (ROOT / file).read_text()
            if source.count(old) != 1:
                sys.exit(f'{name}: the original text is not in {file} exactly once ({source.count(old)})')
            mutated = work / 'mutated.c'
            mutated.write_text(source.replace(old, new, 1))
            obj = work / 'mutated.o'
            run([CC, *FLAGS, '-I' + str((ROOT / file).parent), '-c', str(mutated), '-o', str(obj)])
            p = link(test, (file, obj))
            # an assertion or the test's own CHECK failing; a crash or a link error would not count
            caught = p.returncode != 0 and ('Assertion' in p.stdout or ' line' in p.stdout or 'FAIL' in p.stdout)
            print(('caught ' if caught else 'MISSED ') + name + ' (' + test + ')' + ('' if caught else f'  exit {p.returncode}: {p.stdout[-200:]}'), flush=True)
            n += caught
            caught_all = caught_all and caught
    if not caught_all:
        sys.exit('planted faults not caught')
    print(f'Nickname UI: {n} of {len(FAULTS)} planted faults caught; unchanged code passes all four tests.')


main()
