# Battle-core continuation audit

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
are required separately. Custom-core CI is excluded from these battle tasks by
the user's current instruction. No full battle-core, run or hardware parity is
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
