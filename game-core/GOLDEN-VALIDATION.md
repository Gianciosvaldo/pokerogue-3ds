# Golden Punch inventory and wallet bridge — 2026-10-08

Pinned upstream `8555c08c823b856cbec4eb99ca84ea52a955836d`, Node 22.
This additive game-core slice calls the battle-core implementation from #161.
It transports the run's **double** wallet into initial/rebuilt battles and every
turn, switch resume, CheckSwitch answer and Victory continuation; returns the
wallet after each engine call. Pay Day's separate integer `money_scattered` is
untouched. No new money UI/event transport is claimed.

Loaded Golden Punch real/transient stacks have effective count 1–5 and arg -1/0.
Holder IDs, wildcard -1 and detached owner 0 retain their existing distinct
representation. Both modifier lists survive repeated synchronization, rebuild,
and defeated enemy detachment. Ordered Amulet Coin entries have effective count
1–5, global owner 0 and the same audited args. Transient Coin projection is
specifically enabled by a shape-valid Golden in the represented 96-entry prefix
of either list; standalone transient Coins remain opaque. An overflow Golden
cannot enable it. This preserves the unchanged established Lens host check.
Malformed known dependencies stay typed/opaque so battle-core's monetary
preflight refuses atomically rather than silently dropping a multiplier.
Invalid/NaN/over-cap wallets similarly refuse the monetary effect. All arithmetic
uses the battle API's double wallet and MAX_SAFE_INTEGER cap, not int32 events.

## Real-source evidence

`tools/goldendiff.sh /workspace/shared/upstream-lane-c 2000 SEED`, for 777, 9001,
4242 and 31337. New `golden_diff_test.c` includes the owning Golden replay and
guards read-only; the established producer, comparator, seed/count/exclusion
rules are unchanged. All **8,000** full turns and all rendered message sets match,
with zero parse/stat mismatches, failures, uncovered or skipped cases. Both sides'
Golden/Coin entries, virtual stacks, ordered bonuses, fractional wallets, cap
credit, misses/non-damage/no-holder effects, double hits and defeated ownership
are compared alongside every existing recorded field, phase and RNG state.
Each seed maps 1,800 player/200 enemy Goldens and 800 player/200 enemy Coins;
wallet-gain turns are 1,362 / 1,357 / 1,358 / 1,372. These are gain turns, not
mapping counts or numbers of individual apply calls.

Removing `gc_wallet_from_battle`'s actual wallet assignment in an external C
source mutant fails at turn 0: original wallet 23120.125 versus source/core
wallet 23140.125 (exit 1). Removing the completion footer fails closed (exit 2).
The normal oracle still requires all requested cases and identical rendered text.

`golden-storage.producer.test.ts` makes ten actual pinned `ModifierData`
serialize/reconstruct calls, valid player/enemy holders and real stacks 1–5.
Virtual stacks are omitted/reset; direct source class weights are 40/60/80/100/100
before and 20/40/60/80/100 after, at damage40 with no Coins. The C host reads those
answers, checks saved/rebuilt real and virtual counts, and calls the actual C
monetary dispatcher before/after: the player receives those amounts, while the
enemy correctly receives no credit. Direct source class apply in that storage
producer is constructor/weight evidence, not enemy turn-dispatch parity. The
8k full-turn oracle proves the actual enemy dispatch boundary separately.

To reproduce storage evidence, copy the additive producer to upstream
`test/tests/zz-dump/`, run Node22 Vitest `run --maxWorkers=1` on that file with
`GC_GOLDEN_STORAGE_OUT=/tmp/golden-storage.txt`, then run
`game-core/build/golden_host_test --storage /tmp/golden-storage.txt`.
Normal Golden host: 330 checks; with ten storage answers: 454 checks.
Actual command knockout/Victory continuation, real CheckSwitch prompt/answer,
caller wallet changes, subsequent battle rebuild, legal save/load, cap nominal67
versus actualcredit1, unchanged profile, owner/refusal/overflow and repeated
synchronization are tested.

## Host regression and limits

`make -j2 -C game-core test` passes. Existing Mint/vitamin/Money/Orb/Seed/Lens
host counts remain 107/187/110/235/126/195. Older Mint/Seed/Lens save fixtures
still load with their original 113/130/199 checks. All four existing apply,
candy, ball and reward-option replay outputs remain byte-identical; all four
Mint/vitamin/Money/Orb/Seed/Lens real-source traces still pass at established
counts. No established source harness was weakened or edited.

Before and after, `gcplay --auto 1000 --jobs 8`, with and without `--save-test`:
1,000 ended, zero stuck/crashed/timed out, 49,206 turns; 14,291 save/load round
trips, largest 11,400 bytes. On original base432ee53, paired digest changes from
`c2937ca0` to `876746da` and held-item unsupported reports fall 52,161→52,115.
After integration of main50f279b/#169, the clean baseline is `5022ea67` and
Golden's final paired digest is `15d6d01d`, reports 52,064→52,018. Outcome,
terminal, reward-choice and stand-in rows remain unchanged. The digest hashes
outcomes/stubs/unported counters, not money: its change does not itself prove
wallet credits. Final 200-run paired test is `2f376abe` (original base Golden
pair `522c1e4f`). External actual-dispatch observation records seven player
credit calls and seven wallet increases totaling33, distinct from eight Golden
and eleven Coin rewards taken. Existing fallback reward tally remains139runs/
169occurrences: canonical acquisition/stacking/replacement is unclaimed.

MoneyMultiplier/DamageMoneyReward and Scene.addMoney audit shows this Golden
source path has no GimmighoulTracker dispatch (unlike consumable MoneyReward).
We do not invent that side effect. The represented wallet/turn path excludes
source profile/achievement validation, UI refresh/sounds, arbitrary external
scene/source-party extensions, generic acquisition/loot/restacking policy,
whole source ModifierData/session migration, full BattleEnd/Pay Day, full
Task96/104, whole-run upstream parity and hardware acceptance. Virtual-fixture
normal/save equivalence is intentionally not claimed; zero-virtual stock runs
are the paired soak domain. Existing counted fallback remains for unsupported
loaded records and acquisition behavior.

Durable evidence: `/workspace/validation/lane-c/ten/golden/`.
