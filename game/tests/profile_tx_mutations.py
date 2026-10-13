#!/usr/bin/env python3
"""Mutation check of the profile transaction owner (task C08), the same method as capsule_mutations.py and radar_mutations.py.

Each mutation is one textual change to a file under game/source, applied in place, built into the host test that should notice it, run,
and always restored. KILLED: it builds and the test fails. SURVIVED: it builds and the test still passes. INVALID: the pattern did not
match exactly once, or the build failed (nothing was shown either way). Exit 1 on any SURVIVED or INVALID.

  python3 game/tests/profile_tx_mutations.py            # every mutation
  python3 game/tests/profile_tx_mutations.py 3 7        # only these numbers

The groups: the commit point (a grant before the commit, a commit that is not durable, a result revealed before it), the single pull
(a second pull on a retry, the scene RNG not saved, an entropy seed that is not the saved one), the roll-forward (its order, its
completeness, the gate on sysd_save, the recovery cases), the guards (a request that outlives a change of the profile, a wallet that
is not readable, a busy file, a missing scene seed, a newer version), the reward (amount, kind, cap, claimed once, the custom write)
and the result ring.
"""
import os
import subprocess
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
GAME = os.path.join(ROOT, "game")
TESTS = os.path.join(GAME, "tests")

TX = "source/profile_tx.c"
CS = "source/custom_store.c"
SD = "source/system_data.c"
T = ("profile_tx_test_fast", ["build/profile_tx_test_fast", "60"])

# (file under game/, old text, new text, (make target, command), what the mutation is)
M = [
    # ---- the commit point
    (TX, "    const bool back = put_back(pre, n_pre);\n    free(pre);\n    free(post);\n    if (!back) {\n      S.fatal = true;\n      snprintf(S.why, sizeof S.why, \"The purchase could not be saved",
         "    const bool back = true;\n    free(pre);\n    free(post);\n    if (!back) {\n      S.fatal = true;\n      snprintf(S.why, sizeof S.why, \"The purchase could not be saved", T,
     "a failed commit write leaves the pull in the live profile (grant before commit)"),
    (TX, "  if (!write_core(&N, post)) {", "  if (false) {", T, "the commit write's failure is ignored (a result that is not durable)"),
    (TX, "  memcpy(&N, &P, sizeof N);\n  push_result(&N, res);\n  N.pend = PEND_COMMITTED;",
         "  if (out) *out = *res;\n  memcpy(&N, &P, sizeof N);\n  push_result(&N, res);\n  N.pend = PEND_COMMITTED;", T, "the result is revealed before the commit write"),
    (TX, "  /* committed: the result is durable from here on */\n  memcpy(&P, &N, sizeof P);\n  free(g_profile);\n  g_profile = post;\n  free(pre);\n  if (out) *out = *res;",
         "  /* committed: the result is durable from here on */\n  memcpy(&P, &N, sizeof P);\n  free(g_profile);\n  g_profile = post;\n  free(pre);", T, "the committed result is not handed back"),
    # ---- the single pull
    (TX, "  if (done) {\n    if (out) *out = *done;", "  if (false) {\n    if (out) *out = *done;", T, "a retry does not return the committed result (it would have to pull again)"),
    (TX, "    N.scene = *scene;\n    N.scene_seeded = true;", "    N.scene_seeded = true;", T, "the scene RNG after the pull is not saved"),
    (TX, "  sow_text(&ctx.entropy, P.req.seed, P.req.seed_len);", "  sow_text(&ctx.entropy, (const uint8_t*)\"x\", 1);", T, "the entropy seed is not the one saved at begin"),
    (TX, "  ctx.timestamp_ms = P.req.timestamp_ms;", "  ctx.timestamp_ms = P.req.timestamp_ms + 86400000ull * 3;", T, "the clock is not the one saved at begin"),
    (TX, "  ctx.scene_rng = P.scene;", "  sow_text(&ctx.scene_rng, (const uint8_t*)\"fresh\", 5);", T, "the scene RNG is re-seeded instead of continued"),
    (TX, "  if (!N.scene_seeded) {\n    sow_text(&N.scene", "  if (true) {\n    sow_text(&N.scene", T, "the scene RNG is re-seeded at every begin"),
    (TX, "  if (!P.scene_seeded && !S.have_seed) return PTX_E_NO_SEED;\n", "", T, "a first transaction without a scene seed is accepted"),
    # ---- the roll-forward and the recovery
    (TX, "  if (P.pend_kind == PTX_KIND_CLAIM) {\n    const cs_claim_outcome o", "  if (false) {\n    const cs_claim_outcome o", T, "the reward's claimed flag is never written"),
    (TX, "  if (!sysd_write_bytes(g_profile, P.profile_len)) return false;\n", "", T, "the candidate profile is never written to the card"),
    (TX, "  N.pend = PEND_NONE;\n  N.pend_kind = 0;\n  N.pend_txn = 0;\n  memset(&N.req, 0, sizeof N.req);\n  N.claim_record = 0;\n  N.pre_hash = N.post_hash = 0;\n  N.profile_len = 0;\n  if (!write_core(&N, NULL)) return false;",
         "  N.pend = PEND_COMMITTED;\n  if (!write_core(&N, NULL)) return false;", T, "the transaction is never marked finished"),
    (TX, "  sysd_set_gate(gate);\n", "", T, "nothing stops sysd_save from writing over an unfinished transaction"),
    (TX, "  if (d == P.pre_hash && d != P.post_hash) {", "  if (false) {", T, "the boot does not install the candidate when the card still has the old profile"),
    (TX, "  } else if (d != P.post_hash) {\n    S.state = PTX_UNUSABLE;", "  } else if (false) {\n    S.state = PTX_UNUSABLE;", T, "the boot overwrites a profile that something else changed"),
    (TX, "  ptx_settle();   /* a failure leaves it committed: the gate and the next boot retry */", "", T, "the boot does not finish a committed transaction"),
    # ---- the guards
    (TX, "  if (sysd_digest(pre, n_pre) != P.pre_hash) return finish_refused(", "  if (false) return finish_refused(", T, "a request is run on a profile it was not made for"),
    (TX, "  if (sysd_state(why) == SYSD_UNREADABLE) {\n    snprintf(S.why, sizeof S.why, \"The system data cannot be read, so it",
         "  if (false) {\n    snprintf(S.why, sizeof S.why, \"The system data cannot be read, so it", T, "an unreadable profile is used as the wallet"),
    (TX, "  if (ver != VERSION) return ver > VERSION ? 2 : 1;", "  if (ver > 100) return 2;", T, "a file from a newer version is read"),
    (TX, "  if (!n_pre) {   /* a profile that cannot be serialized cannot be saved: nothing is done */\n    free(pre);",
         "  if (false) {   /* a profile that cannot be serialized cannot be saved: nothing is done */\n    free(pre);", T, "an unserializable profile is run anyway"),
    # ---- the reward
    (TX, "(int)CC_REMATCH_REWARD_VOUCHERS);\n    res.record", "2);\n    res.record", T, "the reward gives two vouchers"),
    (TX, "gc_system_add_voucher_count(sys, GC_VOUCHER_REGULAR, (int)", "gc_system_add_voucher_count(sys, GC_VOUCHER_PLUS, (int)", T, "the reward gives a plus voucher"),
    (TX, "    if (sys->voucher_counts[GC_VOUCHER_REGULAR] > UINT32_MAX - CC_REMATCH_REWARD_VOUCHERS)", "    if (false)", T, "the voucher count can wrap"),
    (TX, "    if (cs_reward_state(P.claim_record) != CS_CLAIM_OWED) return finish_refused(", "    if (false) return finish_refused(", T, "a reward claimed meanwhile is granted again"),
    (CS, "  if (e.state == CC_RW_OWED) return CS_CLAIM_OWED;", "  if (e.state == CC_RW_OWED || e.state == CC_RW_CLAIMED) return CS_CLAIM_OWED;", T, "a claimed record is still owed"),
    (CS, "  if (!write_stage()) return CS_CLAIM_WRITE_FAILED;", "  write_stage();", T, "the claim's write failure is ignored"),
    # ---- the refusal and the ring
    (TX, "  push_result(&N, &x);\n  clear_pending(&N);", "  clear_pending(&N);", T, "a refusal is not kept as a result"),
    (TX, "  if (!write_core(&N, NULL)) return PTX_X_IO;\n  memcpy(&P, &N, sizeof P);\n  if (out) *out = x;", "  write_core(&N, NULL);\n  memcpy(&P, &N, sizeof P);\n  if (out) *out = x;", T, "a refusal's write failure is ignored"),
    (TX, "  if (c->n_results == PTX_RESULTS) {", "  if (c->n_results == PTX_RESULTS + 5) {", T, "the result ring overflows"),
    (TX, "  const ptx_result* x = find_result(txn);\n  if (!x) return false;\n  if (out) *out = *x;\n  return true;", "  const ptx_result* x = find_result(txn);\n  if (!x) return false;\n  return true;", T, "ptx_result_of returns no result"),
    # ---- begin
    (TX, "  if (!write_core(&N, NULL)) return PTX_E_IO;\n  memcpy(&P, &N, sizeof P);\n  if (txn) *txn = id;\n  return PTX_OK;\n}\n\nptx_status ptx_begin_claim",
         "  memcpy(&P, &N, sizeof P);\n  if (txn) *txn = id;\n  return PTX_OK;\n}\n\nptx_status ptx_begin_claim", T, "the request is not saved at begin"),
    (TX, "  N.pre_hash = d;\n  memset(&N.req, 0, sizeof N.req);\n  N.req = *req;", "  N.pre_hash = 0;\n  memset(&N.req, 0, sizeof N.req);\n  N.req = *req;", T, "the begin-time profile digest is not recorded"),
    (TX, "  if (sysd_state(why) == SYSD_UNREADABLE) {\n    snprintf(S.why, sizeof S.why, \"A purchase is waiting", "  if (false) {\n    snprintf(S.why, sizeof S.why, \"A purchase is waiting", T, "the boot finishes a purchase onto an unreadable profile"),
    # ---- the system data hooks
    (SD, "  if (S.gate && !S.gate()) return false;", "  if (S.gate && !S.gate()) return true;", T, "a refused gate is reported as a successful save"),
]

# Changes that cannot change what the code does, and why (listed, not run)
EQUIVALENT = [
    ("ptx_begin_pull's own `if (P.pend != PEND_NONE) return PTX_E_BUSY;` removed",
     "begin_common checks the same thing before anything is written; the first check only avoids serializing the profile for a request that will be refused."),
    ("sysd_write_bytes' refusal to write over an unreadable profile file removed",
     "no caller reaches it: ptx_begin_* refuse an unreadable profile, the boot refuses to settle on one (the unreadable-at-boot test kills a mutant of ptx_recover's own check), and sysd_save returns before the gate for one. It stays as a last guard on the only function that writes bytes it did not serialize itself."),
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
