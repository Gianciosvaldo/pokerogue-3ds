#!/usr/bin/env python3
"""Mutation check of the Nemesis Radar (task C06), the same method as observer_mutations.py.

Each mutation is one textual change to a file under game/source, applied in place, built into the host test that should notice it, run,
and always restored. KILLED: it builds and the test fails. SURVIVED: it builds and the test still passes. INVALID: the pattern did not
match exactly once, or the build failed (nothing was shown either way). Exit 1 on any SURVIVED or INVALID.

  python3 game/tests/radar_mutations.py            # every mutation
  python3 game/tests/radar_mutations.py 3 7        # only these numbers

The groups: what the roster list says and in which order, input and paging, leaving the screen, the sprite read from the SD card (when and
how often), the copies the screen keeps, the words that must not claim a live rematch, the Scout page's honest "not recorded" fallbacks,
the type tint, and the hook in the app's flow.
"""
import os
import subprocess
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
GAME = os.path.join(ROOT, "game")
TESTS = os.path.join(GAME, "tests")

R = "source/radar_screen.c"
RT = ("radar_test", ["build/radar_test", "300"])
FT = ("flowtest", ["build/flowtest"])

# (file under game/, old text, new text, (make target, command), what the mutation is)
M = [
    # ---- the list and its order
    (R, "  return &R.roster.rec[R.roster.count - 1 - list_index];", "  return &R.roster.rec[list_index];", RT, "the list shows the oldest record first"),
    (R, "  if (i > R.roster.count - 1) i = R.roster.count - 1;\n", "", RT, "the cursor can go past the last record"),
    (R, "  if (i < 0) i = 0;\n  if (i > R.roster.count", "  if (i > R.roster.count", RT, "the cursor can go above the first record"),
    (R, "  if (R.sel < R.scroll) R.scroll = R.sel;\n", "", RT, "the list does not scroll back up to the cursor"),
    (R, "  if (R.sel >= R.scroll + VISIBLE) R.scroll = R.sel - VISIBLE + 1;\n", "", RT, "the list does not scroll down to the cursor"),
    (R, "    case BTN_LEFT: select_to(R.sel - VISIBLE); return true;", "    case BTN_LEFT: select_to(R.sel - 1); return true;", RT, "LEFT moves one record, not a page"),
    (R, "    case BTN_RIGHT: select_to(R.sel + VISIBLE); return true;", "    case BTN_RIGHT: select_to(R.sel + 1); return true;", RT, "RIGHT moves one record, not a page"),
    (R, "  if (i < 0 || i == R.sel) return;\n  R.sel = i;", "  if (i < 0) return;\n  R.sel = i;", RT, "a move that goes nowhere still plays the cursor sound"),
    # ---- the words about rematches
    (R, "    case CC_LIFE_STORED: return \"Remembered\";", "    case CC_LIFE_STORED: return \"Held\";", RT, "a stored record is tagged as held"),
    (R, "  if (n->life == CC_LIFE_DEFEATED) {\n    snprintf(out, len, \"Rematch won. The reward is not claimed yet.\");", "  if (n->life == CC_LIFE_NONE) {\n    snprintf(out, len, \"Rematch won. The reward is not claimed yet.\");", RT, "a beaten record is not said to be won"),
    (R, "    if (needs_hold && !held) {", "    if (0 && needs_hold && !held) {", RT, "a plan and a record that contradict each other are both shown as true"),
    (R, "  const bool named = s && s->chosen_id != 0 && s->chosen_id == n->record_id;", "  const bool named = s && s->chosen_id != 0;", RT, "any record is said to be the one the plan names"),
    (R, "        w = window_in_state(s, CC_WIN_PENDING);", "        w = window_in_state(s, CC_WIN_OFFERED);", RT, "the next window is read from the wrong state"),
    (R, "        w = window_in_state(s, CC_WIN_ACCEPTED);", "        w = window_in_state(s, CC_WIN_PENDING);", RT, "an accepted rematch names the wrong wave"),
    (R, "    snprintf(out, len, \"Held by a run whose plan is not loaded here.\");\n  else", "    snprintf(out, len, \"No rematch is scheduled.\");\n  else", RT, "a held record with no plan loaded says nothing is scheduled"),
    (R, "    case CC_SKIP_BOSS: return \"a boss\";", "    case CC_SKIP_BOSS: return \"a trainer battle\";", RT, "a skipped window gives the wrong reason"),
    (R, "  if (R.has_sched && R.sched.chosen_id == n->record_id && R.sched.chosen_id != 0) {", "  if (R.has_sched) {", RT, "the windows are listed for a record the plan does not name"),
    # ---- the Scout page
    (R, "  if (n->known & CC_NK_FALLEN)\n    snprintf(t, sizeof t, \"Last of yours to fall: %s.\"", "  if (1)\n    snprintf(t, sizeof t, \"Last of yours to fall: %s.\"", RT, "an unknown last-to-fall is shown as a species"),
    (R, "  if (n->known & CC_NK_FINISH_MOVE)\n", "  if (1)\n", RT, "an unknown final move is shown as a move"),
    (R, "  if (n->known & CC_NK_NATURE)\n", "  if (1)\n", RT, "an unknown nature is shown as a nature"),
    (R, "  if (n->known & CC_NK_IVS) {\n", "  if (1) {\n", RT, "unknown IVs are shown as numbers"),
    (R, "  if (n->known & CC_NK_PASSIVE) {\n", "  if (1) {\n", RT, "an unknown passive is shown as a passive"),
    (R, "  if (n->known & CC_NK_LOADOUT) {\n    if (n->n_items == 0)", "  if (1) {\n    if (n->n_items == 0)", RT, "an unknown loadout is shown as none"),
    (R, "(n->known & CC_NK_LOADOUT_TRUNCATED) ? \", and more.\" : \".\"", "\".\"", RT, "a cut-short loadout does not say there is more"),
    (R, "  if (it->stack > 1)\n    snprintf(out, n, \"%s x%u\"", "  if (it->stack > 0)\n    snprintf(out, n, \"%s x%u\"", RT, "an item held once is shown as x1"),
    (R, "  if (!out[0]) snprintf(out, (size_t)n, \"an unknown place\");", "", RT, "an unnamed biome is shown as nothing"),
    (R, "        if (R.dscroll < R.n_detail - DETAIL_VISIBLE) R.dscroll++;", "        if (R.dscroll < R.n_detail) R.dscroll++;", RT, "the Scout page scrolls past its end"),
    (R, "        if (R.dscroll > 0) R.dscroll--;", "        R.dscroll--;", RT, "the Scout page scrolls above its start"),
    (R, "        R.mode = MODE_LIST;\n        return true;", "        return true;", RT, "B on the Scout page does not return to the list"),
    (R, "      if (!R.usable || R.roster.count == 0) {\n        ui_play_error();", "      if (!R.usable) {\n        ui_play_error();", RT, "A on an empty roster plays the selection sound, not the error"),
    # ---- leaving
    (R, "      R.left = true;\n", "", RT, "the screen keeps taking input after it asked to leave"),
    (R, "      if (R.on_exit) R.on_exit(R.exit_arg);\n", "", RT, "B on the list does not leave"),
    (R, "  R.on_exit = a->on_exit;\n", "", RT, "the exit callback is not kept"),
    (R, "  if (R.left) return true;\n  if (R.mode == MODE_DETAIL) {", "  if (R.mode == MODE_DETAIL) {", RT, "a second B after leaving leaves again"),
    # ---- the sprite
    (R, "#define SPRITE_DELAY_MS 140.0f", "#define SPRITE_DELAY_MS 0.0f", RT, "a sprite is read for every record the cursor passes"),
    (R, "  if (R.sprite_id == n->record_id) return;   /* loaded", "  /* loaded", RT, "the sprite is read again every frame"),
    (R, "  R.sprite_wait = 0;\n  ui_play_select();", "  ui_play_select();", RT, "the rest timer is not restarted when the cursor moves"),
    (R, "  R.sprite_id = n->record_id;\n", "", RT, "a record without a sprite is looked for again every frame"),
    (R, "  ra_sprite_free();\n  R.sprite_ok = false;\n}", "  R.sprite_ok = false;\n}", RT, "leaving does not free the sprite"),
    # ---- the copies and what is refused
    (R, "  if (a->meta && cc_roster_validate(&a->meta->roster) != CC_OK) return false;\n", "", RT, "a roster that does not validate is shown"),
    (R, "  if (a->schedule && cc_schedule_validate(a->schedule) != CC_OK) return false;\n", "", RT, "a schedule that does not validate is shown"),
    # ---- the tint
    (R, "  if (typed && (t1 < 0 || t1 > 17)) typed = false;", "", RT, "a type outside the eighteen is named"),
    (R, "  if (typed && type_tint(t1)) rr_rect(0, 0, 320, 180, type_tint(t1));", "  if (type_tint(t1 < 0 ? 0 : t1)) rr_rect(0, 0, 320, 180, type_tint(t1 < 0 ? 0 : t1));", RT, "an unknown type is tinted as Normal"),
    (R, "RR_RGBA(rgb[type][0], rgb[type][1], rgb[type][2], 70)", "RR_RGBA(rgb[type][0], rgb[type][1], rgb[type][2], 220)", RT, "the tint is strong, not modest"),
    (R, "  if (text_width(style, s, NULL) <= w) return;", "  if (text_width(style, s, NULL) <= w + 200) return;", RT, "a long name is not cut to its place"),
    # ---- the app's hook
    ("source/app_flow.c", "    case TITLE_RADAR:\n      show_radar();\n      break;", "    case TITLE_RADAR:\n      break;", FT, "the title entry opens nothing"),
    ("source/app_flow.c", "  const bool usable = cs_get_state(why) != CS_UNREADABLE;", "  const bool usable = cs_get_state(why) == CS_UNREADABLE;", FT, "an unusable file is treated as usable and the reverse"),
    ("source/app_flow.c", "  r.on_exit = on_radar_exit;\n", "", FT, "leaving the Radar goes nowhere"),
    ("source/app_flow.c", "  r.why = why;\n", "", FT, "an unusable file is not explained"),
    ("source/app_flow.c", "    r.meta = NULL;\n    radar_open(&r);", "    r.meta = NULL;", FT, "a refused open shows nothing"),
]


# Changes that cannot change what the screen does, and why (listed, not run)
EQUIVALENT = [
    ("`if (n->known & CC_NK_ABILITY)` in build_detail() made unconditional",
     "radar_open requires cc_roster_validate, which requires cc_nemesis_support == OK, which requires the ability bit: a record without a "
     "known ability cannot reach the page. The 'not recorded' text stays as a guard in case a later version relaxes the validator."),
    ("`if (n->known & CC_NK_MOVES)` in build_detail() made unconditional",
     "the same: a valid record always has its moves known."),
]


def run(cmd, cwd, timeout=900):
    p = subprocess.run(cmd, cwd=cwd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, universal_newlines=True, timeout=timeout)
    return p.returncode, p.stdout


def main():
    want = set(int(a) for a in sys.argv[1:])
    results = []
    for i, (rel, old, new, (target, cmd), what) in enumerate(M, 1):
        if want and i not in want:
            continue
        path = os.path.join(GAME, rel)
        with open(path) as f:
            src = f.read()
        if src.count(old) != 1:
            results.append((i, "INVALID", what + " (pattern matches %d times)" % src.count(old)))
            print("%2d %-8s %s (pattern matches %d times)" % (i, "INVALID", what, src.count(old)), flush=True)
            continue
        out = ""
        try:
            with open(path, "w") as f:
                f.write(src.replace(old, new))
            rc, out = run(["make", target], TESTS)
            if rc == 0:
                rc, out = run(cmd, TESTS)
                status = "KILLED" if rc != 0 else "SURVIVED"
            else:
                status = "INVALID"
                out = out[-600:]
        finally:
            with open(path, "w") as f:
                f.write(src)
            os.utime(path, None)  # newer than the mutant's build, so the next make rebuilds from the real source
        results.append((i, status, what))
        print("%2d %-8s %s" % (i, status, what), flush=True)
        if status == "INVALID":
            print(out)
    for t in sorted(set(m[3][0] for m in M)):
        run(["make", t], TESTS)
    k = sum(1 for r in results if r[1] == "KILLED")
    for what, why in EQUIVALENT:
        print("equivalent, not run: %s -- %s" % (what, why))
    print("mutations: %d run, %d killed, %d survived, %d invalid" % (len(results), k, sum(1 for r in results if r[1] == "SURVIVED"), sum(1 for r in results if r[1] == "INVALID")))
    return 0 if k == len(results) else 1


if __name__ == "__main__":
    sys.exit(main())
