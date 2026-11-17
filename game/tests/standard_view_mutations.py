#!/usr/bin/env python3
"""Mutation check of the battle screen's standard view (lorenzo's Black 2 design, 2026-11-17: strip, buttons, item panel), the same method as
capsule_mutations.py.

Each mutation is one textual change to a file under game/, applied in place, built into the host test that should notice it, run, and always
restored. KILLED: it builds and the test fails. SURVIVED: it builds and the test still passes. INVALID: the pattern did not match exactly
once, or the build failed (nothing was shown either way). Exit 1 on any SURVIVED or INVALID.

  python3 game/tests/standard_view_mutations.py            # every mutation
  python3 game/tests/standard_view_mutations.py 3 7        # only these numbers

Not covered here: run_screen's refresh_header (runtest fakes the view) and main.c's battery hook (3DS only).
"""
import os
import subprocess
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
TESTS = os.path.join(ROOT, "game", "tests")

BV = "game/source/battle_view.c"
SV = (TESTS, "standard_view_test", ["build/standard_view_test"])

# Not a mutation that can be seen: a tap on the item panel falling through to the buttons under it. Every button under the panel answers with A,
# which the open panel swallows (sv_items_input), so nothing changes; the Tera button that once sat under it moved to the move screen.
# (file under the repo, old text, new text, (cwd, make target, command), what the mutation is)
M = [
    # ---- the strip
    (BV, '  if (wave > 0) snprintf(g_hdr.wave, sizeof g_hdr.wave, "Wave %d", wave);\n  else g_hdr.wave[0] = 0;', '  snprintf(g_hdr.wave, sizeof g_hdr.wave, "Wave %d", wave);', SV, "wave 0 is shown"),
    (BV, '  text_draw(TS_WINDOW, 150, y, g_hdr.biome, &o);\n', '', SV, "no biome"),
    (BV, '  text_draw(TS_WINDOW, 290, y, g_hdr.money, &o);\n', '', SV, "no money"),
    (BV, "  if (g_batt < 0) return;\n", "", SV, "an unknown battery is drawn"),
    (BV, "  const int n = g_batt > 5 ? 5 : g_batt;", "  const int n = 5;", SV, "the battery is always full"),
    (BV, "  g_batt_ms = 5000;", "  g_batt_ms = 0;", SV, "the battery is asked every frame"),
    (BV, "  if (!battery_hook) return;\n  g_batt_ms -= dt;", "  g_batt_ms -= dt;", SV, "no hook crashes (NULL call)"),
    # ---- the buttons
    (BV, "  rr_tri(x, y + h / 2, x + p, y, x + p, y + h, c);\n", "", SV, "hexagons with no left point"),
    (BV, "  return dx >= h->p || dy <= h->h / 2 * dx / h->p;", "  return true;", SV, "a tap on a point's empty corner is the button"),
    (BV, "static bool sv_cmd_ok(int c) { return c == 1 ? B.opts.can_ball : c == 3 ? B.opts.can_run : true; }", "static bool sv_cmd_ok(int c) { return true; }", SV, "a command that is off is not greyed"),
    (BV, "    rr_draw(f, SV_BALLS_X + i * 14, y, &o);", "    if (i < 3) rr_draw(f, SV_BALLS_X + i * 14, y, &o);", SV, "half the party's balls"),
    (BV, '  sv_label(&SV_SETTINGS, "Settings", 72, B.opts.on_menu ? 0 : RR_RGBA(0x90, 0x90, 0x98, 255));\n', '', SV, "no Settings label"),
    (BV, "  const bool strip_cmd = cmd_shown() && !info && !sv();", "  const bool strip_cmd = cmd_shown() && !info;", SV, "the strip keeps its command window in the standard view"),
    (BV, "  if (!B.begun || sv()) return;", "  if (!B.begun) return;", SV, "the top screen keeps the item bar"),
    (BV, "static bottom_view g_view = BV_VIEW_STANDARD;", "static bottom_view g_view = BV_VIEW_DEBUG;", SV, "the debug view is the default"),
    # ---- the D-pad
    (BV, "    if (sv() && (b == BTN_UP || b == BTN_DOWN || b == BTN_LEFT || b == BTN_RIGHT)) {", "    if (false) {", SV, "the standard view moves as the strip's 2x2 grid"),
    (BV, "  if (sv() && B.mode == M_COMMAND && (b == BTN_CYCLE_FORM || b == BTN_CYCLE_SHINY)) { /* L / R open the item panel */\n    sv_items_open();", "  if (sv() && B.mode == M_COMMAND && (b == BTN_CYCLE_FORM || b == BTN_CYCLE_SHINY)) { /* L / R open the item panel */\n    B.bar ^= 1;", SV, "L / R switch the hidden bar instead of opening the panel"),
    (BV, "  if (sv() && B.mode == M_FIGHT && (b == BTN_CYCLE_FORM || b == BTN_CYCLE_SHINY)) return false; /* no item bar on screen */\n", "", SV, "L / R switch an unseen bar in the fight menu"),
    (BV, "    case 0: return b == BTN_UP ? -2 : b == BTN_LEFT ? 1 : b == BTN_RIGHT ? 2 : b == BTN_DOWN ? 3 : -1;", "    case 0: return b == BTN_UP ? -1 : b == BTN_LEFT ? 1 : b == BTN_RIGHT ? 2 : b == BTN_DOWN ? 3 : -1;", SV, "UP from FIGHT opens nothing"),
    (BV, "    case 2: return b == BTN_UP ? 0 : b == BTN_LEFT || b == BTN_DOWN ? 3 : -1;", "    case 2: return b == BTN_UP ? 0 : b == BTN_LEFT || b == BTN_DOWN ? 1 : -1;", SV, "Pokemon's left is Ball, over Run"),
    (BV, "    case 3: return b == BTN_UP ? 0 : b == BTN_LEFT ? 1 : b == BTN_RIGHT ? 2 : -1;", "    case 3: return b == BTN_UP ? 0 : b == BTN_LEFT ? 2 : b == BTN_RIGHT ? 1 : -1;", SV, "Run's sides swapped"),
    (BV, "    if (c == 4 && (!B.opts.can_tera || sv())) c = B.cmd_cursor = 0;", "    if (c == 4 && !B.opts.can_tera) c = B.cmd_cursor = 0;", SV, "a strip Tera cursor stays on in the standard command screen"),
    # ---- the move screen
    (BV, "  if (sv_fight_shown()) {\n    sv_draw_moves();\n    return;\n  }\n", "", SV, "no move screen (the commands stay up)"),
    (BV, "static bool sv_screen_shown(void) { return sv_cmd_shown() || sv_fight_shown(); }", "static bool sv_screen_shown(void) { return sv_cmd_shown(); }", SV, "the move screen keeps the X / Y line under the strip"),
    (BV, "  return i < m->n_moves && m->pp_left[i] > 0 && B.opts.move_ok[i];", "  return i < m->n_moves && m->pp_left[i] > 0;", SV, "a refused move is not greyed"),
    (BV, '  snprintf(s, sizeof s, "PP %d/%d", m->pp_left[i], m->pp_max[i]);', '  snprintf(s, sizeof s, "PP %d/%d", m->pp_max[i], m->pp_max[i]);', SV, "PP shows the maximum twice"),
    (BV, '  sv_label(&SV_BACK, "Back", 72, 0);\n', '', SV, "no Back label"),
    (BV, "  sv_draw_balls(SV_FIGHT_BALLS_Y);\n", "", SV, "no balls on the move screen"),
    (BV, '    snprintf(s, sizeof s, "%s", B.reason); /* the move can\'t be used: say why where the description is */', '    s[0] = 0;', SV, "the refusal's reason is not shown"),
    (BV, '    snprintf(s, sizeof s, "Power %s   Accuracy %s", pw, ac);', '    snprintf(s, sizeof s, "Power %s   Accuracy %s", ac, pw);', SV, "power and accuracy swapped"),
    (BV, "  if (c == 4) return b == BTN_LEFT ? B.sv_tera_row * 2 : b == BTN_RIGHT ? B.sv_tera_row * 2 + 1 : -1;", "  if (c == 4) return b == BTN_LEFT ? 0 : b == BTN_RIGHT ? 1 : -1;", SV, "the Tera button forgets the row"),
    (BV, "  if (b == BTN_RIGHT) return c % 2 ? -1 : t ? 4 : c + 1;", "  if (b == BTN_RIGHT) return c % 2 ? -1 : c + 1;", SV, "RIGHT skips the Tera button"),
    (BV, "  if (b == BTN_LEFT) return c % 2 ? (t ? 4 : c - 1) : -1;", "  if (b == BTN_LEFT) return c % 2 ? 4 : -1;", SV, "LEFT goes to a Tera button that is not offered"),
    (BV, "  if (b == BTN_DOWN) return c < 2 ? c + 2 : -1;", "  if (b == BTN_DOWN) return c < 2 ? c + 2 : c - 2;", SV, "DOWN wraps round"),
    (BV, "      if (to == 4) B.sv_tera_row = c / 2;\n", "", SV, "the row is never kept"),
    (BV, "      B.tera_fight = !B.tera_fight;", "      B.tera_fight = true;", SV, "A on Tera cannot take it back"),
    (BV, "  if (B.mode == M_FIGHT && sv()) return sv_move_touch(px, py);\n", "", SV, "taps on the move screen go to the strip's fight menu"),
    (BV, "  if (sv_hex_hit(&SV_BACK, x, y)) return bv_input(NULL, BTN_CANCEL);\n", "", SV, "Back does nothing"),
    (BV, "  if (B.opts.can_tera && sv_hex_hit(&SV_TERA, x, y)) {", "  if (sv_hex_hit(&SV_TERA, x, y)) {", SV, "the Tera hexagon is tappable when not offered"),
    (BV, "      B.move_cursor = i;\n      return bv_input(NULL, BTN_ACTION);", "      B.move_cursor = 3 - i;\n      return bv_input(NULL, BTN_ACTION);", SV, "a tap picks the opposite move"),
    (BV, "  if (B.move_cursor == 4 && !B.opts.can_tera) B.move_cursor = 0; /* (the standard move screen's Tera button) */\n", "", SV, "the Tera cursor outlives the offer"),
    (BV, "    if (c == 4) c = B.move_cursor = 0; /* the standard view's Tera button, left with X: the strip has no such place */\n", "", SV, "the debug view keeps the move screen's Tera cursor"),
    (BV, "    draw_tera_at(SV_TERA.x + SV_TERA.w / 2, SV_TERA.y + SV_TERA.h / 2, B.tera_fight);\n", "", SV, "no Tera icon on the move screen"),
    (BV, "  if (!has) {\n    text_draw(TS_WINDOW, h->x + h->w / 2, h->y + h->h / 2, \"-\", &o);\n    return;\n  }", "  if (!has) return;", SV, "an empty slot says nothing"),
    # ---- the taps
    (BV, "  if (B.mode == M_COMMAND && sv()) return sv_touch(px, py);\n", "", SV, "taps go to the strip's command window"),
    (BV, "  if (sv_hex_hit(&SV_SETTINGS, x, y)) return bv_input(NULL, BTN_MENU);\n", "", SV, "Settings does nothing"),
    (BV, "      B.cmd_cursor = c;\n      return bv_input(NULL, BTN_ACTION);\n    }\n  return false;", "      B.cmd_cursor = (c + 1) % 4;\n      return bv_input(NULL, BTN_ACTION);\n    }\n  return false;", SV, "a tap answers the next button"),
    # ---- the item panel
    (BV, "      const int d = b == BTN_LEFT ? -1 : b == BTN_RIGHT ? 1 : b == BTN_UP ? -SV_ITEM_COLS : SV_ITEM_COLS;", "      const int d = b == BTN_LEFT ? -1 : b == BTN_RIGHT ? 1 : b == BTN_UP ? -1 : 1;", SV, "UP / DOWN move by one item, not a row"),
    (BV, "      if (B.bar_sel < 0 || i < 0 || i >= n) return false;", "      if (B.bar_sel < 0 || i < 0) return false;", SV, "the cursor runs past the last item"),
    (BV, "      B.bar ^= 1;\n      B.bar_sel = bar_n() > 0 ? 0 : -1;\n      ui_play_select();\n      return true;\n    case BTN_CANCEL:", "      B.bar_sel = bar_n() > 0 ? 0 : -1;\n      ui_play_select();\n      return true;\n    case BTN_CANCEL:", SV, "L / R do not switch the bar in the panel"),
    (BV, "  if (sv() && B.mode == M_COMMAND && B.items_on && b != BTN_MENU) return sv_items_input(b);", "  if (sv() && B.mode == M_COMMAND && B.items_on) return sv_items_input(b);", SV, "START is swallowed by the panel"),
    (BV, "  if (sv() && B.mode == M_COMMAND && B.items_on && b != BTN_MENU) return sv_items_input(b);\n", "", SV, "the panel's keys move the buttons under it"),
    (BV, "      if (i < bar_n() && i != B.bar_sel) {", "      if (i != B.bar_sel) {", SV, "a tap on an empty cell points at no item"),
    (BV, "    if (sv_hex_hit(&SV_ITEMS, x, y)) return sv_items_input(BTN_CANCEL);\n", "", SV, "Items does not close the panel"),
    (BV, "      return h == 0 ? sv_items_input(BTN_CYCLE_SHINY) : h == 1 ? sv_items_input(BTN_CANCEL) : false;", "      return h == 1 ? sv_items_input(BTN_CYCLE_SHINY) : h == 0 ? sv_items_input(BTN_CANCEL) : false;", SV, "the panel's two hints swapped"),
    (BV, "      sv_items_close(); /* the item bar's cursor and the panel are the view's own */\n", "", SV, "X leaves the panel open"),
    (BV, "  B.bar_sel = bar_n() > 0 ? 0 : -1;\n  B.bar_open = false;\n}\nstatic void sv_items_close", "  B.bar_sel = 0;\n  B.bar_open = false;\n}\nstatic void sv_items_close", SV, "an empty bar's panel has a cursor"),
    (BV, '    if (desc && *desc) text_draw(TS_MOVE_LABEL, SV_GRID_X - 5, SV_PANEL_Y + SV_PANEL_H - 26, desc, &d);\n', '', SV, "the item's effect is not shown"),
    (BV, '  if (n == 0) text_draw(TS_MOVE_LABEL, SV_GRID_X, SV_GRID_Y + 9, "none", NULL);\n', '', SV, "an empty bar says nothing"),
    (BV, "  if (B.items_on) sv_draw_items();\n", "", SV, "the panel is never drawn"),
]


def run(cmd, cwd, timeout=1800):
    p = subprocess.run(cmd, cwd=cwd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, universal_newlines=True, timeout=timeout)
    return p.returncode, p.stdout


def main():
    want = set(int(a) for a in sys.argv[1:])
    results = []
    for i, (rel, old, new, (cwd, target, cmd), what) in enumerate(M, 1):
        if want and i not in want:
            continue
        path = os.path.join(ROOT, rel)
        with open(path) as f:
            src = f.read()
        if src.count(old) != 1:
            results.append((i, "INVALID", what))
            print("%2d %-8s %s (pattern matches %d times)" % (i, "INVALID", what, src.count(old)), flush=True)
            continue
        try:
            with open(path, "w") as f:
                f.write(src.replace(old, new))
            rc, out = run(["make", target], cwd)
            if rc == 0:
                rc, out = run(cmd, cwd)
                status = "KILLED" if rc != 0 else "SURVIVED"
            else:
                status = "INVALID"
                print(out[-600:])
        finally:
            with open(path, "w") as f:
                f.write(src)
            os.utime(path, None)  # newer than the mutant's build, so the next make rebuilds from the real source
        results.append((i, status, what))
        print("%2d %-8s %s" % (i, status, what), flush=True)
    bad = [r for r in results if r[1] != "KILLED"]
    print("\n%d mutations: %d killed, %d not killed" % (len(results), len(results) - len(bad), len(bad)))
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
