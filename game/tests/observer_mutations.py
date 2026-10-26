#!/usr/bin/env python3
"""Mutation check of the combat observer's wiring (task C03).

Each mutation is one textual change to a file under game/source, applied in place, built into the host test that should notice it, run,
and always restored. A mutation is KILLED when its test builds and fails, SURVIVED when it builds and the test still passes, INVALID when
the pattern did not match or the build failed (so nothing was shown either way). Exit 1 on any SURVIVED or INVALID.

  python3 game/tests/observer_mutations.py            # every mutation
  python3 game/tests/observer_mutations.py 3 7        # only these numbers

The first group removes or misplaces the observation the way a careless edit would: dropping the call in drain(), observing twice,
ending without a report, losing the report on its way to the result screen. The rest change what the observer or the screen says.
"""
import os
import subprocess
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
GAME = os.path.join(ROOT, "game")
TESTS = os.path.join(GAME, "tests")

# (file under game/, old text, new text, make target, command to run, what the mutation is)
M = [
    ("source/run_screen.c",
     "  co_observe(++G.drain_no, G.q, G.nq, gc_run_battle_event_data(&G.run), G.run.n_battle_events, G.run.battle_events_overflow, G.run.ev_overflow);\n",
     "",
     "runtest", ["build/runtest", "60", "0"], "drain() no longer gives the raw events to the observer"),
    ("source/run_screen.c",
     "  co_observe(++G.drain_no, G.q, G.nq, gc_run_battle_event_data(&G.run), G.run.n_battle_events, G.run.battle_events_overflow, G.run.ev_overflow);\n",
     "  co_observe(++G.drain_no, G.q, G.nq, gc_run_battle_event_data(&G.run), G.run.n_battle_events, G.run.battle_events_overflow, G.run.ev_overflow);\n"
     "  co_observe(++G.drain_no, G.q, G.nq, gc_run_battle_event_data(&G.run), G.run.n_battle_events, G.run.battle_events_overflow, G.run.ev_overflow);\n",
     "runtest", ["build/runtest", "60", "0"], "every drain is observed twice (fresh numbers each time)"),
    ("source/run_screen.c",
     "  co_finish_run(kind == GC_SCREEN_CLEAR, &G.run);\n",
     "",
     "runtest", ["build/runtest", "60", "0"], "the run ends without taking the report"),
    ("source/run_screen.c",
     "  co_finish_run(kind == GC_SCREEN_CLEAR, &G.run);\n",
     "  co_finish(kind != GC_SCREEN_CLEAR, G.run.biome, G.run.wave);\n",
     "runtest", ["build/runtest", "60", "0"], "the report gets the wrong outcome"),
    ("source/run_screen.c",
     "  co_finish_run(kind == GC_SCREEN_CLEAR, &G.run);\n",
     "  co_finish(kind == GC_SCREEN_CLEAR, G.run.biome, G.run.wave - 1);\n",
     "runtest", ["build/runtest", "60", "0"], "the report gets the wrong wave"),
    ("source/run_screen.c",
     "  reset_driver();\n  co_begin_run(false);\n",
     "  reset_driver();\n",
     "runtest", ["build/runtest", "60", "0"], "a new run does not restart the observer"),
    ("source/run_screen.c",
     "  co_begin_run(true);   /* observation starts here",
     "  co_begin_run(false);   /* observation starts here",
     "runtest", ["env", "RUNTEST_SAVE=resume", "build/runtest", "60", "0"], "a continued run is observed as if it had been seen from its start"),
    ("source/app_flow.c",
     "  A.result.report = co_report_get();",
     "  A.result.report = NULL;",
     "flowtest", ["build/flowtest"], "the result screen is not given the report"),
    ("source/result_screen.c",
     "      if (!postmortem_open(&pm)) leave_result();",
     "      leave_result();",
     "postmortem_test", ["build/postmortem_test"], "A on the result screen leaves without the post-mortem"),
    ("source/result_screen.c",
     "  const result_args again = R.a; /* result_screen_open clears R */\n  result_screen_open(&again);",
     "  leave_result();",
     "postmortem_test", ["build/postmortem_test"], "B in the post-mortem does not come back"),
    ("source/combat_observer.c",
     "  if (drain_no <= C.drain_no) {\n    C.st.replayed_drains++;\n    return;\n  }",
     "",
     "observer_test", ["build/observer_test"], "a replayed drain is counted again"),
    ("source/combat_observer.c",
     "    C.st.skipped_drains += drain_no - C.drain_no - 1;\n    C.pending_loss = true;",
     "    C.st.skipped_drains += drain_no - C.drain_no - 1;",
     "observer_test", ["build/observer_test"], "a drain that never arrived is not marked"),
    ("source/combat_observer.c",
     "        C.turn = 0; /* what happens before the first turn of a battle is turn 0 */",
     "        /* turn kept */",
     "observer_test", ["build/observer_test"], "a new battle starts on the last battle's turn number"),
    ("source/combat_observer.c",
     "        C.battle_seq++;\n",
     "",
     "observer_test", ["build/observer_test"], "turns of different battles are not told apart"),
    ("source/combat_observer.c",
     "      C.st.unrepresentable++;\n      lost = true;",
     "      C.st.unrepresentable++;",
     "observer_test", ["build/observer_test"], "an event the reducer cannot take is dropped without saying so"),
    ("source/combat_observer.c",
     "  if (n == 0 && !flags) {",
     "  if (n == 0 && !flags && 0) {",
     "observer_test", ["build/observer_test"], "a range with nothing to store makes an empty turn"),
    ("source/combat_observer.c",
     "    case PR_EV_HP:\n      kind = CC_OBS_HP;",
     "    case PR_EV_HP:\n      kind = CC_OBS_DAMAGE;",
     "observer_test", ["build/observer_test"], "an HP change is stored as a hit"),
    ("source/combat_observer.c",
     "  r->history = C.red.hist;\n",
     "",
     "observer_test", ["build/observer_test"], "the report has no history"),
    ("source/combat_observer.c",
     "  if (C.finished) return true;\n",
     "",
     "observer_test", ["build/observer_test"], "ending twice captures again"),
    ("source/combat_observer.c",
     "  if (!C.active || C.finished) return;\n  if (drain_no",
     "  if (!C.active) return;\n  if (drain_no",
     "observer_test", ["build/observer_test"], "events after the end change the report's counts"),
    ("source/combat_observer.c",
     "        if (ev->kind == CC_EV_FAINT && ev->target_side == CC_SIDE_PLAYER) {",
     "        if (ev->kind == CC_EV_FAINT) {",
     "observer_test", ["build/observer_test"], "a foe's faint counts as the player's last to fall"),
    ("source/combat_observer.c",
     "  if (nq >= GC_MAX_EVENTS) drain_loss = true;",
     "",
     "observer_test", ["build/observer_test"], "a full game-core queue is not treated as possibly lossy"),
    ("source/combat_observer.c",
     "  if (queue_overflow && !C.queue_overflow_seen) {",
     "  if (queue_overflow) {",
     "observer_test", ["build/observer_test"], "game-core's sticky overflow flag flags every later drain"),
    ("source/postmortem_screen.c",
     '    const char* lead = P.sum.exact ? "" : "At least: ";',
     '    const char* lead = "";',
     "postmortem_test", ["build/postmortem_test"], "totals of a turn that lost events read as whole"),
    ("source/postmortem_screen.c",
     "  P.page = P.rep->history.n_turns > 0 ? P.rep->history.n_turns - 1 : 0;",
     "  P.page = 0;",
     "postmortem_test", ["build/postmortem_test"], "the post-mortem opens on the oldest turn"),
    ("source/postmortem_screen.c",
     '    } else snprintf(t, sizeof t, "Final blow: source not recorded");',
     '    } else snprintf(t, sizeof t, "Final blow: the last attacker");',
     "postmortem_test", ["build/postmortem_test"], "the top screen claims a final blow"),
    ("source/postmortem_screen.c",
     "      if (P.scroll < P.n_rows - VISIBLE) P.scroll++;",
     "      P.scroll++;",
     "postmortem_test", ["build/postmortem_test"], "scrolling runs past the last row"),
    ("source/postmortem_screen.c",
     '  if (t->flags & CC_TF_SOURCE_LOSS) add_phrase(why, sizeof why, "some events were not observed");',
     "",
     "postmortem_test", ["build/postmortem_test"], "a turn that lost events carries no notice"),
    ("source/postmortem_screen.c",
     "  if (P.totals[0][0]) line(10, 44, P.totals[0], TS_INSTRUCTIONS_TEXT, 0);",
     "  if (P.totals[0][0]) line(10, 44, P.totals[0], TS_MOVE_INFO_CONTENT, 0);",
     "postmortem_test", ["build/postmortem_test"], "the totals are drawn in the 9 px style that was unreadable in Azahar"),
    ("source/postmortem_screen.c",
     "P.n_rows > VISIBLE ? LIST_W - 72 : LIST_W",
     "LIST_W",
     "postmortem_test", ["build/postmortem_test"], "a long notice runs into the 'more above' label on a scrolling turn"),
    # ---- the custom store's wiring (task C04)
    ("source/app_flow.c",
     "    const cs_result stored = cs_commit(A.result.report);",
     "    const cs_result stored = {CS_STORED, CC_OK, {0}};",
     "flowtest", ["build/flowtest"], "the run's end does not store its report"),
    ("source/app_flow.c",
     "    cs_commit(A.kept);\n",
     "",
     "flowtest", ["build/flowtest"], "leaving the result screen does not retry a refused write"),
    ("source/app_flow.c",
     "    if (stored.outcome == CS_WRITE_FAILED) A.kept = A.result.report;",
     "",
     "flowtest", ["build/flowtest"], "a refused write is not kept for the retry"),
    ("source/app_flow.c",
     "  co_set_uid_source(cs_begin_run);\n",
     "",
     "flowtest", ["build/flowtest"], "runs get session uids: nothing is ever stored"),
    ("source/app_flow.c",
     "    A.result.note = A.note[0] ? A.note : NULL;",
     "    A.result.note = NULL;",
     "flowtest", ["build/flowtest"], "the player is never told a report was not kept"),
    ("source/app_flow.c",
     "stored.outcome == CS_FILE_UNUSABLE || stored.outcome == CS_NO_DURABLE_RUN",
     "stored.outcome == CS_FILE_UNUSABLE",
     "flowtest", ["build/flowtest"], "an unusable file's note loses its reason (the run had no durable uid)"),
    ("source/custom_store.c",
     "  return write_stage() ? uid : 0;   /* the counter is on the card before anyone sees the uid */",
     "  write_stage();\n  return uid;",
     "store_test", ["build/store_test"], "a uid is handed out although its write failed"),
    ("source/custom_store.c",
     "  memcpy(&g_live, &g_stage, sizeof g_live);\n  S.state = CS_LOADED;",
     "  S.state = CS_LOADED;",
     "store_test", ["build/store_test"], "a successful write does not become the state"),
    ("source/custom_store.c",
     "  if (do_write(g_buf, n) != SAVE_OK) return false;\n  memcpy(&g_live",
     "  do_write(g_buf, n);\n  memcpy(&g_live",
     "store_test", ["build/store_test"], "a refused write still changes the state in memory"),
    ("source/custom_store.c",
     "  if (out.commit.replayed) {\n    out.outcome = CS_REPLAYED;\n    return out;\n  }\n",
     "",
     "store_test", ["build/store_test"], "a replayed run is written again"),
    ("source/custom_store.c",
     "  if (S.state == CS_UNREADABLE) {\n    out.outcome = CS_FILE_UNUSABLE;\n    return out;\n  }\n",
     "",
     "store_test", ["build/store_test"], "an unusable file is written over by a commit"),
    # not listed: dropping the `CS_UNREADABLE` guard in cs_begin_run changes nothing, because the zeroed state's counters make
    # cc_alloc_run_uid refuse anyway (the guard is a second line of defence, not the only one)
    ("source/custom_store.c",
     "  if (!r->uid_durable) {\n    out.outcome = CS_NO_DURABLE_RUN;\n    return out;\n  }\n",
     "",
     "store_test", ["build/store_test"], "a report under a session uid is stored"),
    ("source/custom_store.c",
     "  memcpy(&g_stage, &g_live, sizeof g_stage);\n  out.status = cc_commit_terminal(",
     "  out.status = cc_commit_terminal(",
     "store_test", ["build/store_test"], "a commit is staged on whatever the last stage left, not on the state"),
    ("source/custom_store.c",
     "  t.run_uid = r->run_uid;\n",
     "",
     "store_test", ["build/store_test"], "the stored report loses its run uid"),
    ("source/custom_store.c",
     "  t.biome = (uint8_t)r->biome;\n",
     "",
     "store_test", ["build/store_test"], "the stored report loses its biome"),
    ("source/custom_store.c",
     "    unreadable(\"was made by a different version of the game\");",
     "    cc_meta_init(&g_live, S.nonce ? S.nonce : 1u);",
     "store_test", ["build/store_test"], "a file of another version is treated as an empty one"),
]


def run(cmd, cwd, timeout=900):
    p = subprocess.run(cmd, cwd=cwd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, universal_newlines=True, timeout=timeout)
    return p.returncode, p.stdout


def main():
    want = set(int(a) for a in sys.argv[1:])
    results = []
    for i, (rel, old, new, target, cmd, what) in enumerate(M, 1):
        if want and i not in want:
            continue
        path = os.path.join(GAME, rel)
        with open(path) as f:
            src = f.read()
        if src.count(old) != 1:
            results.append((i, "INVALID", what + " (pattern matches %d times)" % src.count(old)))
            continue
        try:
            with open(path, "w") as f:
                f.write(src.replace(old, new))
            rc, out = run(["make", "-B", target] if False else ["make", target], TESTS)
            built = rc == 0
            if built:
                # make only rebuilds when a listed source is newer: the mutated file was just written, so it is
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
    # rebuild the real tests so build/ matches the source again
    for t in sorted(set(m[3] for m in M)):
        run(["make", t], TESTS)
    k = sum(1 for r in results if r[1] == "KILLED")
    print("mutations: %d run, %d killed, %d survived, %d invalid" % (len(results), k, sum(1 for r in results if r[1] == "SURVIVED"), sum(1 for r in results if r[1] == "INVALID")))
    return 0 if k == len(results) else 1


if __name__ == "__main__":
    sys.exit(main())
