#!/usr/bin/env python3
"""Mutation check of the live Nemesis rematch (task C10), the same method as profile_tx_mutations.py and observer_mutations.py.

Each mutation is one textual change to a source file, applied in place, built into custom_run_test (which links game-core, the controller,
the store, profile_tx, the save slots and the observer for real), run, and always restored. KILLED: it builds and the test fails.
SURVIVED: it builds and the test still passes. INVALID: the pattern did not match exactly once, or the build failed. Exit 1 on any
SURVIVED or INVALID.

  python3 game/tests/custom_run_mutations.py            # every mutation
  python3 game/tests/custom_run_mutations.py 3 7        # only these numbers

The groups: the encounter hook and the answer (game-core), the offer and its answers, the enemy's identity, the result and the reward
(including the duplicate grant), the finisher's gate, the roster's release, the slot (overlay, reconciliation, clearing).
"""
import os
import subprocess
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
GAME = os.path.join(ROOT, "game")
TESTS = os.path.join(GAME, "tests")

CR = "source/custom_run.c"
CS = "source/custom_store.c"
GCE = "../game-core/src/gc_encounter.c"
GCC = "../game-core/src/gc_custom.c"
T = ("custom_run_test", ["build/custom_run_test"])

# (file under game/, old text, new text, (make target, command), what the mutation is)
M = [
    # ---- the real encounter hook and the answer (game-core)
    (GCC, "    if (!h || !h->encounter) return false;\n    return h->encounter(h->user, run, enc);", "    return false;", T,
     "the real encounter hook is never called (no offer can come)"),
    (GCE, "        if (gc_custom_report(run, &enc)) {", "        if (false) {", T, "the encounter report never asks the player"),
    (GCC, "        if (h->answer(h->user, run, accept, &spec) && accept) {", "        if (h->answer(h->user, run, accept, &spec) && false) {", T,
     "an accepted rematch builds no enemy"),
    (GCC, "            if (h->built) h->built(h->user, run, ok, ok ? \"\" : why);", "", T, "the controller is never told whether the enemy was built"),
    (GCE, "        if (run->n_enemy && run->enemy[0].b.fusion_species) enc.flags |= GC_ENF_FUSED;", "", T, "a fusion is reported as an ordinary wild encounter"),
    (GCE, "        if (nb.double_battle) enc.flags |= GC_ENF_DOUBLES;", "", T, "a double roll is reported as an ordinary wild encounter"),
    (GCE, "        else if (stand_in) enc.kind = GC_ENC_FIXED_OR_SCRIPTED;", "", T, "a stand-in for a fixed battle is reported as an ordinary wild encounter"),
    # ---- the offer and its answers
    (CR, "  R.st.offers++;\n  return true;\n}", "  R.st.offers++;\n  return false;\n}", T, "the offer is counted but never shown"),
    (CR, "  x.accept = accepted;\n  if (!run_tx(tx_decide", "  x.accept = true;\n  if (!run_tx(tx_decide", T, "a decline is recorded as an accept"),
    (CR, "  if (!accepted) {\n    R.st.declines++;\n    return false;\n  }", "  if (!accepted) {\n    R.st.declines++;\n  }", T, "a decline still builds the rematch enemy"),
    (CR, "  if (R.forced_done || R.degraded || !is_window((uint16_t)e->wave)) return false;", "  if (R.degraded || !is_window((uint16_t)e->wave)) return false;", T,
     "a run whose rematch is over is still offered one"),
    (CR, "  if (R.forced_done || R.degraded || !is_window((uint16_t)e->wave)) return false;", "  if (R.forced_done || !is_window((uint16_t)e->wave)) return false;", T,
     "a run whose roster cannot be written is still offered a rematch"),
    (CR, "    degrade(\"The answer could not be saved to the SD card, so the rematch is skipped.\");\n    return false;", "    return true;", T,
     "an answer the card refused still builds the enemy"),
    (CR, "    rep[i] = gc_custom_enemy_check(&spec, NULL) ? 1 : 0;", "    rep[i] = 1;", T, "a record the game cannot build is a candidate"),
    (CR, "  R.st.refused++;\n  write_result_ex(2, false);", "  R.st.refused++;", T, "a refused build leaves the record challenged"),
    (CR, "  R.st.refused++;\n  write_result_ex(2, false);", "  R.st.refused++;\n  write_result_ex(2, true);", T, "a refused build counts as a lost fight"),
    # ---- the enemy's identity
    (CR, "  s->has_nature = (n->known & CC_NK_NATURE) != 0;", "  s->has_nature = false;", T, "the record's nature is not applied"),
    (CR, "  s->shiny = n->shiny != 0;", "  s->shiny = false;", T, "the record's shininess is not applied"),
    (CR, "  s->gender = n->gender;", "  s->gender = -1;", T, "the record's gender is not applied"),
    (CR, "  s->form = n->form;", "  s->form = 0;", T, "the record's form is not applied"),
    (CR, "  s->ability = n->ability;", "  s->ability = 0;", T, "the record's ability is not applied (the build refuses)"),
    (CR, "  s->n_moves = n->n_moves;", "  s->n_moves = 1;", T, "only the first move of the record is applied"),
    (CR, "  for (int i = 0; i < GC_CUSTOM_MAX_MOVES && i < CC_MAX_MOVES; i++) s->moves[i] = n->moves[i];", "  for (int i = 0; i < GC_CUSTOM_MAX_MOVES && i < CC_MAX_MOVES; i++) s->moves[i] = n->moves[0];", T,
     "every move of the enemy is the record's first"),
    (GCC, "    g.form = spec->form;", "    g.form = 0;", T, "game-core builds the enemy in the default form"),
    # ---- the result and the reward
    (CR, "  if (R.rematch_live && R.enemy_down && kind != GC_SCREEN_COMMAND && kind != GC_SCREEN_GAME_OVER && kind != GC_SCREEN_CLEAR)\n    write_result(1);",
         "  if (R.rematch_live && R.enemy_down && kind != GC_SCREEN_COMMAND && kind != GC_SCREEN_GAME_OVER && kind != GC_SCREEN_CLEAR)\n    write_result(2);", T,
     "a won rematch is written as lost"),
    (CR, "  if (R.rematch_live && R.enemy_down && kind != GC_SCREEN_COMMAND", "  if (R.rematch_live && kind != GC_SCREEN_COMMAND", T,
     "a rematch is won whatever happened to its enemy (a flight or a loss is paid)"),
    (CR, "    if (R.rematch_live && R.rematch_enemy && f->fainted.id == R.rematch_enemy_id) R.enemy_down = true;", "    if (R.rematch_live) R.enemy_down = true;", T,
     "any enemy that faints is the rematch's"),
    (CR, "  if (result == 1) {\n    R.st.wins++;\n    settle_record(id, true);", "  if (result == 1) {\n    R.st.wins++;", T, "a won rematch is not paid until the next boot"),
    (CR, "  if (!sysd_save()) {\n    R.claim_pending = true;\n    return false;\n  }", "", T,
     "the claim does not first save the profile (a crash then blocks the wallet)"),
    (CR, "  uint32_t pending = ptx_pending();\n  if (pending) {", "  uint32_t pending = 0;\n  if (pending) {", T, "a claim a crash left prepared is not finished first"),
    (CR, "  if (cs_reward_state(id) != CS_CLAIM_OWED) return cs_reward_state(id) == CS_CLAIM_REPLAYED;", "  if (cs_reward_state(id) == CS_CLAIM_UNKNOWN) return false;", T,
     "the claim is attempted for a record that is not owed (the duplicate grant)"),
    (CS, "  if (e.state == CC_RW_OWED) return CS_CLAIM_OWED;", "  if (e.state == CC_RW_OWED || e.state == CC_RW_CLAIMED) return CS_CLAIM_OWED;", T,
     "a claimed record is still owed (the duplicate grant)"),
    (CR, "    if (m->roster.rec[i].life == CC_LIFE_DEFEATED) owed[n++] = m->roster.rec[i].record_id;", "    if (false) owed[n++] = m->roster.rec[i].record_id;", T,
     "a voucher a crash left unclaimed is never claimed at boot or resume"),
    (CR, "  R.st.claims++;\n  R.reward_record = id;", "  R.st.claims++;\n  R.reward_record = id;\n  sysd_get()->voucher_counts[GC_VOUCHER_REGULAR]++;", T,
     "a second voucher is added to the wallet beside the claimed one (a duplicate grant)"),
    (CR, "  if (announce) say_player(\"You got an Egg Voucher for beating the rematch!\");", "", T, "the player is not told about the voucher"),
    (CR, "  } else if (count) {\n    R.st.losses++;", "  } else if (false) {\n    R.st.losses++;", T, "a lost rematch is not counted"),
    # ---- the finisher's gate
    (CR, "  if (!c->valid || !c->direct || !c->source_enemy || c->wave != run->wave || R.enc_wave != run->wave) return false;",
         "  if (!c->valid || !c->source_enemy || c->wave != run->wave || R.enc_wave != run->wave) return false;", T, "an indirect hit (poison, weather...) names a finisher"),
    (CR, "  if (!c->valid || !c->direct || !c->source_enemy || c->wave != run->wave || R.enc_wave != run->wave) return false;",
         "  if (!c->valid || !c->direct || c->wave != run->wave || R.enc_wave != run->wave) return false;", T, "the player's own side can be the finisher"),
    (CR, "  if (!c->valid || !c->direct || !c->source_enemy || c->wave != run->wave || R.enc_wave != run->wave) return false;",
         "  if (!c->valid || !c->direct || !c->source_enemy) return false;", T, "a finisher of another wave is accepted"),
    (CR, "  if (R.enc_kind == GC_ENC_UNSUPPORTED) return false;", "", T, "an unsupported encounter names a finisher"),
    (CR, "  if (a->species != a->start_species || a->form != a->start_form) return false;", "", T, "a finisher that changed form is stored under its new species"),
    (CR, "  if (R.rematch_enemy && a->id == R.rematch_enemy_id) n->traits |= CC_TR_REMATCH;", "", T, "a rematch enemy that beats the party makes a new Nemesis"),
    (CR, "  if (R.enc_flags & GC_ENF_DOUBLES) n->traits |= CC_TR_DOUBLES;", "", T, "a double roll's finisher is stored"),
    (CR, "  if (R.enc_flags & GC_ENF_FUSED) n->traits |= CC_TR_FUSED;", "", T, "a fusion's finisher is stored"),
    (CR, "  if (R.enc_flags & GC_ENF_TERA) n->traits |= CC_TR_TERA;", "", T, "a terastallized finisher is stored"),
    (CR, "  if (a->is_boss) n->traits |= CC_TR_BOSS;", "", T, "a boss finisher is stored"),
    (CR, "  if (a->trainer_owned) n->traits |= CC_TR_TRAINER_OWNED;", "", T, "a trainer's Pokemon is stored"),
    (CR, "  n->traits = trait_of_kind(R.enc_kind);", "  n->traits = 0;", T, "a trainer, mystery, boss or fixed encounter's finisher is stored"),
    (CR, "  if (f->remaining != 0) return;\n  cand_t* c = &R.cand;", "  cand_t* c = &R.cand;", T, "any Pokemon of the party that falls is the one that ended the run"),
    (CR, "  R.cand.valid = false;\n  if (f->remaining != 0) return;", "  if (f->remaining != 0) return;", T, "a finisher stays named after the party recovered"),
    (CR, "  c->source_enemy = f->has_source && f->source_side == 1;", "  c->source_enemy = f->has_source;", T, "a hit by the player's own side is attributed to an enemy"),
    (CR, "    n->known |= CC_NK_FINISH_MOVE;", "", T, "the finishing move is not recorded"),
    (CR, "  n->known |= CC_NK_FINISHER_VERIFIED;\n", "", T, "the finisher is not marked verified"),
    (CR, "  n->orig_level = a->level;", "  n->orig_level = 1;", T, "the stored level is not the enemy's"),
    (CR, "  memcpy(n->ivs, a->ivs, 6);", "", T, "the stored IVs are not the enemy's"),
    (CR, "  n->nature = a->nature;", "  n->nature = 0;", T, "the stored nature is not the enemy's"),
    (CR, "    n->known |= CC_NK_MOVES;\n    n->n_moves = a->n_moves;", "    n->known |= CC_NK_MOVES;\n    n->n_moves = 1;", T, "only one move of the enemy is stored"),
    # ---- the roster's release
    (CR, "  x.release = true;\n  run_tx(tx_result, &x, NULL);\n  cr_settle_owed();", "  x.release = false;\n  run_tx(tx_result, &x, NULL);\n  cr_settle_owed();", T,
     "a run's end does not give back the record it held"),
    (CR, "  cc_status st = cc_roster_release_except(&m->roster, uid, NULL);", "  cc_status st = CC_OK;", T, "a resumed run does not give back the record a crashed run held"),
    # ---- the slot
    (CR, "  w32(&w, R.rematch_enemy_id);", "  w32(&w, 0);", T, "the rematch enemy's identity is not saved"),
    (CR, "(R.forced_done ? OF_DONE : 0) | (R.rematch_live ? OF_LIVE : 0) | (R.rematch_enemy ? OF_REMATCH_ENEMY : 0)", "(R.forced_done ? OF_DONE : 0) | (R.rematch_live ? OF_LIVE : 0)", T,
     "the sticky flag of a rematch enemy is not saved"),
    (CR, "      x->o.flags |= OF_DONE;\n      x->reconciled = true;", "      x->reconciled = true;", T, "an older save of a rematch already won offers it again"),
    (CR, "    if (rec.life == CC_LIFE_CHALLENGED && rec.sched_run_uid == uid) {\n      st = cc_nemesis_transition(&rec, CC_LIFE_STORED, 0, 0, 0);",
         "    if (false) {\n      st = cc_nemesis_transition(&rec, CC_LIFE_STORED, 0, 0, 0);", T, "a save from before the accept is loaded over a challenged record"),
    (CR, "      snprintf(x->why, sizeof x->why, \"The enemy this run was going to face is used by another run.\");\n      return CC_E_CONSISTENCY;",
         "      return CC_OK;", T, "a run whose record another run used up is resumed"),
    (CR, "      if (!same || run->wave != rec.challenge_wave) {", "      if (false) {", T, "a rematch whose enemy is gone is resumed as live"),
    (CR, "  if (state == CC_SCHED_NO_CANDIDATE || state == CC_SCHED_EXHAUSTED || state == CC_SCHED_DECLINED) {\n    return cc_roster_release(&m->roster, uid, NULL);\n  }",
         "  if (state == CC_SCHED_NO_CANDIDATE || state == CC_SCHED_EXHAUSTED || state == CC_SCHED_DECLINED) {\n    return CC_OK;\n  }", T,
     "a finished schedule keeps its record held after a load"),
    (CR, "  return rsv_remove_files(slot_file(slot, name));", "  return true;", T, "a cleared slot keeps its files"),
    (CR, "  if (!overlay_decode(ov, n_ov, &x.o, &carry_in)) {\n    free(raw);", "  if (false) {\n    free(raw);", T, "a damaged overlay is loaded"),
    (CR, "  if (rsv_load_bytes(core, n_core, run, &info) != RSV_OK) {", "  if (false) {", T, "damaged core bytes are loaded"),
    (CR, "  gc_run_set_custom(run, &HOOKS);\n  cr_settle_owed();", "  cr_settle_owed();", T, "a resumed run has no rematch hooks"),
    (CR, "  R.carry = carry_in;\n  R.have_carry = x.o.has_carry;", "  R.have_carry = false;", T, "the observer's turns are not carried over a resume"),
    (CR, "  if (!cr_usable(fw)) {", "  if (false) {", T, "a custom run is resumed with an unusable custom file"),
    (CR, "  if (kind == GC_SCREEN_GAME_OVER) {\n    have = build_defeat(run, out);", "  if (true) {\n    have = build_defeat(run, out);", T, "a Classic clear can name a finisher"),
    (CR, "  gc_run_set_custom(run, &HOOKS);\n  return true;\n}\n\n/* ----", "  return true;\n}\n\n/* ----", T, "a new custom run installs no hooks"),
]

# Mutations that survive and are expected to, and why (listed, run, and not counted as failures)
EXPECTED = {
    "a fusion is reported as an ordinary wild encounter":
        "game-core's wild generator never produces a fused (or terastallized) wild enemy: 0 of the 1,700+ real waves classified, up to wave 220, had one (check 2b), "
        "so no real wave reaches the line. The controller's gate for a fusion / Tera finisher is tested with synthetic reports (check 2).",
    "the claim is attempted for a record that is not owed (the duplicate grant)":
        "redundant with ptx_begin_claim's own refusal of a record that is not owed (profile_tx_mutations.py kills the mutations of that check); "
        "with this one removed the claim is still refused and the voucher is not granted again.",
}
EQUIVALENT = []


def run(cmd, cwd, timeout=1800):
    p = subprocess.run(cmd, cwd=cwd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, universal_newlines=True, timeout=timeout)
    return p.returncode, p.stdout


def main():
    want = set(int(a) for a in sys.argv[1:])
    results = []
    for i, (rel, old, new, (target, cmd), what) in enumerate(M, 1):
        if want and i not in want:
            continue
        path = os.path.normpath(os.path.join(GAME, rel))
        with open(path) as f:
            src = f.read()
        if src.count(old) != 1:
            results.append((i, "INVALID", what))
            print("%2d %-8s %s (pattern matches %d times)" % (i, "INVALID", what, src.count(old)), flush=True)
            continue
        out = ""
        try:
            with open(path, "w") as f:
                f.write(src.replace(old, new))
            rc, out = run(["make", target], TESTS)
            if rc == 0:
                rc, out = run(cmd, TESTS)
                status = "KILLED" if rc != 0 else ("EXPECTED" if what in EXPECTED else "SURVIVED")
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
    k = sum(1 for r in results if r[1] in ("KILLED", "EXPECTED"))
    for what, why in EXPECTED.items():
        if any(r[2] == what for r in results):
            print("expected survivor: %s -- %s" % (what, why))
    for what, why in EQUIVALENT:
        print("equivalent, not run: %s -- %s" % (what, why))
    print("mutations: %d run, %d killed, %d expected survivors, %d survived, %d invalid" % (len(results), sum(1 for r in results if r[1] == "KILLED"),
          sum(1 for r in results if r[1] == "EXPECTED"), sum(1 for r in results if r[1] == "SURVIVED"), sum(1 for r in results if r[1] == "INVALID")))
    return 0 if k == len(results) else 1


if __name__ == "__main__":
    sys.exit(main())
