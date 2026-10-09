# Bounded Classic terminal progression (Card 79)

Pinned source `8555c08c823b856cbec4eb99ca84ea52a955836d`, Node 22.23.3.
This slice invokes the actual source `GameOverPhase.start`/`handleGameOver`,
`offlineNewClear`, achievement/voucher validation, ribbon counting/lineage awards,
and queued unlock/voucher operations. The fixture supplies a legal Classic
terminal team and outcome; it does not play a whole Classic run to completion.
Final-boss/trainer/moveset and run-owner gates remain separate.

## Bookkeeping and local persistence

The new game caller increments sessions won once per team. Each base party member
increments its root Classic count and, when distinct, its starter-selection root
count. Duplicate roots increment each time but only a previously zero root adds
one ribbon-owned count and PLUS voucher. Ribbon payloads independently mark the
base species and its pre-evolution lineage. Fusion species do not gain counts or
ribbons; a fused team unlocks spliced endless. Endless and Mini Black Hole unlock
on a clear; a base species with evolutions also unlocks Eviolite and the unevolved
Classic achievement. Ribbon milestones are 10/25/50/75/100.

The first Classic achievement requires zero prior sessions won and no existing
achievement flag. Its GOLDEN voucher has a separate voucher-unlock flag. A clear
that does not newly earn that achievement queues PREMIUM instead. The replay
includes independently preexisting achievement/voucher flags and counts to check
these distinctions. Ordinary defeat and non-Classic outcomes give no Classic
rewards. Unsupported challenged clears and unrepresentable counters refuse
atomically and remain counted at the existing caller fallback.

`gc_system_add_classic_win` retains its established per-call storage contract.
The canonical terminal caller uses new fields: `ribbons_owned`, RibbonData's
`ribbon_flags`, a separate `classic_voucher_unlocked`, and local achievement slots
192–198. These are this port's named representation, not upstream serialization
IDs. Classic root counts remain uint16; session/ribbon/voucher counts are uint32.
Overflow is refused before commit rather than wrapped.

Source phase reprocessing can award again. The port deliberately adds a local
`terminal_result` marker to prevent duplicate processing, including after reload.
A distinct run still awards normally. The optional named terminal section stores
only processed Classic defeat/clear results; old encounter saves omit it and load
with zero. Terminal load restores the terminal screen without executing awards.
Admission and loading enforce mode/wave/outcome/challenge consistency. The new
section requires exactly one valid record and rejects malformed/trailing data.
System and run saves are separate local files; this does not add an atomic paired
storage transaction or claim source browser history/session-storage parity.

System reload now initializes named dex/starter records and decodes their full
species-bounded block. The previous 256-record limit lost mature-profile entries.
A 400-entry fixture checks preservation, including ribbons; removing all three
new named fields checks zero defaults and preservation of older fields. Original
system/dex/save host tests remain unchanged.

## Source, guards and retained evidence

Seeds `777 9001 4242 31337`, 2,000 cases each: **8,000 exact actual phase cases**,
6,184 clears, 1,816 defeats, 2,668 fused teams, 213 new GOLDEN vouchers, 3,004
PLUS vouchers, 5,764 PREMIUM vouchers and 3,358 cases with distinct root mappings.
Each case compares whole modeled profile, all 54 transported party fields,
wallet and RNG, then checks system/terminal run reload and duplicate processing.
Source-side full PokemonData and opaque profile snapshots prove other source
fields unchanged; queued history/post-game/end completion are observed. Outcome,
party rotation/duplicates/evolved roots, fusions, milestones, prior counts and
achievement/voucher/unlock combinations vary independently.

Presentation/audio/time delivery and external history/session storage are bounded
fixture plumbing. Rendered messages, timestamps, server/account storage, retry
selection, general challenge/non-Classic lifecycle and gameplay-driven full
clears are excluded. Actual source storage delegation is not a copied expected
bookkeeping formula. The local duplicate guard is tested separately from source
behavior.

The additive host fixture passes 136 checks, or 148 with all three original
encounter-save files supplied. It covers repeated/opposite results, distinct
sessions, defeat and >200 failsafe, overflow/malformed-party atomic refusal,
seven invalid save states, eight corrupt terminal sections, dense profile reload,
and missing new fields. True old Mint/Seed/Lens saves also retain their unchanged
113/130/199-check owning host tests.

Six actual caller mutants are detected: per-member session increments, leaf
instead of root counts, fusion instead of base credit, wrong repeat-voucher branch,
disconnected profile commit, and removed local duplicate guard. Eight malformed
transport/footer variants and four invalid count/seed requests are refused.

All **56 existing bridge/temp/Wide/Multi source replays** pass. All **16 existing
apply/candy/ball/options outputs** are byte-identical to the prior validated
record. Their producers, comparators, tolerances, case counts and old host tests
are unchanged. `make -j2 -C game-core test` passes, including original system/dex,
save and approved temporary-booster full-turn checks.

Before and after: both 1,000-run checks have **digest `5b6c9ee2`, 50,394 turns**;
saved mode has **14,679 round trips, largest 11,472 bytes**. Every output row,
including stub counts, is byte-identical. The host paired 200-run digest remains
`bc0fbf1c`. These general soaks have no Classic clears; the terminal cases above
provide the actual clear evidence.

## Reproduction

```sh
. /workspace/shared/activate-parity.sh
make -j2 -C game-core all
make -j2 -C game-core test
for seed in 777 9001 4242 31337; do
  game-core/tools/classicwindiff.sh /workspace/shared/upstream-lane-c 2000 "$seed"
done
GC_LOCALES=/workspace/shared/upstream-lane-c/locales/en game-core/build/gcplay --auto 1000 --jobs 8
GC_LOCALES=/workspace/shared/upstream-lane-c/locales/en game-core/build/gcplay --auto 1000 --jobs 8 --save-test
```

Saved local evidence: `/workspace/validation/lane-c/eight/classic-win/` contains
four source traces/logs and final replay logs, retained mapping/commands and all
outputs, before/after soaks, host/legacy checks, mutations and malformed requests.
The evidence path is environment-local; owning fixtures and wrapper are committed.
