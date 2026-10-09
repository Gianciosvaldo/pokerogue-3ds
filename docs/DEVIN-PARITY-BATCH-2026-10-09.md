# Evidence-gated task pack for Devin and other coding agents

Prepared 2026-10-09 for `Gianciosvaldo/pokerogue-3ds`, after PRs #185–#192.
Port source audited: `fad046c805b0085b836304c9c68fece8edfa0328`.
Remote `main` matched that commit and had no open PRs when this pack was written.
Upstream reference: `pagefaultgames/pokerogue` at
**`8555c08c823b856cbec4eb99ca84ea52a955836d`**, using **Node 22**.

There are **15 bounded tasks**, each with one paste-ready prompt. This is a
plan, not a claim that these implementations or new tests have been completed.
No prompt can guarantee that a model will never hallucinate. These prompts make
unsupported assumptions detectable: inspect the real code, execute the real
upstream behavior, compare independently, and stop rather than invent a missing
contract. Smaller scope does not mean weaker evidence.

## How to use this pack

Copy the complete prompt under one task into an agent with repository access.
It tells the agent to read this file, including the shared contract below; you
do not need to paste the shared contract separately. Merge a reviewed prerequisite
before starting its dependent task. Refresh `main` and check for duplicate work
every time. Keep **one active task per lane**, even if two agents are available.
Lane A and lane C can run in parallel in separate ordinary checkouts. Do not
share a dirty checkout or run two lane A tasks simultaneously.

The task IDs here are **D01–D15**, not replacements for the existing numbered
cards. They provide bounded slices of the ledger's remaining moves/abilities,
Task 96 item callers, and documented diagnostic gaps. The October 4 uploaded
`roadmap-after-parity-ledger-2026-10-04.md` informed priorities; its absence
claims are dated planning context, not instructions or current implementation
evidence. For example, trainer routing, Growth, Speed Boost, Mint, vitamins,
Wide/Multi Lens acquisition, Classic terminal bookkeeping and gacha generation
already have merged work. Do not reimplement them from that roadmap.

| ID | Task | Lane / write scope | Prerequisite in this pack | Deliverable |
|---|---|---|---|---|
| D01 | Psych Up: CopyStatsAttr | A / battle-core | None | Missing leaf + real-turn proof |
| D02 | Power Split / Guard Split: AverageStatsAttr | A / battle-core | None | Missing leaf + temporary-stat proof |
| D03 | Camouflage: CopyBiomeTypeAttr | A / battle-core | None | Missing leaf + exact type-map proof |
| D04 | PartyStatusCureAttr | A / battle-core | None | Bounded singles/bench cure leaf |
| D05 | Fell Stinger post-victory callback | A / battle-core | None | Missing callback leaf + KO proof |
| D06 | Bad Dreams post-turn callback | A / battle-core | None | Missing leaf + indirect-damage proof |
| D07 | Cheek Pouch actual berry caller | A / battle-core | None | Existing handler proof; fix only demonstrated bugs |
| D08 | Canonical Toxic/Flame Orb acquisition caller | C / game-core | Merged reward API #182 | New acquisition caller; preserve existing bridge |
| D09 | Soothe Bell acquisition caller | C / game-core | Merged reward API #163 | Canonical inventory/selection caller |
| D10 | Soothe Bell battle friendship gain | C / game-core | D09 merged | Bounded positive battle-gain caller |
| D11 | Lure acquisition caller | C / game-core | Merged reward API #153 | Canonical add/refresh/selection caller |
| D12 | Lure BattleEnd lapse | C / game-core | D11 merged | Actual timer lifecycle, save/resume |
| D13 | Lure encounter-chance input | C / game-core | D12 merged | Input integration, not double-battle execution |
| D14 | ITEMS missing-text root-cause report | G / docs only | None | Reproducible investigation, no harness repair |
| D15 | Optional trainer-phase metadata proof | A / battle-core | None | Additive proof, no legacy comparator repair |

Suggested order: A: D01 → D02 → D03 → D04 → D05 → D06 → D07 → D15;
C: D08 → D09 → D10 → D11 → D12 → D13. D14 is independent and can be used as
a low-risk first task. D01 and D08 are useful independent implementation starts.
Finish only the assigned card; this pack itself does not authorize an agent to
start the other fourteen tasks or merge its own PRs.

## Shared contract — mandatory for every prompt

### 1. Establish facts before editing

Read root `README.md`, `docs/HANDOFF.md`, `docs/PARITY-TASKS.md`,
`docs/PARITY-LEDGER.md`, the owning folder's README, and the owning validation
records referenced by the card. Obey applicable `AGENTS.md` instructions.
Record the current port SHA, clean/dirty status, existing relevant branches/PRs,
and the exact upstream SHA. Preserve other people's changes. Use a new branch
from current `origin/main`; never push directly to `main`. Use an existing
checkout or a separate full clone if isolation is necessary.

Search current C dispatchers, generated attributes, runtime callers, tests and
validation records using the **symbols**, not only move/item names. Consult
merged/open PRs when accessible. If the feature and requested proof already
exist, report their exact files/PRs and stop: no duplicate implementation, empty
PR, or unrequested replacement task. If GitHub history is inaccessible, state
that limitation; do not claim the duplicate check included PRs.

Keep upstream outside the port repository. Use the repository's documented
setup workflow (`battle-core/tools/setup-upstream.sh` and owning READMEs), then
verify its SHA, Node major, dependencies, locales and headless assets. Do not
switch to Node 24 because upstream's package manifest requests it. Node 22 is
the port's settled numerical reference. Do not alter lockfiles, engine rules,
the upstream reference, TLS checks or checksums to make setup succeed.
Cloud-specific `/workspace/shared/...` paths are not prerequisites for Devin.

Before production edits, put a short evidence table in the task's validation
record: **upstream file + symbol → relevant source lines → intended C behavior
→ fixture which will verify it**. Inspect the class's parent implementation,
factory arguments, conditions, actual phase caller, suppression/simulation
gates, state ownership, RNG calls and queue order. A name, wiki, another
generation's rules, another move's implementation, or an AI summary is not
evidence. If this document and the pinned code disagree, quote the discrepancy
and stop the affected task before implementing that behavior.

### 2. Stay within the card's boundary

One task, one branch, one PR. Only the named lane folder may change, including
its Makefile, owned headers, new tests/tools and validation record. Other folders
are read-only; do not fix their APIs as a side effect. Do not change
`run-core/` (reserved owner), `game/`, hardware/UI/audio/assets, final-boss
transition logic, general fusion/form/doubles rules, network/account persistence
or hatching in this pack. Missing representation or missing cross-folder API
is a blocker, not permission to invent one or silently narrow the promised task.

Use existing phase/stat/tag/item APIs after verifying their contracts. Do not
add a move-ID/ability-ID special-case switch when the source uses a generated
attribute payload. Never hand-edit `*/gen/out/*`; use the owning redump workflow
if truly necessary and inspect the diff. If a redump or runtime API change
would cross lanes, stop and name the prerequisite. Do not copy upstream source
files or commit upstream dumps/assets, traces, logs, binaries or secrets.
New authored producers may import the real upstream classes in the external
checkout. Store raw evidence outside the repository; commit the concise
reproduction record and owned test code.

### 3. Preserve established checks and source independence

Do not edit existing `*.producer.test.ts`, comparators, tolerances, counts,
seeds, coverage criteria, exclusions, auto chooser or digest logic. Add separate
tests and drivers. If an established test is defective, retain the failure,
write the evidence, and stop its repair. D14/D15 specifically investigate known
defects without permission to rewrite the old checks.

The upstream producer must execute the **actual source method/phase** being
ported. Wrappers may observe and delegate; they must not replace the behavior
under test with hand-calculated answers. C output must never determine source
input, expected output or which cases count as covered. Fixture setup can stub
unrelated rendering/storage boundaries, but list each stub and every excluded
observable. Calling only a helper does not establish its runtime caller.

For D01–D13, add a dedicated owning-folder producer, replay driver and wrapper.
Use deterministic boundary fixtures plus **2000 source cases per seed for
777, 9001, 4242, 31337**. All 8000 requested cases must execute and compare;
explicitly bounded fixtures must not vanish as skipped/unsupported. Positive
and refusal branches need printed counts, not just random chance of appearing.
An unsupported broader context may be tested separately as a guard; it must
not be counted as one of the 8000 source comparisons. D14/D15 have their own
more appropriate minimums below. Existing stronger owning checks still apply.

Wrappers must verify the upstream SHA and Node 22, validate integer count/seed
inputs, preserve producer/replay exit codes, use unique output paths, and reject
empty, truncated, malformed, duplicate, out-of-order or count-mismatched traces.
Replay must require the declared header, exact completed case count and footer,
validate enum/owner/index ranges, and reject trailing cases. Add negative trace
checks for missing footer, wrong count and malformed required values. Log parse
errors separately from semantic mismatches; a zero-case success is a failure.

Prove the new test can detect at least one realistic temporary mutation to the
**actual C dispatch/caller** (each move identity or materially separate callback
when the card requests it). Record the mutation, failing command/count and exit
status, revert it, and rerun the clean check. A changed expected value or a
standalone copied formula is not a runtime mutation proof.

### 4. Run the real regressions and report accurately

Capture baseline **before edits**, and final results on the actual proposed
head. Lane A must retain command contract **B** in `docs/PARITY-TASKS.md`, plus
existing item/condition/weather/trainer suites and focused suites relevant to
the change at their documented counts/seeds. Reuse baseline source traces for
before/after replay where appropriate; hash them. Lane C must retain its owning
item caller/bridge suites and the prerequisite reward suites read-only.
D01–D13 also require contract **H**, before and after:

```sh
make -j2 -C game-core test
game-core/build/gcplay --auto 1000 --jobs 8
game-core/build/gcplay --auto 1000 --jobs 8 --save-test
```

Normal/save results must match **each other on the same build**. A legitimate
feature may change the baseline digest/turn count; explain that with source
evidence, never force an old digest. Report actual source counts, coverage,
differences, rendered-text completeness, unsupported reasons, host checks,
ended/cleared/stuck/crashed/timed-out counts, saves, turns and digests.
Ordinary CI's host and 3DS jobs must pass on the exact proposed PR head;
conditional/skipped source jobs are not passes. Do not claim hardware was run.
If dependencies or compute prevent a required check, leave a draft/incomplete
PR and list the unrun command. Do not mark the task complete.

Historical warnings which must remain visible, dated at this pack's baseline:

- General `TURN_TRAINER`, seed 4242, CASE 1051 has a pending non-Pokemon
  `SwitchSummonPhase` without source `battlerIndex`. The old transport compares
  `SwitchSummonPhase:` with `SwitchSummonPhase:-1`. This remains **FAILED**;
  see `battle-core/TRAINER-SWITCH-VALIDATION.md`.
- Established ITEMS strict text-completeness failures are **1/1/2/1** on the
  four seeds. State-only exit 0 does not make those text checks pass.
- Four legacy damage traces retain **2000/2000/2000/1999** coverage; the source
  Fusion Flare throw on seed 31337 is retained. Do not recategorize it as a pass.
- Current 1000-run normal/save host checkpoint ends all runs with **0 clears**,
  50410 turns and matching `75bb4db1`. Diagnostic wave-200 reach also has
  **0 clears**, 94 defeats and six real capture-command refusals, expected exit
  1. Terminal fixtures/gacha generation/digest agreement do not prove a legal
  Classic completion. Historical counts are context, not frozen future outputs.

### 5. Deliver a reviewable result or a precise blocker

The owning validation record and PR must contain: base/head/pinned source SHAs;
tool versions; source evidence table; files/APIs changed; exact wrapper/producer/
driver commands; requested/completed/covered/identical/mismatch/skip/throw and
branch counts; baseline/final regressions; malformed-trace and mutation results;
save/queue/RNG/side-effect boundaries; known failed and unrun checks; exclusions.
Include producer/trace hashes and retained evidence location. Label a method
proof, caller proof, host consistency and whole-run parity separately.

Do not mark the overarching ledger card complete or erase unsupported flags
outside the newly proved domain. No failures may become exclusions to gain a
pass. New unsupported guards must describe genuinely unrepresented inputs and
be tested independently, not hide a failing source fixture.

If blocked, report **BLOCKED**, the exact source/C symbols, why the named scope
cannot implement them safely, the smallest prerequisite, commands actually run
and any incomplete changes. Do not fabricate test results or keep coding by
guessing. Stop after the card and leave its PR for review. Codex's personal
merge authorization in HANDOFF is not automatically Devin's authorization.

## D01 — Psych Up: copy stages and critical tag

Audited gap: generated `CopyStatsAttr` exists, but no case in the C move-effect
dispatcher. This is not another generic stat-stage raise.

```text
Implement only D01 from docs/DEVIN-PARITY-BATCH-2026-10-09.md. Read and obey its
entire shared contract first. Scope: battle-core/ only, lane A, one PR. Target
upstream 8555c08c823b856cbec4eb99ca84ea52a955836d with Node 22. Recheck current
main for CopyStatsAttr/Psych Up implementations and proofs before editing.

Read src/data/moves/move.ts: CopyStatsAttr, MoveEffectAttr, BATTLE_STATS, and
the PSYCH_UP factory; src/field/pokemon.ts: get/setStatStage and add/removeTag;
the CRIT_BOOST tag definition and MoveEffectPhase's real dispatch. In C inspect
src/move_effects.c, stat-stage storage, tag APIs and gen/out/pr_attrs.h.

Source copies every BATTLE_STATS stage directly from target to user. It copies
the presence of CRIT_BOOST by addTag(CRIT_BOOST,0,move.id), or removes the user's
tag when target lacks it. It queues moveTriggers:copiedStatChanges with the real
names. Preserve parent gates and source tag-add behavior for an existing tag.
Do not run copied values through StatStageChangePhase, Simple or Contrary; do
not copy base/effective stats, held items or arbitrary tags.

Add the generic leaf through the existing dispatcher and an additive actual
Psych Up turn producer/driver. Balance both acting sides; all seven stages
including accuracy/evasion at -6,0,+6; unequal user/target stages; no/one/both
critical tags; pre-existing tag metadata; Simple/Contrary controls; Substitute
and protection behavior from the actual factory/parent; unsuccessful move gates.
Compare stage arrays, tag metadata, unchanged target/base stats, PP/history,
queues, rendered texts and both RNG states. Probe a subsequent critical helper
through existing APIs to expose wrong tag transfer, without claiming all future
turns. Mutations: omit accuracy/evasion copy; leave stale critical tag.

Create new owned test files/tools; suggested stem psych-up (new, not an existing
command). Record their actual commands in battle-core/PSYCH-UP-VALIDATION.md.
Require the shared 8000 source cases, strict malformed guards, B/relevant old
suites and H. Missing tag/state observability is a blocker, not a skipped case.
Stop after this PR; no whole move/ability/fusion/doubles completion claim.
```

## D02 — Power Split and Guard Split: temporary stat averaging

Audited gap: generated `AverageStatsAttr` exists without a dispatcher case.
Existing `pr_pokemon.summon_stats` and `pr_get_stat(..., false)` are relevant
storage, but their complete write/reset contracts must be checked.

```text
Implement only D02 from docs/DEVIN-PARITY-BATCH-2026-10-09.md, obeying the shared
contract. Lane A, battle-core/ only, one PR; pinned upstream
8555c08c823b856cbec4eb99ca84ea52a955836d, Node 22. Deduplicate first.

Read move.ts: AverageStatsAttr, MoveEffectAttr, POWER_SPLIT and GUARD_SPLIT
factories; pokemon.ts: getStat/setStat and summon-data resets. The source reads
getStat(stat,false) for both Pokemon, computes Math.floor((a+b)/2), and calls
setStat(stat,average,false) on both. Power Split selects ATK/SPATK; Guard Split
selects DEF/SPDEF. Generated stats/msgKey payload controls dispatch. This is
not averaging stat stages or replacing permanent stats/IVs. Source getStat's
zero override falls back to base; setStat ignores values <=0.

Inspect C src/pokemon.c, src/move_effects.c, include/pr/battle.h summon_stats,
switch/reset paths and generated payload. Add the missing leaf using existing
temporary storage; do not invent a permanent-stat rewrite or edit generated
tables manually. If the required storage/reset API is absent, stop and report.

Build additive actual-move source fixtures for both moves and sides: odd/even
sums, unequal base stats, existing unequal temporary overrides, stage changes
which must remain unchanged, repeated split reading the new overrides, parent
refusals/protection/Substitute, and switch-out reset using an already ported
ordinary switch. Record base stats AND temporary overrides explicitly in the
new trace, plus effective-stat probes, state/queue/text/RNG. Test a later damage
calculation to expose wrong storage, using existing tested APIs. Do not modify
the established turn trace format. Each move needs positive/refusal counts and
a detected actual-dispatch mutation (round up instead of floor, or overwrite
base stats). Do not add final-boss checks that are commented out upstream.

Suggested new test stem average-stats; record commands and 8000 complete source
comparisons in battle-core/AVERAGE-STATS-VALIDATION.md. Retain B, relevant stat/
item/switch regressions and H. Keep scope ordinary singles, no broad form/fusion
completion. Stop after the PR.
```

## D03 — Camouflage: use the source terrain/biome map

Audited gap: `CopyBiomeTypeAttr` is generated but lacks a C effect handler.
The map must come from this class, not Nature Power's different map.

```text
Implement only D03 from docs/DEVIN-PARITY-BATCH-2026-10-09.md. Read the shared
contract. Lane A; battle-core/ only; one PR; upstream
8555c08c823b856cbec4eb99ca84ea52a955836d; Node 22. Check current main first.

Read move.ts: CopyBiomeTypeAttr.apply/getTypeForTerrain/getTypeForBiome,
MoveEffectAttr and CAMOUFLAGE factory; pokemon.ts summonData.types/getTypes;
terrain and biome enums; real MoveEffectPhase. TerrainType.NONE selects the
biome map; any other terrain selects the terrain map, whose default is UNKNOWN.
The source replaces summonData.types with ONE type, calls updateInfo, and queues
moveTriggers:transformedIntoType. Do not infer types from backgrounds, weather,
Nature Power, or a Pokemon wiki. Read whether addedType is preserved; do not
clear it just because that seems logical. Exclude Terastallization/fusion.

Inspect C type override/cache representation, ChangeTypeAttr/ReflectTypeAttr
and switch resets. Implement the generic generated class through existing
type APIs. Extract the full pinned map into the validation evidence table and
exercise EVERY defined biome with no terrain, all four ordinary terrains over
conflicting biomes, and the method's UNKNOWN fallback in a separate legal
helper probe if that input is not a legal whole turn. Important source examples:
Town=Normal, Forest=Bug, Slum=Poison, Mountain=Flying, Desert=Rock,
ConstructionSite=Fighting, Laboratory=Steel, End=Dragon, Abyss=Dark.

Use real Camouflage turns, both sides, monotype/dual-type/previous override,
added type, parent refusal, unchanged target, subsequent STAB/type-effectiveness
probes and a proved ordinary switch reset. Compare actual type lists, tag/base
state, PP/history, queues, rendered texts and RNG. Mutations must detect a wrong
biome mapping and failure to let terrain take precedence in the real handler.

Suggested new stem camouflage; commit battle-core/CAMOUFLAGE-VALIDATION.md with
actual commands, branch counts, 8000 comparisons, strict guards, B/relevant type/
terrain/switch suites and H. Stop if type storage cannot represent the source;
do not implement a guessed array/sentinel convention. Finish this PR only.
```

## D04 — Party status cures, including off-field members

Audited gap: `PartyStatusCureAttr` has three generated uses and no C leaf.
Ordinary singles still require the **whole acting party**, not just the active
Pokemon. Natural doubles and partner display behavior are a separate scope.

```text
Implement only D04 from docs/DEVIN-PARITY-BATCH-2026-10-09.md and its shared
contract. Lane A; battle-core/ only; one PR; upstream
8555c08c823b856cbec4eb99ca84ea52a955836d, Node 22. Deduplicate PartyStatusCureAttr.

Read move.ts: PartyStatusCureAttr.canApply/apply/cureStatus and the HEAL_BELL,
AROMATHERAPY, SPARKLY_SWIRL factories; pokemon.ts resetStatus; source party/field
identity and the real move caller. Heal Bell uses bellChimed/SOUNDPROOF;
Aromatherapy uses soothingAromaWaftedThroughArea/SAP_SIPPER; Sparkly Swirl has
message null/Ability.NONE. Use generated message/abilityCondition, not move-ID
branches. Its custom canApply deliberately does NOT check target HP.

Source cures every off-field member and the user with resetStatus(false), even
if that Pokemon has Soundproof/Sap Sipper. Only another ON-FIELD party member
uses the abilityCondition/display branch. Do not impose that immunity on bench
members, or automatically reset confusion/other volatile tags. Inspect the exact
resetStatus default/false semantics and mutation/display/message order first.

Implement bounded ordinary singles with loaded player/enemy bench parties via
existing party APIs. Do not turn this into natural doubles support. If safe
coverage cannot be delimited with current dispatch/state representation, stop
rather than enable an unproved whole-party branch. Exercise all three factories,
both acting sides, party sizes1-6, mixed statuses including sleep/toxic counters,
fainted bench, already healthy members, bench/user immunity abilities and
unrelated enemy status; source protection/target-HP gates and Sparkly Swirl's
hit/miss. Explicitly compare every member, retained volatile tags, messages,
ability displays, queue, PP/history and RNG. Include a direct source probe of
the active-partner branch as a documented exclusion check; do not claim it is
implemented merely from that probe. Mutations: cure only the user; wrongly
exclude Soundproof bench. Require detection and nonzero cases per factory.

Suggested stem party-status-cure; own record
battle-core/PARTY-STATUS-CURE-VALIDATION.md. Shared 8000 source cases, strict
guards, B/condition/status/switch suites and H. No new doubles/party creation,
revival or full Task104 completion. Stop after the PR.
```

## D05 — Fell Stinger's actual faint callback

Audited gap: `src/phases.c` detects `PostVictoryStatStageChangeAttr` and calls
`pr_note_unported`; that callback is not yet implemented.

```text
Implement only D05 from docs/DEVIN-PARITY-BATCH-2026-10-09.md. Obey the shared
contract; lane A; battle-core/ only; one PR; pinned upstream
8555c08c823b856cbec4eb99ca84ea52a955836d on Node 22. Recheck current dispatch.

Read move.ts: PostVictoryStatStageChangeAttr, StatStageChangeAttr and FELL_STINGER
factory; FaintPhase.doFaint; StatStageChangePhase. Source checks that victim
attacksReceived has entries and defeatSource is still on field; it reads
attacksReceived[0].move, runs PostVictoryAbAttr first, then calls each move
attribute with defeatSource as BOTH user and target. Do not substitute the last
attack or trigger on every KO. The move payload is ATK,+3. Constructor
_selfTarget is ignored by this class; applyPostVictory passes showMessage as
StatStageChangeAttr's third argument (selfTarget at this pin). Inspect and
preserve this real call, not the misleading parameter names.

Use the existing source-equivalent stat-phase and condition/chance machinery
from the actual C faint path, with generated payload; no immediate hard-coded
ATK write and no Moxie/Beast Boost/Battle Bond work. The proof must execute actual
Fell Stinger damage/KO and queue drain, not only call a helper with a dead target.
Test both sides, KO/no KO, attack-stage floors/caps, Simple/Contrary, multiple
attack history entries with deliberately different first/last move in a source
legal focused fixture, absent attack history, defeat source off field, and an
indirect faint control. Distinguish hand-constructed callback probes from legal
whole-turn cases. Observe phase order versus existing victory abilities without
requiring an unported ability implementation. Compare full state, queue/text,
history, RNG and boost/refusal counts. Preserve unrelated unported faint paths.

Mutation: use last rather than first attacksReceived, and skip the queued stat
phase. Suggested new stem fell-stinger; record exact commands in
battle-core/FELL-STINGER-VALIDATION.md. Require 8000 complete bounded comparisons,
strict guards, B/trainer/stat/faint regressions and H. Missing callback state or
an unrepresented condition is a blocker. Stop after this PR.
```

## D06 — Bad Dreams: preserve the pinned indirect-damage behavior

Audited gap: `PostTurnHurtIfSleepingAbAttr` is generated but lacks a C leaf.
This source contains a surprising holder/victim distinction; do not “repair”
the upstream rule from memory.

```text
Implement only D06 from docs/DEVIN-PARITY-BATCH-2026-10-09.md; shared contract
mandatory. Lane A; battle-core/ only; one PR; upstream
8555c08c823b856cbec4eb99ca84ea52a955836d; Node 22. Deduplicate first.

Read ab-attrs.ts: PostTurnHurtIfSleepingAbAttr.canApply/apply and parent classes;
init-abilities.ts BAD_DREAMS; apply-ab-attrs.ts; TurnEndPhase; pokemon.ts
getOpponentsGenerator/damageAndUpdate; toDmgValue and BlockNonDirectDamageAbAttr.
Source canApply needs an opponent sleeping or with Comatose, not switched out,
and without BlockNonDirectDamageAbAttr. apply returns if simulated. Its loop
skips non-sleeping/non-Comatose or switched-out opponents, then applies
BlockNonDirectDamageAbAttr to the ABILITY HOLDER pokemon, not the opponent.
If not cancelled it damages the opponent by toDmgValue(maxHP/8), INDIRECT, and
queues abilityTriggers:badDreams. Preserve that pinned argument choice; report
it with source evidence rather than replacing it with a canonical-game rule.

Add the generated ability leaf through existing post-turn/indirect-damage APIs.
Bound to ordinary singles. Fixtures: both sides, sleeping/awake/Comatose,
odd and tiny maxHP (minimum1 damage), HP1/KO/full, opponent Magic Guard vs holder
Magic Guard (using a legal primary/passive fixture), suppression, switched-out
holder/victim, simulated callback, and waking before end-turn. Separate direct
leaf probes for inputs not reachable in a legal turn and label them. Compare
HP/faint/history/indirect bookkeeping, displays, queue order, rendered texts,
both RNG states and positive/refusal counts. Do not count it as direct damage,
invent a damage roll, or skip source gates. Natural doubles/form/fusion excluded.

Mutation: wrong holder in the blocker check; wrong rounding in the real leaf.
Suggested new stem bad-dreams; own record battle-core/BAD-DREAMS-VALIDATION.md.
Shared 8000 comparisons/strict guards, B/status/post-turn/ability regressions
and H. Stop if indirect damage or blocker invocation needs an absent API. End
with this PR only, with the source quirk and bounded scope clearly recorded.
```

## D07 — Cheek Pouch: prove the existing BerryPhase integration

Audited existing code: `ab_attrs.c` already handles `HealFromBerryUseAbAttr`;
`phases.c` already calls it after berry use. The older berry suite checks raw
berry behavior, so this task is specifically an **actual caller proof**, not a
claim that Cheek Pouch has no implementation.

```text
Do only D07 from docs/DEVIN-PARITY-BATCH-2026-10-09.md and obey its shared
contract. Lane A, battle-core/ only, one PR. Target upstream
8555c08c823b856cbec4eb99ca84ea52a955836d on Node 22. Search existing Berry/Cheek
Pouch proofs; stop if this caller coverage has already landed.

Read ab-attrs.ts HealFromBerryUseAbAttr; init-abilities.ts CHEEK_POUCH (1/3);
BerryPhase.eatBerries; PokemonHealPhase; BerryModifier and applyAbAttrs gates.
The source constructor clamps healPercent to[0,1]; the leaf returns for simulated,
uses primary/passive abilityName, and queues toDmgValue(maxHP*healPercent). The actual
BerryPhase calls it once AFTER the round of berries, modifier updates and queued
berry stat changes. It is not one Cheek Pouch heal per berry stack.

C has an existing leaf using maxHP/3 and has_eaten_berry and an actual callback
in phases.c. Prove or disprove these gates against the actual source caller;
do not presume that a persistent has_eaten_berry flag is enough this turn. Do
not reimplement the entire berry engine or change the established berry suite.

Add actual full-turn/BerryPhase source fixtures: Sitrus/Lum/Leppa, multiple
eligible berry records in one round, no usable berry but previously eaten,
Unnerve preventing use, ordinary/passive Cheek Pouch, suppression, full/near-full
HP, tiny/odd maxHP, healing-block tag, fainted holder and simulated direct leaf.
Observe consumption, remaining stacks/order, hasEatenBerry, heal count/order,
HP caps, displays/abilityName, texts, full state and RNG. Print nonzero consumed,
blocked, no-new-berry and primary/passive counts. Fix only a demonstrated C
discrepancy in the named path; a proof-only PR is acceptable if it already
matches. Do not invent extra percentage factories to justify production work.

Mutation: duplicate the actual caller or remove its invocation; verify the new
suite fails. Suggested stem cheek-pouch; record in
battle-core/CHEEK-POUCH-VALIDATION.md. Shared 8000 source comparisons, strict
guards, B, established berry/item/condition suites and H are required. Cud Chew,
berry theft/transfer and natural doubles remain outside this task. Stop here.
```

## D08 — Canonical Toxic/Flame Orb acquisition in game-core

Prerequisite already merged: reward-core #182, separate
`pr_reward_add_canonical_orb_item`. Existing game-core #154 proves loaded Orb
inventory reaching battles, not canonical Scene acquisition.

```text
Implement only D08 from docs/DEVIN-PARITY-BATCH-2026-10-09.md and its shared
contract. Lane C; game-core/ only, all other cores read-only; one PR; upstream
8555c08c823b856cbec4eb99ca84ea52a955836d, Node 22. Verify main has reward-core
pr_reward_add_canonical_orb_item and its recorded #182 proof; deduplicate caller.

Read reward-core/README.md canonical Orb section and game-core/ORB-VALIDATION.md,
WIDE-ACQUISITION-VALIDATION.md and MULTI-ACQUISITION-VALIDATION.md. Source: actual
TOXIC_ORB/FLAME_ORB factories in modifier-type.ts; PersistentModifier.add;
TurnStatusEffectModifier.matchType; BattleScene.addModifier/updateModifiers;
SelectModifierPhase's public selection/filter/purchase paths. Both Orbs share
one source class and cap1: the opposite Orb on the SAME holder also refuses
FULL. Raw wildcard owner does not match acquisition. Do not change the older
opaque-Orb contracts or blindly clone Wide Lens's caller rules.

Add a bounded caller in game-core using the separate merged canonical API.
Likely integration points: gc_reward.c, internal.h, Makefile; a new gc_orb_add.c
is suggested, not pre-existing. Storage index -> party_order -> unique Pokemon
ID/owner mapping must be proved. Preserve inventory identity/order/other fields;
normalize constructor arguments only in a validated shadow context. Follow
actual source Scene return value, updates, selection-filter FULL refusal, direct
Scene tier replacement, free selection completion and paid error/wallet behavior
even when the pinned source mutates stock before returning false.

New source fixtures must execute actual Scene and public selection/purchase,
not only .add: both Orbs, empty/existing/opposite/same class, other holder,
wildcard/detached cleanup, reordered/fainted parties, duplicates, capacity,
free/paid/full/tier paths, save/reload at a legal command boundary and the
existing actual Orb battle projection. Unsupported malformed/virtual/challenge/
fusion or unrepresented update contexts must remain explicit counted fallback
with no unintended mutation; do not degrade existing supported legacy paths.
Compare result/wallet/inventory/order/flags/party/RNG/queue and message events;
claim rendered text only if independently compared. Mutation: allow opposite
Orb stacking, or map reordered holder wrongly, in the real game caller.

Suggested new stem orb-acquisition; record
game-core/ORB-ACQUISITION-VALIDATION.md. Shared 8000 cases/strict guards, unchanged
raw #182 and old Orb bridge plus acquisition regressions, H and exact-head CI.
No transfers/status-engine/run-core changes. Stop after this PR.
```

## D09 — Soothe Bell acquisition, before friendship effects

Prerequisite already merged: reward-core #163,
`pr_reward_add_soothe_bell`. This task does not yet alter friendship gains.

```text
Implement only D09 from docs/DEVIN-PARITY-BATCH-2026-10-09.md and its shared
contract. Lane C; game-core/ only; one PR. Upstream
8555c08c823b856cbec4eb99ca84ea52a955836d, Node 22. Verify merged
pr_reward_add_soothe_bell and reward-core's Soothe Bell proof; deduplicate.

Read modifier-type.ts SOOTHE_BELL factory; modifier.ts
PokemonFriendshipBoosterModifier/PersistentModifier.add; actual Scene.addModifier,
updateModifiers and SelectModifierPhase filter/free/purchase. Read reward-core
README #163 and game-core Wide/Multi acquisition records. Reward API validates
unique party IDs, canonical Bell arg0/count0/stack1-3, exact-or-wildcard owners;
acquisition matches actual holder and caps at3. Validate precise source wildcard
and duplicate behavior instead of inferring it from the gain helper.

Add a bounded canonical acquisition caller, suggested gc_soothe.c, with
gc_reward.c/internal.h/Makefile integration. Preserve original inventory arg
representation, stock order, unrelated opaque stock and actual party_order/ID
mapping. Source full-stack selection filtering and Scene tier fallback are
different entry points; record actual return/wallet/error/update/cleanup behavior
for free and paid use. Do not assume “successful mutation” means paid success.
Do not edit reward-core or add a friendship multiplier in this PR.

Fixture matrix: Bell absent/stacks1,2,3; duplicate and wildcard records; wrong
holder; reordered/fainted parties; mixed known stock; valid capacity/full path;
free/paid/direct Scene/filter refusal; detached/virtual/invalid guards; save
round-trip. Observe actual source method calls, whole inventory/order/flags,
party/wallet/RNG/queue/messages; friendship must remain unchanged on acquisition.
Use real Scene/public caller for the 8000 comparisons, not just raw add. Source
update/recalc contexts which cannot be represented are explicit boundaries.
Mutation: ignore holder identity or raise the cap in the actual game caller.

Suggested stem soothe-acquisition; create game-core/SOOTHE-ACQUISITION-VALIDATION.md
with exact commands, strict guards and shared source counts. Retain raw
soothebelldiff.sh and existing game acquisition/save regressions, H and CI.
No candy/battle friendship/transfers/fusion events in this task. Stop after PR.
```

## D10 — Soothe Bell at the positive battle-friendship caller

Prerequisite: D09 merged. This card deliberately excludes Rare Candy's separate
application path, whose existing C caller overwrites friendship from reward
output after `gc_system_add_friendship`. Mixing both paths would hide a double
application or discarded gain.

```text
Implement only D10 from docs/DEVIN-PARITY-BATCH-2026-10-09.md and shared contract.
Lane C; game-core/ only; one PR; upstream
8555c08c823b856cbec4eb99ca84ea52a955836d, Node 22. Require merged D09; verify
pr_reward_soothe_bell_gain exists. Check this exact battle caller is not done.

Read pokemon.ts Pokemon.addFriendship; modifier.ts
PokemonFriendshipBoosterModifier.apply/shouldApply; BattleScene.applyModifier
(singular) and battle friendship distribution. Read gc_system_add_friendship
in gc_system.c, the actual positive participating-survivor gain caller in
gc_phases.c, and Rare Candy's later overwrite in gc_reward.c. The raw Bell API
matches only the FIRST eligible exact/wildcard record, multiplying by
floor(base*(1+0.5*stack)). Source uses the boosted amount for BOTH Pokemon
friendship and starter candy progress, even when Pokemon friendship is capped.
Nonpositive friendship exits before modifiers/candy. Do not stack all Bells.

Implement only ordinary non-fused, non-event positive BATTLE gain (+3 at the
current caller), preserving existing no-Bell behavior and other callers. Use
the merged reward helper through a validated read-only inventory projection.
Keep Rare/Rarer Candy, loss/gain APIs outside this new behavior; do not silently
change shared gc_system_add_friendship for them or discard a boosted value via
reward's overwrite. If the boundary cannot be expressed with existing APIs,
stop and report the smaller caller prerequisite instead of broadening scope.

Source fixtures execute real battle friendship distribution plus actual
Pokemon.addFriendship. Test surviving participants vs fainted/nonparticipants,
party reorder, no Bell/stacks1-3, first wildcard before/after exact and duplicate
records, wrong holder, starting friendship199/200/254/255, starter candy progress
around its exact cost-based threshold, successful/refused candy award, Classic
vs an already-supported non-Classic mode with event multiplier explicitly
fixed to the source's ordinary value. Compare full Pokemon friendship, starter
progress/candies, inventory, queue/events, wallet and RNG; separately prove
unchanged nonpositive/capped/Rare Candy controls without claiming Bell support
there. Exclude ribbon/achievement persistence unless actually represented.

Mutation: apply Bell twice, or award starter progress from capped HP-style delta
instead of full friendship amount, in the actual caller. Suggested stem
soothe-battle; own record game-core/SOOTHE-BATTLE-VALIDATION.md. Shared 8000 cases,
strict guards, raw Bell/candy/acquisition baselines and H. Reject unrepresented
contexts before new mutations, preserving legacy fallback. Stop after this PR.
```

## D11 — Canonical Lure acquisition and refresh

Prerequisite already merged: reward-core #153,
`pr_reward_add_lure`. Lure timers and encounter inputs are separate cards.

```text
Implement only D11 from docs/DEVIN-PARITY-BATCH-2026-10-09.md; obey shared contract.
Lane C; game-core/ only; one PR; upstream
8555c08c823b856cbec4eb99ca84ea52a955836d, Node 22. Verify merged reward Lure API
and source proof, and check current game caller before adding anything.

Read modifier-type.ts LURE/SUPER_LURE/MAX_LURE factories; modifier.ts
DoubleBattleChanceBoosterModifier and LapsingPersistentModifier.add; actual
Scene.addModifier/updateModifiers and SelectModifierPhase free/purchase paths.
Read reward-core README Lures and game-core/TEMP-VALIDATION.md, but verify the
Lure caller independently. Raw durations10/15/30; side-wide owner-1; arg0;
real stacks1-2. Matching is first same source class AND maxBattles. A partially
expired match refreshes battleCount to max without changing stack/order; an
already-full match returns false. A new record starts stack1 at full duration.
Do not turn refresh into incrementStack, sum durations, or combine Lure kinds.

Add bounded gc_lure acquisition helper(s), internal/Makefile and gc_reward.c
hooks, using pr_reward_add_lure. Preserve the original inventory fields and
named battle_count save transport; do not erase older opaque count0 records
or invent their remaining lifetime. Prove actual Scene success/error, free vs
paid wallet handling, public full filtering and direct Scene tier replacement
from the source. Keep temporary X/Dire acquisition unchanged.

Fixtures: each Lure kind; absent; last tick/partial/full; stack1/2; duplicate
matches; mixed Lures/temporary/held stock; capacity; free/paid/refusal/fallback;
legal save/reload; malformed/virtual/held/detached/legacy timer guards. Execute
actual source Scene/public selection and compare result/money, full stock
identity/order/flags/counts, party/RNG/queues/messages. No lapse or encounter
change in this PR. Mutation: increment stack on refresh, or match different
maxBattles, in the real game acquisition path.

Suggested new stem lure-acquisition; own record
game-core/LURE-ACQUISITION-VALIDATION.md. Shared 8000 cases/strict guards, unchanged
reward lurediff.sh, temporary and existing acquisition/save suites, H and CI.
Unknown old timers stay a documented boundary. Stop after this PR.
```

## D12 — Lure expiry at the real BattleEnd boundary

Prerequisite: D11 merged. Existing game-core temporary-booster lapse is not
evidence that Lures lapse; both must keep their independent counters intact.

```text
Implement only D12 from docs/DEVIN-PARITY-BATCH-2026-10-09.md and shared contract.
Lane C; game-core/ only; one PR; upstream
8555c08c823b856cbec4eb99ca84ea52a955836d, Node 22. Require D11 merged and verify
pr_reward_lapse_lures's recorded proof. Search current BattleEnd lapse first.

Read BattleEndPhase.start, LapsingPersistentModifier.lapse, Scene.removeModifier/
updateModifiers; C gc_phases.c battle_end duplicate culling, enemy cleanup,
gc_temp_lapse, item compaction and save fields. Source runs one lapse per
matching modifier RECORD, not per stack, after enemy held cleanup. It removes
expired records while preserving survivors' order. One BattleEnd may follow
victory, flee or a nonvictory switch-out; trainer member KOs are not separate
completed battles. Do not move the clock to each turn, each faint, or next-wave
entry. Confirm exact queue/cleanup order in source before implementing.

Use the canonical reward lapse API with bounded game inventory conversion.
No-Lure stock is a no-op. Canonical timers decrement exactly once; count0 or
unrepresentable old records retain explicit legacy fallback, without invented
expiry. Keep the existing temporary-booster lapse and all other timers correct.

Source fixtures execute real BattleEnd and observe lapse/removal callbacks:
all three durations, count1/2/max, mixed stacks and duplicate records, mixed
X/Dire/held stock, victory/flee/nonvictory, queued duplicate BattleEnds, trainer
member KO control, enemy cleanup, expiration compaction. Pair with actual C
phase advancement. Compare stock/order/flags/timers and party/wallet/global/
battle RNG/queues/messages. Save before the boundary, load and execute once;
compare with uninterrupted execution, and prove no double lapse after resume.
Use legal game save boundaries; don't loosen save availability for the test.

Mutation: run lapse twice in the actual BattleEnd handler, or fail to remove
expired records. Suggested stem lure-lapse; own record
game-core/LURE-LAPSE-VALIDATION.md. Shared 8000 cases, strict trace guards, D11/raw
Lure/temporary/save regressions and H. Full post-battle abilities/Pay Day and
natural doubles are excluded. Stop after the PR.
```

## D13 — Feed Lure record count into encounter generation

Prerequisite: D12 merged. Current `gc_new_battle` hard-codes `ctx.n_lures = 0`.
The existing run-core API already applies the numerical chance formula; it is
read-only. **Double encounters still have a counted game-core execution gap.**

```text
Implement only D13 from docs/DEVIN-PARITY-BATCH-2026-10-09.md; read shared contract.
Lane C; game-core/ only, run-core read-only; one PR; upstream
8555c08c823b856cbec4eb99ca84ea52a955836d, Node 22. Require merged D12 and verify
current gc_new_battle still omits the canonical Lure input.

Read BattleScene.getDoubleBattleChance, checkIsDouble and their actual
newBattle callers; DoubleBattleChanceBoosterModifier.apply; actual seeded RNG
range conversion. Read C gc_encounter.c gc_new_battle, pr_new_battle_ctx.n_lures,
and run-core's pr_run_get_double_battle_chance/pr_run_new_battle read-only.
Source begins with denominator32 on wave multiples of10, else8, divides by4
per eligible modifier RECORD (not stack), applies existing ability effects and
clamps to minimum1. Existing run-core already performs these divisions. Feed
the validated count; do not pre-divide and then divide again or add a new roll.

Implement only canonical Lure inventory -> ctx.n_lures mapping, using D11/D12
validation. Preserve existing ability inputs, fixed/trainer/mystery gates and
RNG draw order. Handle mixed canonical/legacy/invalid Lure stock explicitly;
do not count opaque count0 records as a newly proved active item, and do not
erase its fallback warning. Do not change run-core or “fix” its behavior here.

Source proof executes the actual scene chance method AND an ordinary supported
newBattle chance decision. Balanced fixtures: no/one/two/three Lure records,
same record stack1/2, mixed kinds, partial timers, expiration via actual prior
BattleEnd, waves around multiples of10, denominator clamp and seeded zero/nonzero
outcomes. Record numeric denominator, actual input record count, decision, exact
RNG draws/state and unchanged inventory. C driver must exercise the real
gc_new_battle input/call (or an instrumented observer delegating that same call),
not only the read-only run helper. Stop before unrepresented enemy-generation
outputs and label that boundary. Pair save/reload before generation. If the
game caller cannot be observed safely without a cross-folder change, report
that blocker rather than substitute a helper-only claim.

Mutation: leave real ctx.n_lures at0, or sum stacks instead of records. Suggested
stem lure-encounter; own game-core/LURE-ENCOUNTER-VALIDATION.md. Shared 8000 source
cases/strict guards, D11/D12/raw Lure and encounter/save baselines, H and CI.
Keep GC_STUB_DOUBLE_AS_SINGLE and genuine unsupported double execution counted.
This is chance-input parity, not doubles or full Task64 completion. Stop here.
```

## D14 — Investigate the retained ITEMS text failures

This is a useful small **investigation**, not permission to fix an established
producer/comparator or add a production workaround for a test artifact.

```text
Do only D14 from docs/DEVIN-PARITY-BATCH-2026-10-09.md. Read shared contract.
Lane G; write docs/ only; all implementation/tests/upstream tracked source are
read-only. Upstream 8555c08c823b856cbec4eb99ca84ea52a955836d, Node 22. Deduplicate
existing root-cause reports. Deliver docs/ITEMS-TEXT-GAP-INVESTIGATION.md, one PR.

Read the current ledger, battle-core README and owning item validation records.
At this pack's baseline the established TURN_ITEMS source-state replay still
has strict text-completeness failures1/1/2/1 on seeds777/9001/4242/31337. Reproduce
the unchanged producer at N2000 per seed and replay with the unchanged C driver
using the README's actual commands. Producer is
battle-core/tests/turntest.producer.test.ts; comparator
battle-core/tests/turntest.c. Use separate external traces, Node22, locales and
the exact pin; record real SHA/versions/commands/exit codes/coverage.

For EVERY missing/untranslated text, report seed/case ID, actual source text
and queue context, C event/key/arguments/text, producing upstream symbol and
source line, C callsite, and the exact first mismatch. Trace the source locale
key/argument formatter, numeric/name/ability formatting and queue extraction;
do not infer cause solely from the visible text. Classify only after evidence:
production event difference, missing locale/asset setup, source transport gap,
or comparator/formatting defect. Keep unresolved causes explicitly unresolved.
Observe actual source methods via external diagnostics which delegate originals;
do not patch a committed producer or manufacture expected output from C.

Report all completed cases, covered state comparisons AND separate text-complete
counts. A state-only exit0 must not be described as a complete text pass. Include
a minimal reproduction for each distinct cause, the smallest proposed future
repair and its owning lane, and a regression fixture it would need. If current
main already repairs the gap, prove that and identify its commit instead of
recreating the historical failure. No code/test repair, unsupported waiver,
regex ignore, reclassification or blanket ledger completion in this task.

Validation: 4x2000 unchanged ITEMS source requests, exact failing case inventory,
source/C callsite citations, reproducible commands, clean implementation diff.
H is not needed for this docs-only investigation. Missing environment/source
evidence means BLOCKED with commands unrun; no invented case IDs/results. Stop.
```

## D15 — Prove optional trainer queue metadata separately

Existing runtime trainer routing is already proved by #185. This card targets
the documented `undefined` versus integer `-1` transport boundary, not another
trainer switching implementation. The old failed comparator remains intact.

```text
Do only D15 from docs/DEVIN-PARITY-BATCH-2026-10-09.md and the shared contract.
Lane A; battle-core/ only; additive tests/tools/validation record, NO production
or established producer/comparator changes. One PR; upstream
8555c08c823b856cbec4eb99ca84ea52a955836d, Node 22. Read
battle-core/TRAINER-SWITCH-VALIDATION.md and verify current optional-metadata gap.

Reproduce unchanged TURN_TRAINER N2000 seed4242 CASE1051 if still present. Inspect
tests/turntest.producer.test.ts queue serialization (map(ph.battlerIndex).join),
tests/turntest.c, upstream SwitchSummonPhase/Phase/PokemonPhase, the actual
pending source queue object and C engine phase metadata. Known baseline:
source non-Pokemon phase has no battlerIndex and the join prints an empty field;
the old C integer transport prints -1. State/RNG/text otherwise matched in the
recorded case. Verify this on current main; do not assume every empty field is
-1, null or undefined, or assign a fake runtime index to match the test string.

Create a separately named strict source probe/replay which records the actual
phase type, queue level/order and optional field presence/type/value. Distinguish
absent, undefined, null and integer explicitly when observed; reject malformed
or fractional integers. Derive C non-Pokemon metadata semantics from its real
phase type/engine contract with source citations, not from source expected
values. If C cannot independently express the absence, deliver a precise
representation blocker instead of an always-passing comparison.

Require the reproduced failure plus deterministic source fixtures for a pending
non-Pokemon SwitchSummonPhase, normal indexed Pokemon phases and genuine -1
integer metadata where the source actually produces it. Do not fabricate a
source queue by editing the expected string. Validate full queue order/phase
kind and optional metadata; preserve state/RNG checks as controls. Mutation:
force absent metadata to present integer-1 in the new independent C mapping,
and prove rejection. Truncated/wrong-count/wrong-presence traces must fail.

Record actual commands and fixtures in
battle-core/OPTIONAL-PHASE-METADATA-VALIDATION.md. The new harness stem is proposed
optional-phase, not an existing command. Replay the unchanged broad trainer
suite on all four seeds at N2000 and the unchanged dedicated trainerswitchdiff.sh
suite at N2000/four seeds, reporting the original FAILED result separately.
No “whole trainer suite fixed” claim; no weakening of the old failure and no
production changes. H is optional for this test-only task; required CI still
applies. Stop after this PR or documented blocker.
```

## Explicitly deferred work

Do not replace a blocked card with these larger tasks: final-boss phase
transition/Classic clears, complete enemy movesets/fixed teams, generic
Moxie/Beast Boost payload decoding, Fetch Ball's cross-core ball-stock event,
Transform/Parental Bond/Harvest, egg rehydration/migration/hatching, full item
transfers, natural doubles, frontend/audio, mystery-event parity or hardware
acceptance. They need broader contracts or owner coordination. D01–D15 make
bounded progress without pretending that those project gates are resolved.
