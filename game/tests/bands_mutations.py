#!/usr/bin/env python3
"""Mutation check of the party and summary screens' background in the bottom screen's bands (party_bands_test.c, summary_ui_test.c), the same
method as bottom_view_mutations.py.

Each mutation is one textual change to a file under game/, applied in place, built into the host test that should notice it, run, and always
restored. KILLED: it builds and the test fails. SURVIVED: it builds and the test still passes. INVALID: the pattern did not match exactly
once, or the build failed (nothing was shown either way). Exit 1 on any SURVIVED or INVALID.

  python3 game/tests/bands_mutations.py            # every mutation
  python3 game/tests/bands_mutations.py 3 7        # only these numbers

Not covered: the art's pixels (not in the repo): that the rows copied really are plain grid, that the header's dark row is row 8.
Left out as equivalent: drawing a page's lower band the screen's width instead of the page's (draw_row_below(key, px, MUI_W)): a page never
sits left of PAGE_X (106), and from there its 214 px reach the screen's edge, so both clip to the same rectangle for every x it can have.
"""
import os
import subprocess
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
TESTS = os.path.join(ROOT, "game", "tests")

PC = "game/menus/party.c"
SC = "game/menus/summary.c"
PT = (TESTS, "party_bands_test", ["build/party_bands_test"])
ST = (TESTS, "summary_ui_test", ["build/summary_ui_test"])

# (file under the repo, old text, new text, (cwd, make target, command), what the mutation is)
M = [
    # ---- party: the background copies
    (PC, "  draw_bg_bands(bg);\n  mui_image(bg, NULL, 0, Y(0), 0, 1);", "  mui_image(bg, NULL, 0, Y(0), 0, 1);", PT, "no band is drawn"),
    (PC, "  draw_bg_bands(bg);\n  mui_image(bg, NULL, 0, Y(0), 0, 1);", "  mui_image(bg, NULL, 0, Y(0), 0, 1);\n  draw_bg_bands(bg);", PT, "the bands go over the canvas' own background"),
    (PC, "  mui_clip(0, -MUI_BAND, MUI_W, MUI_BAND + 1);\n  for", "  for", PT, "the top band has no clip: the copies spill over the canvas"),
    (PC, "  mui_clip(0, MUI_H, MUI_W, MUI_BAND);\n  for", "  mui_clip(0, MUI_H + 1, MUI_W, MUI_BAND);\n  for", PT, "the bottom clip starts a row low"),
    (PC, "  mui_clip(0, -MUI_BAND, MUI_W, MUI_BAND + 1);", "  mui_clip(0, -MUI_BAND, MUI_W, MUI_BAND);", PT, "the top clip leaves the canvas' first row (empty in the art) black"),
    (PC, "  mui_clip(0, MUI_H, MUI_W, MUI_BAND);", "  mui_clip(0, MUI_H, MUI_W, MUI_BAND - 1);", PT, "the bottom clip is a row short"),
    (PC, "  mui_clip(0, -MUI_BAND, MUI_W, MUI_BAND + 1);", "  mui_clip(0, -MUI_BAND, MUI_W - 1, MUI_BAND + 1);", PT, "the top clip is a column short"),
    (PC, "  mui_clip_off();\n}\n\nvoid party_draw", "}\n\nvoid party_draw", PT, "the clip stays on for the whole screen"),
    (PC, "#define BAND_COPIES 3\nstatic const float BAND_COPY_X[BAND_COPIES] = {0, 128, 256};", "#define BAND_COPIES 2\nstatic const float BAND_COPY_X[BAND_COPIES] = {0, 128};", PT, "two copies: the right end of the band shows bars"),
    (PC, "{0, 128, 256}", "{256, 128, 0}", PT, "the copies are drawn right to left: the bars of the first one stay"),
    (PC, "{0, 128, 256}", "{0, 130, 260}", PT, "a copy is moved by a number that is not a grid period"),
    (PC, "{0, 128, 256}", "{0, 160, 320}", PT, "the second copy leaves a gap the bars show in"),
    (PC, "#define BAND_TOP_SHIFT (-64)", "#define BAND_TOP_SHIFT (-60)", PT, "the top band's rows are off the grid"),
    (PC, "#define BAND_TOP_SHIFT (-64)", "#define BAND_TOP_SHIFT (-32)", PT, "the top band takes rows 2..32 (the art's edge and the slots' bars start there)"),
    (PC, "#define BAND_TOP_SHIFT (-64)", "#define BAND_TOP_SHIFT 64", PT, "the top band is moved down instead of up"),
    (PC, "#define BAND_BOTTOM_SHIFT 96", "#define BAND_BOTTOM_SHIFT 100", PT, "the bottom band's rows are off the grid"),
    (PC, "#define BAND_BOTTOM_SHIFT 96", "#define BAND_BOTTOM_SHIFT 0", PT, "the bottom band copies the canvas' own rows 180.. (none)"),
    (PC, "#define BAND_BOTTOM_SHIFT 96", "#define BAND_BOTTOM_SHIFT 32", PT, "the bottom band takes rows 148..178 (the last of the bars)"),
    (PC, "  for (int i = 0; i < BAND_COPIES; i++) mui_image(bg, NULL, BAND_COPY_X[i], Y(0) + BAND_BOTTOM_SHIFT, 0, 1);", "  for (int i = 0; i < BAND_COPIES; i++) mui_image(bg, NULL, BAND_COPY_X[i], Y(0) + BAND_BOTTOM_SHIFT, 0, 0);", PT, "the bottom copies are drawn from their top edge, not the bottom"),
    (PC, "  for (int i = 0; i < BAND_COPIES; i++) mui_image(bg, NULL, BAND_COPY_X[i], Y(0) + BAND_TOP_SHIFT, 0, 1);", "  for (int i = 0; i < BAND_COPIES; i++) mui_image(bg, NULL, BAND_COPY_X[i], Y(0) + BAND_TOP_SHIFT, 0, 1);\n  mui_image(\"ui/party_bg\", NULL, 0, Y(0), 0, 1);", PT, "the bands also draw the singles' background (a double-battle screen gets the wrong art)"),
    (PC, "  if (!P.active) return;\n  const char* bg = item_manage_mode()", "  const char* bg = item_manage_mode()", PT, "the bands are drawn while the screen is closed"),
    # ---- summary: above
    (SC, "  mui_clip(0, -MUI_BAND, MUI_W, MUI_BAND + 1);\n  rr_draw_opts o", "  mui_clip(0, -MUI_BAND, MUI_W, MUI_BAND);\n  rr_draw_opts o", ST, "the header's dark row stops a row short of the canvas"),
    (SC, "#define HEADER_DARK_ROW 8", "#define HEADER_DARK_ROW 14", ST, "the stretched row is the header's bottom line, not its dark middle"),
    (SC, "  o.scale_y = MUI_BAND + 1;\n  mui_image_ex(\"ui/summary_bg\", NULL, 0, -MUI_BAND", "  o.scale_y = MUI_BAND;\n  mui_image_ex(\"ui/summary_bg\", NULL, 0, -MUI_BAND", ST, "the dark row is stretched a row short"),
    (SC, "#define HEADER_LINE_ROW 2", "#define HEADER_LINE_ROW 0", ST, "the teal line copied up is the art's empty first row"),
    (SC, "  mui_clip(0, -MUI_BAND, MUI_W, 1);\n  mui_image(\"ui/summary_bg\"", "  mui_clip(0, -MUI_BAND, MUI_W, 2);\n  mui_image(\"ui/summary_bg\"", ST, "the teal line is two rows thick"),
    (SC, "static void outer_clip(void) { mui_clip(0, -MUI_BAND, MUI_W, MUI_H + 2 * MUI_BAND); }", "static void outer_clip(void) { mui_clip(0, 0, MUI_W, MUI_H); }", ST, "the mask is still the canvas: the bands are drawn, then cut"),
    (SC, "static void outer_clip(void) { mui_clip(0, -MUI_BAND, MUI_W, MUI_H + 2 * MUI_BAND); }", "static void outer_clip(void) { mui_clip(0, -MUI_BAND, MUI_W, MUI_H + MUI_BAND); }", ST, "the mask stops at the canvas' bottom"),
    # ---- summary: below
    (SC, "  mui_clip(x0, MUI_H, x1 - x0, MUI_BAND);\n  rr_draw_opts o", "  mui_clip(x0, MUI_H - 1, x1 - x0, MUI_BAND);\n  rr_draw_opts o", ST, "the lower clip starts a row up (over the canvas' last row)"),
    (SC, "  mui_clip(x0, MUI_H, x1 - x0, MUI_BAND);\n  rr_draw_opts o", "  mui_clip(x0, MUI_H, x1 - x0, MUI_BAND - 1);\n  rr_draw_opts o", ST, "the lower clip is a row short"),
    (SC, "  o.origin_y = 1; /* the art's bottom edge on y", "  o.origin_y = 0; /* the art's bottom edge on y", ST, "the stretched row is the art's first, not its last"),
    (SC, "  o.scale_y = MUI_BAND + 1;\n  mui_image_ex(key, NULL, x, MUI_H + MUI_BAND, &o);", "  o.scale_y = MUI_BAND;\n  mui_image_ex(key, NULL, x, MUI_H + MUI_BAND, &o);", ST, "the lower row is stretched a row short"),
    (SC, "  mui_image_ex(key, NULL, x, MUI_H + MUI_BAND, &o);", "  mui_image_ex(key, NULL, x, MUI_H, &o);", ST, "the lower row is stretched up over the canvas, not down"),
    (SC, "  const float x0 = x < 0 ? 0 : x, x1 = x + w > MUI_W ? MUI_W : x + w;", "  const float x0 = x, x1 = x + w;", ST, "a sliding page's band is not cut to the screen"),
    (SC, "  draw_row_below(key, px, frame_w(key));\n", "", ST, "the pages' panels do not go on into the lower band"),
    (SC, "  draw_row_below(\"ui/summary_bg\", 0, MUI_W);\n", "", ST, "the background's last row does not go on into the lower band"),
    (SC, "  draw_bg_bands(); /* leaves the mask on", "  mui_clip(0, 0, MUI_W, MUI_H); /* leaves the mask on", ST, "the summary draws no bands"),
    (SC, "    mui_clip(112, 121, 205, 59);\n    mui_text(TS_WINDOW_ALT, mx + 1, my + dy, S.move_desc);\n    outer_clip();", "    mui_clip(112, 121, 205, 59);\n    mui_text(TS_WINDOW_ALT, mx + 1, my + dy, S.move_desc);\n    mui_clip(0, 0, MUI_W, MUI_H);", ST, "the moves page puts the canvas-only mask back after a description"),
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
