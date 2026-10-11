# D08 Canonical Toxic/Flame Orb acquisition

Implementation/host validation complete; exact-head CI pending. Base6199b5c5af511a76c443cfac515a45207d2ff458 (refreshed from3f75a39 after D02 merged). Pinned upstream
8555c08c823b856cbec4eb99ca84ea52a955836d, Node22.23.3. Current main/open
PRs checked: no canonical game Orb caller; existing #154 is inventory-to-battle.
Reward #182 canonical API exists; that core stays read-only.

Evidence collected before production edits:

| Source symbol | Lines / intended C behavior | Fixture |
|---|---|---|
| modifier.ts TurnStatusEffectModifier constructor/matchType |1672–1702: distinct effect identities; shared class matches either Orb | both, opposite refusal, other holder |
| PersistentModifier.add; PokemonHeldItemModifier.match | first exact holder, max1; wildcard does not match | duplicates/first full/wildcard |
| modifier-type.ts factories |2162–2174: real Toxic/Flame constructor args holder only | canonical shadow arg0; retain original -1/0 |
| BattleScene.addModifier |2395–2429: full tier replacement; success mutation returns false | direct/free/paid/full |
| BattleScene.updateModifiers |2748–2780: orphan cleanup, party recalculation | wildcard cleanup/order/full party |
| SelectModifierPhase apply/open |271 onward and public selectFilter: full before add; paid false errors, no charge | actual menu/purchase and wallet |
| reward canonical_orb.c | separate raw API: shared cap1; stable owner mapping | reordered storage/party IDs |

Baseline before edits: H host/normal/save pass;1000 ended each,0 clears/stuck/
crashes/timeouts,50406 turns,paired66edb535,14701 saves. Four seeds×2000 unchanged
orbdiff/wideacquirediff/multiacquirediff all pass (24000 comparisons).

## Bounded caller and results

Only game-core changed: gc_orb_add.c, reward dispatch, internal declarations,
Makefile and new owned tests/tools. Raw acquisition calls the separate #182 API;
legacy opaque Orb and other reward contracts stay untouched. Stable storage
maps through party_order to unique reward IDs. Constructor-only Orb args -1/0
normalize in a validated shadow; original inventory identity/order/args persist.
Orphan wildcards are removed only after actual successful Scene add; both Orb
identities share that cleanup. Public first same/opposite-holder FULL refusal
leaves inventory unchanged; direct Scene FULL queues the full-item event and
executes actual tier-default ball replacement. Successful persistent additions
return false at this pin: free selection finishes, paid selection errors without
charging although stock changed. Other-holder stock does not refuse.

Unrepresented malformed stock/order/ID, virtual/detached, missing-holder,
challenges, fusion, stat-changing recalculation and projection overflow are
explicit atomic unsupported guards. Scene orphan non-Orb held classes and
nonpositive opaque records stay bounded fallback. The existing generic legacy
fallback remains available and counted. No transfer/status-engine changes.

Actual source uses real factories, addModifier/updateModifiers and public
SelectModifierPhase filter/apply/purchase; observer delegates addModifier.
Headless UI party-choice, graphics, audio and phase-end callbacks are fixture
inputs; modified classes and inventory decisions are never stubbed. All54 modeled
party fields, inventory order/holder/args/stacks/virtual flags, wallet, ball deltas,
source finish/error flags, profile, both RNG preservation and projected Orb
holder counts compare. Friendship remains unchanged. Rendered full-message
presence is checked source-side; the game full-item event key/count is compared.
Exact rendered message parity is excluded from this acquisition proof.

Seeds777/9001/4242/31337: requested/completed/covered/identical2000 each;
zero differences/skips/throws, Toxic/Flame1000 each; each identity has500 direct
and500 public cases. Each seed:400 Scene inventory changes,800 direct full ball
fallbacks,800 public filter refusals,660 wildcards,800 duplicates,1332 paid cases,
1000/1000 direct/public paths. Fainted requested targets1003/1017/998/997. Each seed includes20 empty
inventories,140 visits to95/96-record capacity boundaries, and306/336/357/329
one-member parties; all party sizes1–6 occur.
The unchanged #182 raw producer/comparator independently passes2000 each,
37 original guards; full1600/add400/stack0/oppositeFull800, both identities1000.
These are prerequisite raw checks, not another8000 game caller comparisons.

New host checks483 pass, including both identities/same/opposite holders, exact
free/paid behavior, wildcard cleanup, full refusal, legal command-boundary save/
reload, original args/identity/order and17 atomic unsupported contexts.
38 malformed transports all exit2; empty/missing/truncated/out-of-order/count/
footer/duplicate/nonfinite/out-of-range records are refused. The wrapper checks
source SHA/Node22, bounded integers/nonempty unique seeds, creates unique paths
and preserves source/replay error codes.

Real gc_orb_add mutations using seed777 N2000: allowing opposite-Orb additions
fails400 caller comparisons (exit1); mapping reward target to storage index
fails754 (exit1). Intact2000/2000, exit0; temporary copied build directories
removed, final working production never mutated. Original source expectations
remain unchanged.

Development failures retained in d08-full.log: the first additional raw-game
adapter experiment attempted the reward comparator's256-slot guard against the
96-slot game projection and correctly refused. That experiment is not included
or counted as source parity; the unchanged reward comparator runs independently.
The first new finish/error transport guard incorrectly expected paid source
success to end the reward phase; source SelectModifierPhase keeps it open.
The new owning guard was corrected from the source, and final full comparisons
pass. No established test was changed or failure reclassified.

Regression finals: unchanged old Orb bridge2000×4, Wide acquisition2000×4 and
Multi acquisition2000×4 all pass. Multi/Orb reuse this session's baseline source
traces; Wide final replays its retained pinned historical corpus because the
old Wide/Multi wrappers share caller filenames. Baseline Wide source checks also
passed fresh in this session; historical source is labeled honestly.
All established host/save fixtures remain passing. H final:1000 normal/save
ended each,clear0/stuck0/crash0/timeout0,50406 turns,paired176e408e,14701 saves,
max11472 bytes,200-run host pair4a43b96f. Baseline3f75 had66edb535; refreshed
D02 head already has176e408e. This Orb acquisition leaves the refreshed digest
unchanged. This is host consistency, not a legal Classic clear or hardware proof.

Reproduce:

```sh
. /workspace/shared/activate-parity.sh
game-core/tools/orb-acquisition-diff.sh /workspace/shared/upstream-lane-b 2000 '777 9001 4242 31337'
TURNTEST_LOCALES=<upstream>/locales/en python3 game-core/tools/orb-acquisition-malformed.py game-core/build/orb_acquisition_diff_test <caller-trace>
TURNTEST_LOCALES=<upstream>/locales/en python3 game-core/tools/orb-acquisition-mutations.py <caller-trace>
make -j2 -C game-core test
game-core/build/gcplay --auto 1000 --jobs 8
game-core/build/gcplay --auto 1000 --jobs 8 --save-test
```

Evidence retained in `/workspace/work/batch-2026-10-11/` (d08 logs, smoke,
baseline, mutations and final regressions), plus the owning ignored build
folder below. Earlier failed runs are retained separately.

Final evidence directory: `/workspace/work/batch-2026-10-11/lane-c/game-core/build/orb-acquisition-evidence.iPMkap`.

|Trace|SHA256|
|---|---|
|caller-2000-31337.txt|`f9415471d650492ae890bc1f014df147989647b5df55e541a811d33978af43db`|
|caller-2000-4242.txt|`f318577700a0dcd1ea44a9f312579e1ebff30ebcb649fbcb2510171136baa10d`|
|caller-2000-777.txt|`cf058a8946a9f30989c3fc5e29a22926469131e3352052f3b277f95aa30b1265`|
|caller-2000-9001.txt|`3c8d7a1f6d2370ccf0522384f6147fd9aace90bbc745aebf20f016baceffd239`|
|raw-2000-31337.txt|`dc2f1af90f88072b92f09b500eb391c3a755d54eda3cd49b707f9f7a741ad532`|
|raw-2000-4242.txt|`9ea40723af5194eefc37f507057104952e8fd1310f764c59e90cc02563c7a793`|
|raw-2000-777.txt|`a9ebc4d6eb3feb9e8f4c0a74c65e405cfbf742f0ba844c76b9d7c7d8d9e137f1`|
|raw-2000-9001.txt|`2fc215f68674b339b7f8f0ce08690fed3fd8d86135a13a84cb1d74ca961eb4d2`|

Producer SHA256: `d025ef730fb93e72bfc5e191a35353fb0cc6f90678bde75f773135a1cb075a7d`; GCC14.2.0, Node22.23.3.
