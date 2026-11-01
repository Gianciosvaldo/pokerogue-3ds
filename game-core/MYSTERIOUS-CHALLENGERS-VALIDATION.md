# Mysterious Challengers battle and reward repair

This is a source-backed follow-up to the already merged encounter, not a second
initial port. The merged implementation filters paired trainer classes, resolves
templates using the previous wave, discards a hard trainer's template function,
omits native constructor name draws, and can substitute a normal class when a
boss party cannot be built. The retained native observations expose those gaps.

The encounter now selects the source-seeded normal, hard and boss trainers,
starts the selected battle (including double-only classes), emits the trainer
scene event, and uses the encounter-specific victory rewards. The app draws
three black silhouettes from those selected trainer keys. Preview queries use a
local RNG owner so opening/redrawing the question cannot consume battle RNG.

The shared Pillar Men owner still accepts its custom-team hook, filters paired
classes and retains its existing fallback. Ordinary Challengers do not replace
unsupported trainer parties with a different class: generation failures retain
the diagnostic and leave through the existing heal shop. Only this encounter
opts into the already-supported Gym/E4 factories plus the newly ported Twins and
Mysterious Sisters random-pool factories. Other encounter guards remain in place.
The existing private `pillar` state owns both encounters' victory path;
its merged mc flag is retained. The session save format does not change.

## Pinned source and observation boundaries

Source: `8555c08c823b856cbec4eb99ca84ea52a955836d`.
Locales: `270ed0a2938b2c3e2e34e7de940e84aecadf548b`.
The source's assets gitlink is `87426a79611f9d212c4dc8af58e2834a05b93725`;
these tests observe sprite keys and renderer calls, not hardware asset decoding.

`tests/challengers.producer.test.ts` executes the actual encounter onInit and
selected-option callbacks. It observes `initBattleWithEnemyConfig` at an explicit
boundary and constructs the actual native Trainer there, including native name
keys, template selection/member getters and levels. Rewards are observed at
`setEncounterRewards` configuration. Both producer and paired-factory producer
capture native `randBattleSeedInt` before constructing GameManager and restore it
after starting the battle.

The 320 cases cover ten waves, four biomes, all three options, named/female/double
trainers and template boundaries at waves 21/31/61/91. All original 192 cases are
retained unchanged. The first version of this producer assigned `arena.biomeId`
and left its cached trainer pool stale; that invalid capture is not authoritative.
The retained producer calls the real `newArena` API. Native name keys are kept
verbatim; the additional numeric name index is the native locale object's key
position, not an inferred numeric suffix.

`run-core/tests/pair-trainers.producer.test.ts` invokes the actual configured
Twins/Mysterious Sisters party-member factories, observes species/level/slot and
Phaser RNG at entry to `addEnemyPokemon`, then runs the real constructor. Its 640
cases retain 320 normal-evolution cases and add 320 strong-evolution cases,
including Classic wave-20 controls. C comparisons wrap the real base constructor
and check its inputs and serialized RNG fields before invoking it. Phaser state
contains c/s0/s1/s2; C's seed-mash scratch `n` is not part of that observation.

## Validation

* All 320 configuration/constructor-level C cases pass 15,607 checks. The wrapper
  verifies template size, aggregate/per-member strengths, evolution kinds,
  same-species/balanced flags and actual generation levels. Trainer selections,
  variants, names, battle entry and victory reward configuration also match.
* All 640 pair-factory boundary cases plus six invalid/unknown-input guards pass
  2,567 checks. The paired factory owner passes scoped ASan/UBSan/LeakSanitizer.
* Fresh native captures match the frozen observations and C projections exactly.
  Both frozen captures and projection checkers run in normal tests; twenty
  malformed/forged capture variants are rejected.
* Thirteen compiled owner faults are rejected after four pristine controls pass:
  wrong selection/option streams, double variant, name position, previous-wave
  template selection, easy/hard rewards, paired slot/evolution/pool, and missing
  caller opt-in, post-fusion field mapping and send-out snapshots. The previous-wave fault initially survived the 192 samples;
  adding actual boundary cases made that regression observable.
* Six played C battles, including a double battle, execute 25 fight commands and
  pass 104 checks through actual victory, configured reward offers and next-wave
  transition. A strong healthy fixture party isolates encounter plumbing from
  difficulty. This test does not call the victory owner directly. Scoped shared
  owner/entry/trainer-party ASan/UBSan/LeakSanitizer passes.
* Actual me_flow -> me_screen drawing passes all 2,703 checks with the 320 native
  silhouette records and retained UI cases, under ASan/UBSan/LeakSanitizer.
* Retained Pillar Men behaviour passes. Other retained trainer/app/custom/save
  checks are recorded in `work/challengers/` as they complete.

The legacy merged encounter cases are retained. Their assertions now check the
source-selected double variant and exact trainer identity (without the old boss
substitution allowance); hard sizes retain the source template function where
present. The 320-case native comparison independently checks those templates.
No coverage floors were reduced.

The native battle-initialization boundary is intentional. This proof does not
compare full generated parties, movesets, stats or post-constructor/finish RNG.
`gc_port_enemy_moveset` still has broader source-coverage work; the guards above
are not evidence that every trainer family is complete. Safari Zone, Teleporting
Hijinks and integrated wide Endless/source review remain separate work. No
Windows builder is included.

## App regressions uncovered by the changed deterministic sequence

Main's separately merged same-species Illusion fix is retained; it is not
reimplemented here. The app transfer oracle now follows the source's
`TurnStatusEffectModifier.matchType`, which intentionally ignores orb effect.
An opposite orb consumes the one-item capacity. Literal copy conservation and
full-stack refusal assertions remain, with no threshold changes.

The custom run also exposes a Shedinja defeated by entry hazards before its
queued send-out is drawn. GC_EV_SUMMON now carries its initial HP/maximum/status
as runtime-only data, captured by normal setup, manual switch and load owners.
The screen uses that state before replaying later hazard damage/faint events.
Other appearance fields remain sourced as before; this is not a complete new
out-of-turn presentation snapshot format.

Cleanup also refreshes player field pointers from ordered battle party entries
before ReturnPhase. This matches pinned getPlayerField/getField after a fusion
splice. The immediate post-fusion contract is retained. The focused integration fails without this refresh. It now passes 24 checks
including the surviving ally's real ID/species in the withdrawal snapshot and
Shedinja's 1-HP send-out followed by actual lethal Spikes damage/faint events.
The focused owners pass scoped ASan/UBSan/LeakSanitizer.
The reconciled 200-run custom corpus then passes with zero observer losses or
model violations; the observer validator was not changed. Diagnostic code used
to locate the invalid event remains outside committed source.

Final production-harness checks after reconciliation: 10,000 normal runs and
10,000 runs with external event storage, 200 strong custom runs, 100 custom
save/resume runs and 100 custom Save and Quit runs all pass with zero model
violations. The 78 retained arena cleanup/save checks and merged encounter tests
pass. The 320 native cases and thirteen fault controls pass after the runtime
snapshot change as well. Temporary debug observers are not part of this change.
