#!/usr/bin/env python3
"""Mutation check of the battle screen's X (bottom-screen view) and Y (game speed) hotkeys, the same method as capsule_mutations.py.

Each mutation is one textual change to a file under game/, applied in place, built into the host test that should notice it, run, and always
restored. KILLED: it builds and the test fails. SURVIVED: it builds and the test still passes. INVALID: the pattern did not match exactly
once, or the build failed (nothing was shown either way). Exit 1 on any SURVIVED or INVALID.

  python3 game/tests/bottom_view_mutations.py            # every mutation
  python3 game/tests/bottom_view_mutations.py 3 7        # only these numbers

Not covered (bv_begin cannot be built in a host test): the held-key flags being cleared when a battle begins.
"""
import os
import subprocess
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
TESTS = os.path.join(ROOT, "game", "tests")

BV = "game/source/battle_view.c"
RS = "game/source/run_settings.c"
AF = "game/source/app_flow.c"
VT = (TESTS, "bottom_view_test", ["build/bottom_view_test"])
ST = (TESTS, "run_settings_test", ["build/run_settings_test"])
FT = (TESTS, "flowtest", ["build/flowtest"])

# Not a mutation that can be seen: with two views, stepping backwards (g_view + BV_VIEW_COUNT - 1) is stepping forwards. A third view
# makes it one worth a mutation.
# (file under the repo, old text, new text, (cwd, make target, command), what the mutation is)
M = [
    # ---- X: the view
    (BV, "      g_view = (bottom_view)((g_view + 1) % BV_VIEW_COUNT);\n", "", VT, "X changes no view"),
    (BV, "(bottom_view)((g_view + 1) % BV_VIEW_COUNT)", "(bottom_view)(g_view + 1)", VT, "the views do not wrap round"),
    (BV, "      g_view_held = true;\n", "", VT, "a held X repeats the change"),
    (BV, "  if(b==BTN_CYCLE_TERA)g_view_held=false;\n", "", VT, "X is never released"),
    (BV, "  if(b==BTN_CYCLE_TERA)g_view_held=false;\n", "  if(b==BTN_CYCLE_TERA||b==BTN_CYCLE_NATURE)g_view_held=false;\n", VT, "Y's release frees X"),
    (BV, "      g_view = (bottom_view)((g_view + 1) % BV_VIEW_COUNT);\n      ui_play_select();\n", "      g_view = (bottom_view)((g_view + 1) % BV_VIEW_COUNT);\n", VT, "X plays no sound"),
    (BV, "  if (b == BTN_CYCLE_TERA) { /* X: the next bottom-screen view, in any mode (the events play with it too) */", "  if (b == BTN_CYCLE_TERA && B.mode != M_EVENTS) { /* X */", VT, "X does nothing while the events play"),
    (BV, "  if (b == BTN_CYCLE_TERA) { /* X: the next bottom-screen view, in any mode (the events play with it too) */", "  if (b == BTN_CYCLE_TERA && B.mode != M_COMMAND) { /* X */", VT, "X does nothing in the command menu"),
    (BV, "    return true;\n  }\n  if (b == BTN_CYCLE_NATURE && speed_hook) {", "    return false;\n  }\n  if (b == BTN_CYCLE_NATURE && speed_hook) {", VT, "X reports itself unpressed"),
    # ---- Y: the speed
    (BV, "      speed_hook(true);\n", "      speed_hook(false);\n", VT, "Y does not step the speed"),
    (BV, "      speed_hook(true);\n", "      speed_hook(true);\n      speed_hook(true);\n", VT, "Y steps the speed twice"),
    (BV, "      g_speed_held = true;\n", "", VT, "a held Y repeats the step"),
    (BV, "  if(b==BTN_CYCLE_NATURE)g_speed_held=false;\n", "", VT, "Y is never released"),
    (BV, "  if(b==BTN_CYCLE_NATURE)g_speed_held=false;\n", "  if(b==BTN_CYCLE_NATURE||b==BTN_CYCLE_TERA)g_speed_held=false;\n", VT, "X's release frees Y"),
    (BV, "      speed_hook(true);\n      ui_play_select();\n", "      speed_hook(true);\n", VT, "Y plays no sound"),
    (BV, "  if (b == BTN_CYCLE_NATURE && speed_hook) {", "  if (b == BTN_CYCLE_NATURE) {", VT, "Y without a hook takes the key and calls nothing"),
    (BV, "  if (b == BTN_CYCLE_NATURE && speed_hook) {", "  if (b == BTN_CYCLE_NATURE && speed_hook && B.mode != M_EVENTS) {", VT, "Y does nothing while the events play"),
    (BV, "  if (b == BTN_CYCLE_NATURE && speed_hook) {", "  if (b == BTN_CYCLE_NATURE && speed_hook && !B.lvl.on) {", VT, "Y does nothing over the level-up window"),
    # ---- the drawing
    (BV, "  if (g_view == BV_VIEW_DEBUG) draw_debug();", "  draw_debug();", VT, "the debug rows are drawn in both views"),
    (BV, "  if (g_view == BV_VIEW_DEBUG) draw_debug();", "  if (g_view != BV_VIEW_DEBUG) draw_debug();", VT, "the debug rows are drawn in the other view"),
    (BV, "  if (speed_hook) {\n    i18n_t(name", "  if (1) {\n    i18n_t(name", VT, "the speed line is drawn without a hook"),
    (BV, "    i18n_t(name, sizeof name, speed_hook(false), NULL);", "    i18n_t(name, sizeof name, speed_hook(true), NULL);", VT, "drawing the speed line steps the speed"),
    (BV, '  snprintf(x_label, sizeof x_label, "X  Screen: %s", g_view == BV_VIEW_DEBUG ? "Debug" : "Standard");', '  snprintf(x_label, sizeof x_label, "X  Screen: %s", "Debug");', VT, "the view's name is always Debug"),
    (BV, '  snprintf(x_label, sizeof x_label, "X  Screen: %s", g_view == BV_VIEW_DEBUG ? "Debug" : "Standard");', '  snprintf(x_label, sizeof x_label, "X  Screen: %s", g_view == BV_VIEW_DEBUG ? "Standard" : "Debug");', VT, "the view's name is the other one's"),
    # ---- where the X / Y line sits (lorenzo, 2026-11-17)
    (BV, "static float bv_keys_y(void) { return g_view == BV_VIEW_DEBUG ? 34.0f : 6.0f; }", "static float bv_keys_y(void) { return 6.0f; }", VT, "the X / Y line sits on the debug rows"),
    (BV, "static float bv_keys_y(void) { return g_view == BV_VIEW_DEBUG ? 34.0f : 6.0f; }", "static float bv_keys_y(void) { return 34.0f; }", VT, "the standard view's X / Y line is not at the top"),
    (BV, "    text_draw(TS_MOVE_LABEL, BV_KEYS_X + text_width(TS_MOVE_LABEL, x_label, NULL) + BV_KEYS_GAP, bv_keys_y(), y_label, NULL);", "    text_draw(TS_MOVE_LABEL, BV_KEYS_X, bv_keys_y(), y_label, NULL);", VT, "Y's label is drawn over X's"),
    (BV, "  text_draw(TS_MOVE_LABEL, BV_KEYS_X, bv_keys_y(), x_label, NULL);", "  text_opts o = text_defaults();\n  o.origin_x = 1;\n  text_draw(TS_MOVE_LABEL, BV_KEYS_X, bv_keys_y(), x_label, &o);", VT, "X's label is right-aligned again"),
    (BV, "static void draw_bottom_extras(void) {\n  if (!ui_space(UI_PHYS_BOTTOM)) return;", "static void draw_bottom_extras(void) {", VT, "the labels are drawn on the top screen too"),
    (BV, "static bottom_view g_view = BV_VIEW_DEBUG;", "static bottom_view g_view = BV_VIEW_STANDARD;", VT, "the screen starts in the other view"),
    # ---- the speeds
    (RS, "return speed == RSET_SPEED_TURBO ? RSET_SPEED_SLOW : speed + 1;", "return speed == RSET_SPEED_TURBO ? RSET_SPEED_TURBO : speed + 1;", ST, "Turbo does not wrap to Slow"),
    (RS, "return speed == RSET_SPEED_TURBO ? RSET_SPEED_SLOW : speed + 1;", "return speed == RSET_SPEED_TURBO ? RSET_SPEED_SLOW : speed - 1;", ST, "the speeds go down"),
    (RS, "  if (speed < RSET_SPEED_SLOW || speed > RSET_SPEED_TURBO) return RSET_SPEED_NORMAL;\n  return speed == RSET_SPEED_TURBO", "  return speed == RSET_SPEED_TURBO", ST, "a speed that is no preset is stepped from"),
    (RS, "  const int speed = (int)(scale + 0.5f);", "  const int speed = (int)scale;", ST, "a scale is cut, not rounded, to a preset"),
    (RS, '{"settings:gameSpeedSlow", "settings:gameSpeedNormal", "settings:gameSpeedFast",\n                                      "settings:gameSpeedTurbo"}', '{"settings:gameSpeedSlow", "settings:gameSpeedNormal", "settings:gameSpeedTurbo",\n                                      "settings:gameSpeedFast"}', ST, "Fast and Turbo are named each other's"),
    # ---- the run settings open on the speed in force
    (AF, "      A.settings.speed = rset_speed_from_scale(ui_game_speed());", "      ;", FT, "the run settings open on the saved speed, not the one in force"),
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
