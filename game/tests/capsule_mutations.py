#!/usr/bin/env python3
"""Mutation check of the capsule-machine presentation (task C07), the same method as radar_mutations.py.

Each mutation is one textual change to a file under game/source, applied in place, built into the host test that should notice it, run,
and always restored. KILLED: it builds and the test fails. SURVIVED: it builds and the test still passes. INVALID: the pattern did not
match exactly once, or the build failed (nothing was shown either way). Exit 1 on any SURVIVED or INVALID.

  python3 game/tests/capsule_mutations.py            # every mutation
  python3 game/tests/capsule_mutations.py 3 7        # only these numbers

The groups: the fixture contract, the slots, the physics and the units of time (a dt read as seconds, a bounce that is not damped, a
deadline that is not enforced), the pull and the skip, the once-only settle, the callbacks (once, never from leave, never after it), the
held-button rule, the pause before a button closes the results, and the debug entry.
"""
import os
import subprocess
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
GAME = os.path.join(ROOT, "game")
TESTS = os.path.join(GAME, "tests")

MD = "source/capsule_model.c"
SC = "source/capsule_screen.c"
CT = ("capsule_test", ["build/capsule_test", "300"])
FT = ("flowtest", ["build/flowtest"])

# (file under game/, old text, new text, (make target, command), what the mutation is)
M = [
    # ---- the fixture contract
    (MD, "  if (!f || f->n < 1 || f->n > CAP_MAX) return false;", "  if (!f || f->n < 1 || f->n > CAP_MAX + 1) return false;", CT, "26 results are accepted"),
    (MD, "  if (!f || f->n < 1 || f->n > CAP_MAX) return false;", "  if (!f || f->n > CAP_MAX) return false;", CT, "an empty preview is accepted"),
    (MD, "    if (f->item[i].tier >= CAP_TIERS) return false;", "", CT, "a tier out of range is accepted"),
    (MD, "    if (!nul || nul == f->item[i].label) return false;", "    if (!nul) return false;", CT, "an empty label is accepted"),
    (MD, "    if (!nul || nul == f->item[i].label) return false;", "    if (nul == f->item[i].label) return false;", CT, "a label with no end is accepted"),
    (MD, "  m->fx = *f;", "  m->fx = *f;\n  m->fx.n = f->n > 1 ? f->n - 1 : f->n;", CT, "the model drops a result"),
    # ---- the slots
    (MD, "  const int in_row = row == rows - 1 ? n - row * CAP_COLS : CAP_COLS;", "  const int in_row = CAP_COLS;", CT, "a short last row is not centred"),
    (MD, "  *y = FLOOR_Y - (float)(rows - 1 - row) * SLOT_DY;", "  *y = FLOOR_Y - (float)row * SLOT_DY;", CT, "the rows are stacked the wrong way up"),
    (MD, "#define SLOT_DX 40.0f", "#define SLOT_DX 14.0f", CT, "the capsules overlap in their slots"),
    # ---- the motion and its units
    (MD, "#define GRAVITY 0.0016f", "#define GRAVITY 0.00016f", CT, "gravity is ten times weaker (nothing lands before the deadline)"),
    (MD, "#define GRAVITY 0.0016f", "#define GRAVITY 0.016f", CT, "gravity is ten times stronger"),
    (MD, "#define RESTITUTION 0.35f", "#define RESTITUTION 0.0f", CT, "the capsules do not bounce"),
    (MD, "#define RESTITUTION 0.35f", "#define RESTITUTION 0.95f", CT, "the bounce is not damped (it never settles by itself)"),
    (MD, "      b->open += dt / CAP_OPEN_MS;", "      b->open += dt / 1000.0f;", CT, "dt is read as seconds when a capsule opens"),
    (MD, "      b->vy += GRAVITY * dt;", "      b->vy += GRAVITY * dt * 1000.0f;", CT, "dt is read as seconds in the gravity"),
    (MD, "    b->spawn_ms = CAP_PULL_MS + CAP_SPAWN_GAP_MS * (float)(f->n - 1 - i);", "    b->spawn_ms = CAP_PULL_MS + CAP_SPAWN_GAP_MS * (float)(f->n - 1 - i) * 10.0f;", CT, "the capsules leave the chute ten times slower"),
    (MD, "  if (m->t >= CAP_DEADLINE_MS) {\n    settle_all(m);\n    return;\n  }", "", CT, "the deadline is not enforced"),
    (MD, "  if (dt_ms > CAP_DT_CLAMP_MS) dt_ms = CAP_DT_CLAMP_MS;", "", CT, "a long pause is not clamped"),
    (MD, "  if (!(dt_ms > 0)) return;  /* zero, negative and NaN */", "  if (dt_ms < 0) return;", CT, "a NaN dt is integrated"),
    (MD, "    m->acc -= CAP_STEP_MS;\n    advance(m, CAP_STEP_MS);", "    m->acc -= CAP_STEP_MS;\n    advance(m, dt_ms);", CT, "the step is the frame's dt, not a fixed one"),
    (MD, "    b->x = CHUTE_X + (float)((i * 37) % 61 - 30);", "    b->x = CHUTE_X + (float)(rand() % 61 - 30);", CT, "the spread at the chute is random"),
    # ---- the pull, the skip and the settle
    (MD, "  if (!m || m->stage != CAP_IDLE) return false;\n  m->stage = CAP_PULL;", "  if (!m) return false;\n  m->stage = CAP_PULL;", CT, "the pull is accepted twice"),
    (MD, "  if (!m || (m->stage != CAP_PULL && m->stage != CAP_DROP)) return false;\n  m->skipped = true;", "  if (!m || m->stage == CAP_SETTLED) return false;\n  m->skipped = true;", CT, "a skip before the pull is accepted"),
    (MD, "  if (!m || m->stage != CAP_SETTLED || m->settled_seen) return false;", "  if (!m || m->stage != CAP_SETTLED) return false;", CT, "the settle is reported every time it is asked"),
    (MD, "    if (b->phase == 2 && b->open < 1.0f) {", "    if (b->phase == 2 && b->open < 1.0f && 0) {", CT, "a capsule never opens by itself"),
    # ---- the screen: input
    (SC, "  if (ui_input_repeating()) return true; /* a held button does its action once */\n", "", CT, "a held button repeats its action"),
    (SC, "        if (cap_pull(&C.m)) ui_play_select();", "        cap_pull(&C.m);", CT, "the pull makes no sound"),
    (SC, "        leave_screen(CAPS_EXIT_BEFORE_REVEAL);", "        leave_screen(CAPS_EXIT_AFTER_REVEAL);", CT, "leaving before the pull says the reveal was shown"),
    (SC, "      if (b == BTN_CANCEL || b == BTN_MENU) {\n        if (cap_skip(&C.m)) {", "      if (b == BTN_CANCEL) {\n        if (cap_skip(&C.m)) {", CT, "START does not skip"),
    (SC, "          if (cap_take_settled(&C.m)) fire_settled();\n        }\n        return true;", "        }\n        return true;", CT, "a skip does not report the settle until the next frame"),
    (SC, "#define DWELL_MS 300.0f", "#define DWELL_MS 0.0f", CT, "a button closes the results at once"),
    (SC, "        if (to >= 0 && to < n) {", "        if (to >= 0) {", CT, "the list scrolls past its last result"),
    (SC, "          if (C.sel < C.scroll) C.scroll = C.sel;\n", "", CT, "the list does not scroll back up"),
    (SC, "          if (C.sel >= C.scroll + LIST_ROWS) C.scroll = C.sel - LIST_ROWS + 1;\n", "", CT, "the list does not scroll down"),
    # ---- the screen: callbacks and lifetime
    (SC, "  if (!C.open || C.exited) return;\n  cap_step(&C.m, dt);", "  if (C.exited) return;\n  cap_step(&C.m, dt);", CT, "a screen that was left keeps running"),
    (SC, "  C.open = true; /* the screen is the top one now", "  C.open = false; /* the screen is the top one now", CT, "the screen is never open"),
    (SC, "  C.open = false; /* no callback from here: whoever left the screen knows */", "  C.open = false;\n  if (C.a.on_exit) C.a.on_exit(CAPS_EXIT_BEFORE_REVEAL, C.a.arg);", CT, "leaving calls on_exit"),
    (SC, "  if (!a || !cap_fixture_valid(a->fixture)) return false;", "  if (!a) return false;", CT, "an invalid fixture opens the screen"),
    (SC, "  cap_init(&C.m, a->fixture);\n  C.a = *a;", "  cap_init(&C.m, a->fixture);", CT, "the callbacks are not kept"),
    (SC, "  C.exited = true;\n  if (C.a.on_exit)", "  if (C.a.on_exit)", CT, "on_exit can be called twice"),
    # ---- the debug entry
    ("source/app_flow.c", "  if (g_capsule_preview > 0)\n    show_capsule_preview(g_capsule_preview);\n  else\n    show_title();", "  show_title();", FT, "the debug entry opens nothing"),
    ("source/app_flow.c", "static void on_capsule_exit(caps_exit how, void* arg) {\n  (void)how;\n  (void)arg;\n  show_title();", "static void on_capsule_exit(caps_exit how, void* arg) {\n  (void)how;\n  (void)arg;", FT, "leaving the preview goes nowhere"),
]

# Changes that cannot change what the code does, and why (listed, not run)
EQUIVALENT = [
    ("the step limit CAP_MAX_STEPS removed from cap_step's loop",
     "one call is clamped to 100 ms, which is 12 steps of 8 ms: the limit of 16 can never be reached. It stays as a visible bound."),
    ("the screen's `if (C.settled_fired) return;` in fire_settled removed",
     "the model reports the settle once (cap_take_settled), and the only callers take it first; the screen's guard repeats that."),
    ("the `if (C.exited) return;` in leave_screen removed",
     "input is refused once exited, and no other caller exists: the guard repeats that."),
    ("app_debug_capsule's clamp of the count to 1..25 removed",
     "show_capsule_preview still refuses a count capsule_debug_fixture rejects and falls back to the title (flowtest's fake makes the same refusal); the clamp repeats that."),
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
