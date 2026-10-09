# Trainer-side switches: bounded Card 103 proof

Target: upstream `8555c08c823b856cbec4eb99ca84ea52a955836d`, Node 22.23.3.
The October 4 uploaded roadmap describes trainer routing as absent. Current
source already implements same-trainer reserve conditions, force replacement,
next-summon callbacks and trainer text; this task proves those existing paths
and repairs a demonstrated pinned condition discrepancy rather than duplicate
selection logic. Existing README statements claiming all trainer switches were
unported are corrected with this dated scope.

## Runtime repair

Pinned ForceSwitchOutAttr.getSwitchOutCondition checks the live **enemy party**
when a trainer exists, even when an enemy Roar targets the player. C previously
counted the targeted side, incorrectly switching a player with reserves when
only one enemy remained. `src/switching.c` now follows the pinned guard exactly.
The unchanged dedicated traces each have 25 failing turns/texts before and zero
after, with 2000 covered on both sides. No new unsupported flags mask a failure.

Other routing is unchanged: a trainer self-switch needs a legal off-field
reserve with the same trainerSlot; nonpartner force replacement considers the
eligible enemy party, and ordinary self-switches request the caller's selection.
The source additionally checks isAllowedInBattle. The proof uses ordinary
allowed fixture inputs; broader challenge legality policy is not represented
or claimed. Mystery encounters and natural partner doubles remain outside it.

## Actual source proof

`tests/trainer-switch.producer.test.ts` constructs a real Youngster trainer,
uses real MoveEffectPhase and original Trainer.getNextSummonIndex (including
actual matchup scores and seeded tie handling), and records callback arguments
and answers. Wrappers observe and delegate original switch predicates. They do
not replace move effects, trainer selection or scores with a fixed answer.
Every actual callback also asserts that global and battle RNG are unchanged.
C echoes those recorded answers through its existing caller-owned callback;
this establishes invocation/continuation parity, not a new AI selection port.

`tools/trainerswitchdiff.sh <upstream> 2000 <seed>` checks the exact pin and
Node 22. Seeds 777/9001/4242/31337 each cover 2000 requested and completed turns,
zero parse/stat/state/RNG/text differences, zero unported turns. Each seed has
500 turns for each of U-turn, Baton Pass, Parting Shot and Roar, both acting
sides and 200 cases in each of ten states. Source switch-summon observations per
move are 425/425/375/300 and reserve/immunity refusals 75/75/75/200. Callback
counts are 575/577/575/576. Cases include no live enemy reserve, wrong loaded
trainer slot, no player reserve, real source tied candidate scores, loaded
TRAINER_PARTNER slots, Baton Pass stages, Parting Shot's stat floor, Suction
Cups and Guard Dog. Loaded slot cases do not claim natural partner team creation.

The additive replay copies the established full-state/RNG/text comparison with
only fresh actual getEffectiveStat records added. Callback order, argument and
answer checks are the existing EPICK contract. The strict wrapper requires all
requested turns and texts, validates exact request/footer/case sequencing and
EPICK shapes, and preserves the explicit missing-callback trainer flag (one
actual switch caller check). Twelve malformed traces exit 2: missing/wrong/
fractional footer or header, wrong count, trailing case, missing/duplicate pick,
fractional pick, invalid owner, out-of-range pick and unknown modifier. A
fractional producer request exits 1 rather than rounding its requested count.

Actual C mutations fail the owning driver: wrong callback owner records 575
callback failures (the unchanged comparator then removes them from its covered
set); ignoring reserve ownership records 100 failures, including 75 callback
coverage refusals and 25 state/text differences; suppressing Baton Pass transfer
records 50 full-turn failures at unchanged 2000 coverage. Reverting the actual
last-enemy guard reproduces 25 failures per seed. These failures remain visible,
and the wrapper exits 1 on incomplete coverage rather than accepting a subset.

## Retained checks and historical transport limitation

All 68 existing battle before/after replay outputs are byte-identical, including
four legacy damage/stat outputs with 2000/2000/2000/1999 covered and zero stat
mismatches. All 56 retained game source replay outputs are byte-identical: prior
48 plus eight raw/acquisition Multi Lens suites. No established producer,
comparator, guard, count, tolerance or exclusion was changed.

The unchanged general TURN_TRAINER producer was additionally rerun at 2000 per
seed. Seeds 777/9001 retain 1936/1935 coverage while previous 2/1 Roar failures
become zero. Seed 31337 retains 1934 coverage while its previous one Roar
failure also becomes zero. Seed 4242 retains 1930 coverage and the same pre-existing single
failure in CASE 1051: a pending non-Pokemon SwitchSummonPhase has no source
battlerIndex; the original serializer uses integer -1 while upstream's join
renders an empty field (`SwitchSummonPhase:-1` versus `SwitchSummonPhase:`).
State, trace, RNG and texts otherwise match. No truthful integer-only runtime
value can repair that representation mismatch. The old comparator and its
failure remain unchanged and are not credited as a pass; correcting its
undefined-metadata transport needs separately scoped stronger proof. Card 103
remains bounded rather than whole trainer-turn parity.

`make -C game-core test` passes. Paired 1000 normal/save runs, jobs8, are
byte-identical before/after: 1000 ended, zero stuck/crashed/timed out, 50394 turns,
digest `5b6c9ee2`,14679 round trips, largest save11472 bytes. All stand-ins and
phase/move/ability counts remain exact. The smaller normal/save host check also
retains digest `bc0fbf1c` equality. Source comparisons, host/3DS CI and hardware
execution are separate; no hardware or whole-run source parity is claimed.

Evidence: `/workspace/validation/lane-a/eight/trainer`.
