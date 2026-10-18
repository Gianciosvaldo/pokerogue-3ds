# Battle-core continuation audit

Current continuation status (2026-10-18): this file retains historical notes
below. Its old "remaining" rows are not a current unsupported-feature count.
Check implementation, callers, merged history and dedicated validation records
before selecting new work. Helping Hand through ability-changing moves are
merged in #285–#306, including Acupressure (#286), attention/redirection (#292),
Commander/Order Up (#293), wide EXP/combat (#299–#300), stat rearrangement (#301),
Quash (#302), highest-stat selection (#303), Spectral Thief (#304), Pain Split
(#305) and ability changing/copying/giving/swapping (#306).

The active slice is HP-cost stat boosts with source-proven maximum-HP getter
corrections. Next candidates include Reflect Type/Conversion/Conversion 2,
Secret Power, Revival Blessing and delayed attacks. A missing local class switch
case is only a candidate: called-move and override dispatch may handle it
elsewhere, and source-disabled registrations must be distinguished. The final
full source inventory, natural wide progression, battle-boundary flags and
inventory lifecycle audit remain open. No completion percentage is inferred
from sampled coverage.

Starting point: main `efd64988c4a1ab3755f999b29c8eac0c48ad8d49`.
Target remains source `8555c08c823b856cbec4eb99ca84ea52a955836d`, Node 22.
This is a work inventory, not a completion percentage or a passing test record.

Card 103 trainer routing is already merged in #185. Ordinary reserve ownership,
callbacks, switching moves and the pinned Roar guard should not be reimplemented.
Its historical optional-index regression was subsequently repaired in #218.
The leave-field primal weather dispatcher is merged in #230:
Primordial Sea/Desolate Land/Delta Stream weather cleanup. Neutralizing Gas's
source-leave tag lifecycle is a separate slice, not covered by that repair.

Completed continuation slices and remaining source-backed work:

| Slice | Current concrete gap | Required proof |
| --- | --- | --- |
| Wonder Skin (merged #232) | Real accuracy dispatcher and leaf now implemented | Status versus damage, accuracy below/at/above 50, bypass/suppression, stage/item ordering, whole turns/RNG/text |
| Priority brackets and Quick Draw (merged #235) | Real bracket, chance and refusal callbacks now implemented | Real source conditions and seeded order, refusals, passives, priority ties; preserve Quick Claw |
| Protean/Libero (merged #237) | Real normal/failure callers, leaf and message now implemented | Source type-change timing, existing type and Tera exclusions, repeated type changes (the pin has no Gen 9 once-per-summon gate), switch reset |
| Field priority immunity (merged #240) | Defending-field caller now dispatches and honors cancellation | Source positive/zero/negative and modified priority, passives, bypass, source phase/text/RNG; broader doubles remain |
| Receiver/Power of Alchemy (merged #263) | Actual victim context and copy/gain/reset callback now implemented | Ally versus enemy victim, eligibility, copied primary/passive and source order, loaded doubles |
| Parental Bond / Skill Link (merged #244) | Real hit-count callbacks now implemented; existing final-strike reduction retained | Move exclusions, hit powers/order, targets, KO interruption, exact RNG consumption |
| Beat Up (merged #246) | Party hit-count case now implemented; existing per-hit power verified | Pinned party/status ordering, self contribution, base species Attack, KO interruption and modifiers |
| Cud Chew / Harvest (merged #262) | Source histories, repeat/restore callbacks, preservation, inventory and saves now implemented | Actual named held-item history and delays, ownership, duplication/refusal and RNG |
| Ditto / Transform / Imposter (merged #261) | Shared move/ability phase, temporary identity/moves/stats, effective sprite and save/reset now implemented | 8,000 actual phase cases and 8,000 full turns; following-turn PP, switch-in visual regression and active saves; broader doubles remain |
| Sketch / other ability copying | Remaining move effects and identity overrides | Real source identity/stat/moveset copying, bounds, reset and state serialization |
| Forms / Battle Bond | Remaining form callbacks and one-use battle flags | Actual conditions and new required state; do not invent absent lifecycle inputs |
| Redirect / Commander / broader doubles | Singles proofs do not close all partner behavior | Natural field/partner state, target redirect and phase ordering with exact source fixtures |
| Final-boss and interlude boundaries | Explicit PR_FEAT_FINAL_BOSS / interlude caller flags | Actual source phase transitions; coordinate run/game prerequisites with their owners |

Other missing moves and conditions must be prioritized using current replay
failure/unsupported reasons, not just class names. At the starting snapshot a static comparison found
40 instantiated ability classes without an apply_one switch case, but several
are handled elsewhere or are AI/encounter metadata; **40 is not an unsupported
ability count**. Starting retained rich seed 777 had 1,871 exact covered turns
and 129 explicitly unsupported turns before this continuation. Its leading
reasons include priority brackets, field priority immunity, ability bypass,
Pokemon type change, form changes and berry restoration. Fresh before/after
replays are required for each implementation.

Each slice keeps its own branch/PR, actual unchanged source methods, strict
complete-case guards and mutation checks. Source expectations, retained seeds,
counts and tolerances remain unchanged. Host save/load consistency and 3DS CI
are required separately. The later user steering requires normal app/custom/save tests and skips only
the dedicated custom-only Task63 workflow. No full battle-core, run or hardware parity is
claimed until the remaining source paths and their prerequisites are proven.

Priority continuation on base main `fa1cb29`: rich seed777 now has 1,896 exact
covered turns, up from 1,872; summon 1,869, up from 1,868. Other retained coverage
is unchanged. The dedicated source matrix matches 8,000 complete turns and
16,000 direct bracket probes. See PRIORITY-BRACKETS-VALIDATION.md for the
boundaries; these counts do not establish a completion percentage.

Protean/Libero continuation on base main `1930826`: rich seed777 now has 1,906
exact covered turns, up from 1,896. Dedicated source proof matches 8,000 full
turns and 80 normal/simulated dispatch boundaries. The next simple caller gap is
FieldPriorityMoveImmunityAbAttr: the leaf exists, while MovePhase's defending-
field caller still records unsupported instead of honoring cancellation.

Field-priority continuation on base main `9761a62`: rich seed777 now has 1,924
exact covered turns, up from 1,906, with no retained covered mismatch or loss.
The dedicated matrix compares 8,000 complete ordinary singles turns. Natural
partner-field coverage still requires broader doubles proof; the global doubles
flags and source disabled Illusion registrations are not silently bypassed.

Hit-count ability continuation on base main `699f9e8`: rich seed777 now covers
1,929/2,000 turns exactly, up from 1,924. Parental Bond/Skill Link source proof
matches 8,000 full turns and 440 dispatcher boundaries. The remaining 71 rich
turns are a prioritization sample, not the whole remaining battle-core scope.
Leading sampled gaps include forms, berry restoration, Battle Bond predicates,
stat-changing moves, ability copying, Beat Up, suppression and Sketch.

Beat Up continuation on base main `7d555af`: rich seed777 now covers 1,932/2,000
turns exactly, up from 1,929. Source proof matches 8,000 complete turns, 72 party
count checks and 324 power probes, with no retained covered mismatch or loss.
These sampled coverage counts do not establish whole-core completion.

User-specified completion gates (2026-10-12):

- Boss score (merged #250): capture and faint callers in phases.c (original
  reported lines 434/2674) now share the actual source calculation, including
  item multipliers and sqrt(bossSegments) before ceil. Four thousand actual
  source API cases match both C callers, within represented levels 1–255.
  Separate form/item lifecycle flags remain; see BOSS-SCORE-VALIDATION.md.
- Wide levels: the initial score matrix exposed uint8_t level truncation above
  255. Keep the failing 1–1,000 source matrix; coordinate a wider level field
  with game-core's serialized field before whole-core/Endless completion.
  The bounded score harness explicitly refuses oversized inputs instead of
  masking this prerequisite. No save representation is changed by scoring.
- Flee phase (merged #253): 10,000 actual-source phase cases now match RNG,
  meaningful ties, natural doubles, chance, field/stat/item state, queues and
  rendered text with no skips/unsupported cases. Biome selection, inactive
  enemy-slot semantics, status/stack cleanup and enemy stat refresh are fixed.
  Unchanged #243 still matches 587 singles and skips thirteen source ties;
  the new phase proof covers ties directly. Full doubles turns, arbitrary
  challenge rules and broader weather/Neutralizing Gas leave callbacks remain
  separate; see FLEE-PHASE-VALIDATION.md.
- Stat ordering: unfused Spliced Endless halving now works after flee's held-item
  cleanup. Source base-stat total/flat effects precede halving/fusion, while the
  combined modifier helper does not yet represent that order; keep explicit
  unsupported reporting until a dedicated source-backed slice closes it.

Latest user steering: continue until battle-core is fully covered. Flee is
merged in #253. Ditto/Transform is merged in #261; see TRANSFORM-VALIDATION.md. Its owning
phase proof does not close the remaining doubles, forms, berries, wide-level
and final-boss/run prerequisites listed above.

Harvest/Cud Chew continuation on main `756c40d`: the owning 8,000 full turns
match the source. See BERRY-LIFECYCLE-VALIDATION.md for the histories, source
quirks, bounds and separate merge-validation gates. Receiver/Power of Alchemy's
knockout victim context is the next independent ability slice.

Receiver/Power of Alchemy continuation: actual natural-double faint/API cases
now compare victim ownership, primary/passive copying, gain effects and reset.
See RECEIVER-VALIDATION.md. A temporary NONE ability on a transformed actor
requires the source ability-index/custom-override lookup state; it remains an
explicit prerequisite in the broader ability/form copying gate.

Indexed ability lookup (merged #266): explicit index/custom/scene/daily and temporary fusion-form inputs now have an 8,000-case actual-source getter matrix and additive save coverage. See ABILITY-LOOKUP-VALIDATION.md. This closes transformed NONE lookup when metadata is supplied; historical actors with only concrete caches remain explicitly limited. The broader form lifecycle and copying-move slices remain open.

Form-state continuation (merged #267): ability-index clamping, ability/HP/stat refresh, major-form Tera reset and quiet callback-history/text semantics now have 8,000 actual source API cases plus 8,000 actual quiet-phase cases. See FORM-LIFECYCLE-VALIDATION.md. Native gain/loss combinations, fusion/Transform transitions, form eligibility, Battle Bond/Ash, Mega item choice and the Max caller are separate remaining contexts.

Battle Bond callback continuation: ordinary per-summon stat boosts and the separate non-fused Greninja victory/faint/init form callbacks have actual source phase/API and whole-turn matrices. See BATTLE-BOND-VALIDATION.md. The Scene.newBattle resetArenaState controller must still invoke PostBattleInit before ReturnPhase clears summon data; ordinary same-arena continuation keeps Ash. Do not substitute a reset on every wave. Fused faint naming remains outside the non-fusion own-faint matrix.

Player-cleanup continuation (2026-10-13): PostBattleInit now precedes actual
Return processing; the incoming battle type selects recall, ordinary wild
continuation keeps Ash, and wave-only data resets without clearing summon data.
The non-fused player boundary matches 8,000 actual source cases plus 256 Tera
cases, with phase order and exact text. See RESET-ARENA-VALIDATION.md. Arena-tag
removal callbacks/messages, suppression, Commander, time-of-day and other native
forms, fused return naming and complete doubles transitions remain open.

Sketch/Mimic continuation (2026-10-13): permanent/effective moves and source
PokemonMove sharing now have 8,000 API comparisons, 8,000 complete copying turns
and 2,000 Mimic-to-Transform turns. Legal command/save/reset checks cover the
new moveset metadata. See COPYING-VALIDATION.md. Rich seed777 increases to
1,951 exact covered turns; this remains a prioritization sample, not a measured
whole-core completion percentage. Native forms/fusion and suppression/arena-tag
removal are the next source-backed slices.

Native-form continuation: eight unfused species now queue their registered
ability transitions, with actual source callback/quiet and complete-turn
comparisons plus legal command/save/controller checks. See
NATIVE-FORMS-VALIDATION.md. Rich seed777 increases to 1,955 exact covered turns;
this does not establish whole-core completion. Other forms, Stance Change's
caller, fusion/Transform transitions and suppression/arena-tag removal remain.

Arena lifecycle continuation: bulk reset callbacks now precede Return/init;
Neutralizing Gas source-count/removal/recovery and Imprison's owning restrictions
are implemented. See ARENA-LIFECYCLE-VALIDATION.md for source APIs, complete Gas
singles turns, cleanup/save checks and limits. Retained summon seed777 reaches
1,914/2,000 exact cases (previously1,897); rich remains1,955, with no coverage loss.
Next concrete gap: Stance Change still mutates inline and emits form text instead
of queuing registered quiet transitions. Its leave-field revert also incorrectly
requires the ability and checks activity without field presence. Fix against
source registrations before expanding native form/fusion/Transform suppression
proofs. Pending Heal, Challenge eligibility and broader doubles remain separate.

Stance Change continuation: registered quiet transitions now use the selected
command, with field-aware ability-independent leave reversion and explicit faint/
leave delays. Source API 16,384 cases and complete singles 8,000 turns match;
rich seed777 reaches1,963/2,000 and summon1,919 with no retained loss/mismatch.
See STANCE-CHANGE-VALIDATION.md. Next: form/fusion/Transform and suppression
interactions, generic weather-form scheduling and remaining item/form registrations.

Copied native-ability continuation: the missing NoTransformAbility gate is fixed.
Actual Transform/native dispatch matches16,384 cases; complete following turns
match8,000 cases. Retained coverage is unchanged with no mismatch. See
TRANSFORM-RESTRICTION-VALIDATION.md. Generic weather-form queue placement and
native fusion/Transform form registrations remain the next concrete boundaries.

Weather-form continuation: default/explicit-delay queue placement and the
source unset temporary-ability reversion predicate are repaired. Complete-turn
comparison also repairs getAlly for reserve effective-stat queries in singles.
See WEATHER-FORM-VALIDATION.md for validation and limits. Native fusion/Transform
registrations, item/form eligibility, Pending Heal, wide levels and broader
doubles remain open. Pending Heal specifically lacks stored target-position
effects, duplicate-move handling, source validity, entry application and empty
tag lapse; implementing only the entry caller would not close the feature.

Stored-healing continuation: native Healing Wish/Lunar Dance now queue stored
effects, deduplicate per position/move, honor simulation and source validity,
apply on entry and expire when empty. Legal saves carry validated named records.
See PENDING-HEAL-VALIDATION.md for proof status and boundaries. Two thousand actual following turns also match; complete doubles remain separate. Native
fusion/Transform form registrations and item/form eligibility are next.

Base-stat ordering continuation: total/flat effects now precede fusion averaging
and Spliced Endless halving, with vitamins afterward. Actual form/fusion/Transform
APIs and complete unfused native form turns match; explicit item arguments are
bridged through legal saves. See STAT-ORDER-VALIDATION.md. Challenge FLIP_STAT,
wide levels, fused move-turn/display-name coverage, native callback interactions
and remaining item/form eligibility are still separate.

Fusion-name continuation after #277 covers native English fragment generation,
permanent/Illusion message snapshots and stored Wish/Healing Wish/Lunar Dance
names, including named save fields. See FUSION-NAME-VALIDATION.md. This does not
close full fused-turn admission or all native form callbacks under fusion and
Transform; those are the next source comparisons.

Callback-context continuation after #278 compares registered summon/turn/init/
faint callbacks and quiet phases for eight native families under fusion and
actual legal Transform, plus naturally indexed fused and owning Ditto turns.
Existing runtime gates match Source; no runtime change is needed for these
captures. See CALLBACK-CONTEXT-VALIDATION.md. Next concrete admission gap is
fused species-stat/crit/evolution items, still explicitly marked unsupported in
turn_modifiers. Broader weather registrations and partner/doubles remain.

Interlude continuation implements Source's X0/all-enemy-party-fainted queue
pruning and the selected TurnEndPhase's upcomingInterlude behavior, preserving
weather/terrain duration lapse. See INTERLUDE-VALIDATION.md. The product audit
and latest user priority move final-boss/doubles, then wide-level/Endless battle
behavior ahead of the paused fused-item slice. Product UI/progression remains
separate, and run-core changes remain coordinated with its owner.

Final-boss controller prerequisite: see `FINAL-BOSS-VALIDATION.md`. The core
transition, native Eternamax form/HP/stats and signed Recover PP have bounded
actual-source evidence. This does not close the final-boss/doubles gate:
Mini Black Hole transfers/persistence, dual commands and natural double starts,
phase-two recovery and legal Classic clears remain open. Wide levels/Endless
remain next in the battle-core priority queue.

Acupressure continuation from #285 implements the native override without
parent gates, using ordered eligible stats, the user's battle RNG and a +2
stat phase with the original source. See ACUPRESSURE-VALIDATION.md for the
API, owning-turn and host/save proof boundaries. Leading fresh doubles gaps
include Core Enforcer's acted/suppression path, After You and attention
redirection. App/run doubles and wide-level Endless remain separate.

After You continuation from #286 now queues its native message and selects the
first cached queued mover through the existing timing helper. Actual source
API and complete doubles turns are documented in AFTER-YOU-VALIDATION.md.
Suppression, attention redirection and the other sampled doubles callbacks
remain unported; these bounded coverage gains do not close whole battle-core
or wide-level Endless support.

Suppression continuation from #287 implements Gastro Acid/Core Enforcer's
distinct gates and shared condition, passive-before-primary loss callbacks,
suppression timing and weather reversion. Native recovery also exposed and
repaired premature/duplicate weather ability displays. See
SUPPRESSION-VALIDATION.md for source API/queue, owning-turn and save boundaries.
Attention/redirection, Flame Burst, ally healing, app/run doubles and wide-level
Endless remain separate source-backed work.

## Wide level storage prerequisite (2026-10-16)

Runtime level and met-level storage now use int32, with historical byte-save
migration. [Validation](WIDE-LEVEL-VALIDATION.md) covers bounded actual source
stats and score APIs plus augmented constructor/save/command continuity. Existing
EXP200/candy255 and save-slot preview limits remain, along with high-level combat
and natural Endless progression proofs. This is not whole-mode completion.

## Completion target (2026-10-16)

Upcoming work is organized around the [completion checklist](COMPLETION-CHECKLIST.md).
The historical rows above are evidence for their stated slices and need
reconciliation in the final full inventory. tools/audit-completion.py records
current explicit candidate locations for that review; its line count is not a
count of unsupported mechanics or a completion percentage.

Flame Burst continuation: actual override and downstream shield/flee paths now
match 16,384 source API cases; broad doubles gains 17 covered IDs without loss.
See [validation](FLAME-BURST-VALIDATION.md). Partner cures/healing, attention,
Commander/Order Up, wide progression/combat and the final inventory audit remain
open. The shared corrections do not close all boss/switching combinations.

## 2026-10-17 continuation reconciliation

Merged #291–#300 now cover partner cures/healing, attention/redirection,
Commander/Order Up, wide EXP/candy and summary storage, Thief/Covet, berry moves,
Xerneas active forms, doubles Bad Dreams, wide EXP event/display fields, and raw
wide combat/AI/Substitute/save migration. Each has a separate validation record;
these updates supersede the corresponding historical gap descriptions above.
Natural high-wave generation/progression callers and the final interaction and
source inventory audit remain open. The next stat-rearrangement slice documents
Speed Swap, Power Shift and Topsy-Turvy; HP splitting, pre-damage stat theft and
ability-changing effects remain concrete enabled callback gaps.

The user authorized cross-core fixes during unattended continuation on
2026-10-17 and confirmed no other lanes are active. Normal app/custom/save tests
remain required. Only the dedicated custom-only Task63 workflow is skipped;
older broad statements excluding custom testing or separately owned run-core
edits no longer describe the current instructions.

Merged #301 closes Speed Swap/Power Shift/Topsy-Turvy and effective-stat/speed-key
narrowing, including native signed-shift conversion. Merged #302 closes Quash's
raw callback and first queued owner timing. The wide highest-stat continuation
addresses the remaining int32 candidate/maximum locals in Protosynthesis and
Quark Drive. Spectral Thief's pre-damage stat theft, HP splitting and ability-
changing callbacks remain concrete next slices; these updates do not close
natural high-wave progression or the final full inventory audit.

Spectral Thief's enabled pre-damage callback and same-hit/queued-gain timing are
now covered by [SPECTRAL-THIEF-VALIDATION.md](SPECTRAL-THIEF-VALIDATION.md), including
native Multi Lens turns. HP splitting and ability-changing callbacks remain next;
this does not close the full source inventory or natural high-wave progression.

Pain Split is covered by [HP-SPLIT-VALIDATION.md](HP-SPLIT-VALIDATION.md): raw
attribute HP changes, boss gating, direct shield bypass API and owning turns.
Ability-changing callbacks and high-wave progression remain open.

Ability-changing moves now have source-backed raw callback/condition/reset and
owning-turn comparisons; see [ABILITY-MOVES-VALIDATION.md](ABILITY-MOVES-VALIDATION.md).
A static candidate scan of MoveEffectAttr subclasses still needs semantic review:
CutHpStatStageBoostAttr, CopyTypeAttr, FirstMoveTypeAttr, ResistLastMoveTypeAttr,
SecretPowerAttr, RevivalBlessingAttr and delayed/called/combined move paths.
Class names without a local switch case are candidates, not missing-mechanic counts;
some are dispatched elsewhere or unavailable at the source pin.
