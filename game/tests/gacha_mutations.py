#!/usr/bin/env python3
"""Mutation check of the capsule machine (task C09), the same method as profile_tx_mutations.py and capsule_mutations.py.

Each mutation is one (or, where two guards protect the same thing, two) textual change to files under game/source, applied in place,
built into the two host tests that should notice it (gacha_host_test: the purchase logic against game-core and the fault matrices;
gacha_screen_test: the menu driven by buttons over the real capsule screen), run, and always restored. KILLED: it builds and a test
fails (the name says which). SURVIVED: it builds and both pass. INVALID: a pattern did not match exactly once, or the build failed.
Exit 1 on any SURVIVED or INVALID.

  python3 game/tests/gacha_mutations.py            # every mutation
  python3 game/tests/gacha_mutations.py 3 7        # only these numbers

The groups: the table read from the source (cost, capacity, order of the two refusals), the purchase (what is sent to the profile
transaction, the guards against a second one, the callbacks that mark eggs shown), the screen (the question's default, held buttons,
stale input, the refusal rows, the words), the result (display order, tier) and the restart paths (finish what was begun, show what
was not shown). Two guards that each hide the other's absence (the host's "unfinished or unshown" check and the screen's "left
screens take no input") are removed together as well as alone.
"""
import os
import subprocess
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
GAME = os.path.join(ROOT, "game")
TESTS = os.path.join(GAME, "tests")

GH = "source/gacha_host.c"
GS = "source/gacha_screen.c"
CS = "source/capsule_screen.c"

# (list of (file under game/, old text, new text), what the mutation is)
M = [
    # ---- the table and the refusal order
    ([(GH, "  if (s->n_eggs < 0 || s->n_eggs + (int)r->pulls > H.capacity) return \"egg:tooManyEggs\";\n", "")],
     "the capacity refusal is dropped from the table"),
    ([(GH, "s->voucher_counts[r->voucher] < r->cost", "s->voucher_counts[r->voucher] <= r->cost")],
     "an exact balance cannot buy"),
    ([(GH, "  if (s->n_eggs < 0 || s->n_eggs + (int)r->pulls > H.capacity) return \"egg:tooManyEggs\";\n  if (s->voucher_counts[r->voucher] < r->cost) return \"egg:notEnoughVouchers\";\n",
           "  if (s->voucher_counts[r->voucher] < r->cost) return \"egg:notEnoughVouchers\";\n  if (s->n_eggs < 0 || s->n_eggs + (int)r->pulls > H.capacity) return \"egg:tooManyEggs\";\n")],
     "cost is checked before capacity (the source checks capacity first)"),
    ([(GH, "(uint8_t)(1000 - sys->voucher_counts[debited])", "(uint8_t)1")],
     "every option costs one"),
    ([(GH, "    H.rule[c].pulls = (uint8_t)pulls;", "    H.rule[c].pulls = (uint8_t)(pulls + 1);")],
     "an option gives one egg more than the source does"),
    ([(GH, "    if (ok) H.capacity = hi;", "    if (ok) H.capacity = hi + 1;")],
     "the capacity is one too high"),
    # ---- the purchase
    ([(GH, "    const char* refusal = preflight(sysd_get(), option);", "    const char* refusal = NULL;")],
     "a refusal is not decided before anything is written (it would begin a transaction to be refused)"),
    ([(GH, "    if (ptx_pending() || gh_unrevealed(NULL)) break;   /* GH_BUSY: the one before it is not done */\n", "")],
     "a purchase can start while another is unfinished or its eggs are not shown"),
    ([(GH, "  if (H.in_call) return;\n  H.in_call = true;\n  do {", "  H.in_call = true;\n  do {")],
     "a purchase can start from inside a purchase"),
    ([(GH, "    ptx_set_scene_seed(seed);   /* used only when the file is new, once */\n", "")],
     "the first purchase has no scene seed"),
    ([(GH, "    env_seed(seed);\n    const uint64_t now = env_now();", "    const uint64_t now = env_now();")],
     "the request's entropy seed is the scene's"),
    ([(GH, "    req.timestamp_ms = (double)now <= TS_MAX ? now : 0;", "    req.timestamp_ms = 0;")],
     "the clock is not the app's"),
    ([(GH, "    req.machine = (uint8_t)machine;", "    req.machine = 0;")],
     "every purchase is made on the first machine"),
    ([(GH, "    req.option = (uint8_t)option;", "    req.option = 0;")],
     "every purchase is the first option"),
    ([(GH, "      case PTX_E_IO:\n      case PTX_E_NO_MEMORY:\n        out->status = GH_RETRY_IO;", "      case PTX_E_IO:\n      case PTX_E_NO_MEMORY:\n        out->status = GH_BAD;")],
     "a refused write is reported as a bad request"),
    ([(GH, "    if (sysd_state(swhy) == SYSD_UNREADABLE) {   /* the defaults", "    if (false) {   /* the defaults")],
     "an unreadable profile is treated as an empty wallet"),
    ([(GH, "        o->result = *r;\n", "")],
     "a purchase hands back no result"),
    # ---- showing and marking
    ([(GH, "  return ptx_mark_revealed(txn);", "  return true;")],
     "shown eggs are never recorded as shown"),
    ([(GH, "  if (txn > H.shown_txn) H.shown_txn = txn;\n", "")],
     "a refused mark does not stop the same eggs being offered again in the session"),
    ([(GH, "    if (u > H.shown_txn) {", "    if (true) {")],
     "eggs shown in this session are offered again"),
    ([(GH, "    const int idx = r->order[k];", "    const int idx = k;")],
     "the eggs are shown in storage order, not the source's display order"),
    ([(GH, "    f->item[k].tier = e->tier;", "    f->item[k].tier = 0;")],
     "every egg is shown as common"),
    ([(GH, "    if (idx >= r->n || seen[idx]) {", "    if (idx >= r->n) {")],
     "a display order with a repeated egg is accepted"),
    ([(GH, "    const char* name = H.species_name && e->species ? H.species_name(e->species) : NULL;", "    const char* name = NULL;")],
     "the rows name no species (the egg's id again)"),
    ([(GH, "    const char* name = H.species_name && e->species ? H.species_name(e->species) : NULL;", "    const char* name = H.species_name && e->species ? H.species_name(f->n ? f->item[0].tier : 1) : NULL;")],
     "the row names the wrong egg's species"),
    ([(GS, "  c.machine = r->n && r->eggs[0].source_type < GH_MACHINES ? r->eggs[0].source_type : 0;", "  c.machine = 0;")],
     "the reveal always draws machine 0"),
    ([(GS, "  gh_set_species_names(a->species_name);\n", "")],
     "the screen never hands the app's namer to the host"),
    # ---- the screen
    ([(GS, "          G.mode = GS_CONFIRM;\n          G.yes = 0;", "          G.mode = GS_CONFIRM;\n          G.yes = 1;")],
     "the question opens on Yes"),
    ([(GS, "        G.yes = !G.yes;", "        G.yes = 1;")],
     "the question cannot go back to No"),
    ([(GS, "  if (ui_input_repeating() && !(nav && G.mode == GS_MENU)) return true; /* a held button does its action once */\n", "")],
     "a held button repeats its action"),
    ([(GS, "  if (!G.open || G.left) return true;", "  if (G.left) return true;")],
     "a screen that was left still takes input"),
    ([(GS, "  if (!G.open || G.left) return true;", "  if (G.left) return true;"),
     (GH, "    if (ptx_pending() || gh_unrevealed(NULL)) break;   /* GH_BUSY: the one before it is not done */\n", "")],
     "a left screen takes input AND a second purchase may start (a real double pull)"),
    ([(GS, "        if (G.view.opt[G.sel].can_buy) {", "        if (true) {")],
     "a row that cannot be bought asks the question"),
    ([(GS, "  gh_buy(G.machine, G.sel, &o);", "  gh_buy(0, G.sel, &o);")],
     "the machine on screen is not the machine bought from"),
    ([(GS, "  gh_buy(G.machine, G.sel, &o);", "  gh_buy(G.machine, 0, &o);")],
     "the option on screen is not the option bought"),
    ([(GS, "  gh_buy(G.machine, G.sel, &o);\n  after(&o);", "  gh_buy(G.machine, G.sel, &o);\n  gh_buy(G.machine, G.sel, &o);\n  after(&o);"),
     (GH, "    if (ptx_pending() || gh_unrevealed(NULL)) break;   /* GH_BUSY: the one before it is not done */\n", "")],
     "the callback buys twice (and nothing stops it)"),
    ([(GS, "  gh_reveal_done(K.reveal_txn);\n}", "}")],
     "the capsule screen's settled callback does not mark the eggs shown"),
    ([(GS, "  gacha_screen_open(&K.a); /* which first gives a half-applied purchase another chance to reach the card */", "  (void)0;")],
     "closing the eggs does not return to the machine"),
    ([(GS, "  c.autoplay = true;", "  c.autoplay = false;")],
     "the capsule screen waits for a second pull after the purchase"),
    ([(GS, "  c.live = true;", "  c.live = false;")],
     "the capsule screen says it is a preview"),
    ([(GS, "  gh_outcome o;\n  gh_resume(&o);\n  ptx_result r;", "  gh_outcome o;\n  o.status = GH_NOTHING;\n  ptx_result r;")],
     "opening the machine does not finish a purchase a restart left unfinished"),
    ([(GS, "o.status != GH_FATAL && gh_unrevealed(&r)) {", "o.status != GH_FATAL && false) {")],
     "opening the machine does not show eggs a restart left unshown"),
    ([(GS, "      tell(ptx_pending() ? GS_RETRY : GS_MESSAGE, text);", "      tell(GS_RETRY, text);")],
     "a purchase that could not be begun offers a retry of nothing"),
    ([(GS, "      tell(ptx_pending() ? GS_RETRY : GS_MESSAGE, text);", "      tell(GS_MESSAGE, text);")],
     "a purchase begun and not finished offers no way to finish it"),
    ([(GS, "    G.machine = (G.machine + (b == BTN_RIGHT ? 1 : GH_MACHINES - 1)) % GH_MACHINES;", "    G.machine = G.machine;")],
     "the machine cannot be changed"),
    ([(GS, "        if (to >= 0 && to < GH_OPTIONS) {", "        if (to >= 0) {")],
     "the option cursor runs past the last option"),
    ([(CS, "  if (a->autoplay) cap_pull(&C.m);", "")],
     "the capsule screen ignores autoplay"),
]

T_H = ("gacha_host_test", ["build/gacha_host_test", "300"])
T_S = ("gacha_screen_test", ["build/gacha_screen_test", "1500"])

# Changes that cannot change what the code does, and why (listed, not run)
EQUIVALENT = [
    ("the screen's `ptx_settle()` at open removed",
     "gh_resume (called next) executes or settles any pending transaction, which settles it; the first call only saves a read of the pending state."),
]


def run(cmd, cwd, timeout=900):
    p = subprocess.run(cmd, cwd=cwd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, universal_newlines=True, timeout=timeout)
    return p.returncode, p.stdout


def main():
    want = set(int(a) for a in sys.argv[1:])
    results = []
    for i, (edits, what) in enumerate(M, 1):
        if want and i not in want:
            continue
        srcs = {}
        bad = None
        for rel, old, new in edits:
            path = os.path.join(GAME, rel)
            if path not in srcs:
                with open(path) as f:
                    srcs[path] = f.read()
            n = srcs[path].count(old)
            if n != 1:
                bad = "pattern matches %d times in %s" % (n, rel)
        if bad:
            results.append((i, "INVALID", what))
            print("%2d %-8s %s (%s)" % (i, "INVALID", what, bad), flush=True)
            continue
        orig = dict(srcs)
        status, who, out = "SURVIVED", "", ""
        try:
            cur = dict(srcs)
            for rel, old, new in edits:
                path = os.path.join(GAME, rel)
                cur[path] = cur[path].replace(old, new)
            for path, text in cur.items():
                with open(path, "w") as f:
                    f.write(text)
            rc, out = run(["make", "-j2", T_H[0], T_S[0]], TESTS)
            if rc != 0:
                status, out = "INVALID", out[-800:]
            else:
                for name, cmd in (("host", T_H[1]), ("screen", T_S[1])):
                    rc, o2 = run(cmd, TESTS)
                    if rc != 0:
                        status, who = "KILLED", name
                        break
        finally:
            for path, text in orig.items():
                with open(path, "w") as f:
                    f.write(text)
                os.utime(path, None)  # newer than the mutant's build, so the next make rebuilds from the real source
        results.append((i, status, what))
        print("%2d %-8s %-6s %s" % (i, status, who, what), flush=True)
        if status == "INVALID":
            print(out)
    run(["make", "-j2", T_H[0], T_S[0]], TESTS)
    k = sum(1 for r in results if r[1] == "KILLED")
    for what, why in EQUIVALENT:
        print("equivalent, not run: %s -- %s" % (what, why))
    print("mutations: %d run, %d killed, %d survived, %d invalid" % (len(results), k, sum(1 for r in results if r[1] == "SURVIVED"), sum(1 for r in results if r[1] == "INVALID")))
    return 0 if k == len(results) else 1


if __name__ == "__main__":
    sys.exit(main())
