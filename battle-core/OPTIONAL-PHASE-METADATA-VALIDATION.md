# D15 optional phase metadata, separately typed

**Follow-up:** the original legacy comparator now uses the independently proven presence classification, fixing trainer CASE 1051 while retaining genuine negative indices. The typed probe, malformed guards and mutations still pass; see [current repair](RETAINED-FAILURES-REPAIR.md). Statements below about unchanged established tests describe the original D15 snapshot.

Base9e819df6ed9f3837b3a30edc49248edf0603b636 (D13 #215 merged).
Lane A additive tests/tools only. Production and established producers/comparators
are read-only. Pin8555c08c823b856cbec4eb99ca84ea52a955836d, Node22.23.3.

|Source/C symbol|Independent transport rule|Fixture|
|---|---|---|
|SwitchSummonPhase26->SummonPhase16->PartyMemberPokemonPhase; Phase/PokemonPhase constructors|SwitchSummon does not declare/inherit battlerIndex; absent A is distinct from U/N/I-1|actual pending broad4242CASE1051 and deterministic constructors|
|C engine.c260 phase_pokemon special PH_SWITCH_SUMMON branch|uses sw_player/sw_field_index/party; integer placeholder is not a source index|new test mapping from phase type, original integer transport unchanged|
|PokemonPhase constructor/getPokemon, CommonAnimPhase/PokemonHealPhase/PostSummonPhase|owned integer field, including genuine -1; undefined constructor input falls back|actual normal indexed0/2 constructors and nested ability phases|
|PlayerPokemon.getFieldIndex/getBattlerIndex pokemon.ts5995–6000; BerryPhase.eatBerries, berry.ts79|off-field party member naturally returns integer-1 and real berry callback queues it|actual off-field Sitrus helper queues CommonAnim/PokemonHeal I-1|
|pr_battler_index, pr_unshift_common_anim, pr_apply_berry->pr_unshift_heal_msg|independently derive off-field-1, use real runtime queue helpers|matching C bounded helper boundary, not a legal off-field whole Berry turn|
|MoveHeaderPhase/TeraPhase Pokemon reference; DancerPhase params.pokemon|dynamic membership does not imply a battlerIndex property|audited absent classification, no blanket -1 conversion|
|turntest source queueLines and C dump_trace_queue|preserve complete level/kind/order, compare separately typed dynamic metadata|anchored additive build overlays, full original state/RNG/text controls|

Final owning checks and exact-head CI required before merge. Original CASE1051
failure remains FAILED. The new record is not whole trainer-turn parity.

## Executed source and runtime checks

Fresh unchanged TURN_TRAINER N2000 at777/9001/4242/31337 completes all requested
cases (source footer has no throws), using the README command and pinned English
locales. Current baseline:

|Seed|Covered/pass/fail|Uncovered|Covered texts identical/missing|Exit|
|---|---|---|---|---|
|777|1946/1946/0|54|1946/0|0|
|9001|1943/1943/0|57|1943/0|0|
|4242|1945/1944/1|55|1945/0|1 FAILED|
|31337|1953/1953/0|47|1953/0|0|

Seed4242 CASE1051 retains the exact first mismatch:
C `dyn=SwitchSummonPhase:-1`, source `dyn=SwitchSummonPhase:`.
Full queue levels/kind/order/deferred flag match; state/RNG/trace/text match.
The pending queue is CheckInterlude,WeatherEffect,PositionalTag,Berry,
CheckStatusEffect,TurnEnd,Marker:SwitchSummon in level0, MoveEnd in level1,
empty levels2/3; stop GameOverPhase. Source serializer's map/join loses field
presence; it does not prove that the source object stores -1. New source probe
reads the actual object: absence is `!("battlerIndex" in ph)`, distinct from a
present undefined/null value. Case1051 records `SwitchSummonPhase:A`.

New `tools/optional-phase-diff.sh /workspace/shared/upstream-lane-a 2000 4242`
checks pin/Node22/request bounds; source build overlay is separately named and
rejects any nonempty original thrown-case dictionary. All2000 source cases
complete; C compares1945 covered turns with zero state/RNG/text/typed-dynamic-
queue differences, texts1945/1945, no parser/stat mismatches. Existing55
unrepresented turns remain excluded, never counted as passes. Source full queue
level/kind/order/deferred records remain the original representation; dynamic
optional metadata is independently typed. No claim that every unrelated phase's
optional fields or all2000 full turns are newly covered.

The C test-only mapping derives A from the actual phase enum/addressing contract,
not an expected source token or integer sign. PH_SWITCH_SUMMON's integer -1
placeholder is unused for PartyMemberPokemonPhase routing. Indexed phases retain
I0/I2/I-1; -1 is never globally turned into absence. Audited reference-based
MoveHeader/Tera/Dancer types are separately absent. No production indices,
phase routing or old comparator change. New C replay is generated into ignored
build/ by one checked dynamic-loop overlay of the unchanged turntest.c. State,
RNG, trace, text, exclusions and tolerances are retained unchanged.

Six deterministic source cases execute actual PhaseManager constructors/queue
insertion: pending SwitchSummon, indexed PostSummon0/2 (including actual nested
ability-activation phases), a genuine off-field Sitrus holder, and combined
queues. Actual BerryPhase.eatBerries invokes CommonAnim/getBerryEffectFunc;
PlayerPokemon.getBattlerIndex returns -1 from playerField.indexOf(bench), so
CommonAnim and PokemonHeal actually carry integer-1. This is a directly supplied
helper boundary; it does not execute the pending off-field healing phase or
claim a legal off-field whole BerryPhase turn. C independently obtains -1 from
pr_battler_index and calls real queue helpers/pr_apply_berry. Both preserve HP,
stats/wallet/global and battle RNG, and consume the held berry as expected.
No unported/deferred C effects or queue overflow are accepted by that probe.

All6 cases match complete queue node kind/order, level indices/count (including
empty levels), deferred flag, dynamic group/order and field presence/type/value.
Actual observations:16 absent nodes (including markers),8 indexed0/2 nodes,
4 genuine integer-1 nodes. A/U/N/I are distinct transport tokens. No actual U/N
object occurs in these bounded fixtures; negative well-typed U/N records fail
comparison rather than being coerced to A or -1. This does not establish a
universal nullable phase runtime representation.

46 malformed traces exit2 after both intact controls0, including truncated/
missing footer, count/version/fractional request, case/order, missing/duplicate
records, empty/duplicate dynamic groups, invalid class/presence/fractional index,
level count/deferred shape.7 validly typed semantic corruptions exit1: absent->
I-1/U/N and genuine I-1->A. Source expectations are unchanged for positive runs.
Rebuilt independent C mapping mutation removes PH_SWITCH_SUMMON from the absent
classification. Intact typed/fixture drivers exit0; mutant exits1 in both,
reproducing CASE1051's single covered failure and3/6 deterministic failures.
No compile error or skipped turn is used as mutation success.

Unchanged trainerswitchdiff.sh is freshly rerun at2000/four seeds: all8000
covered/state/RNG/text comparisons pass,0 uncovered, and the existing missing-
callback guard passes. Original trainer failure above remains FAILED, with
current coverage recorded rather than substituted historical counts. ITEMS
text1/1/2/1 and legacy damage31337coverage1999 also remain explicit; no whole
trainer suite, full ledger, natural doubles, Classic clear or hardware claim.
H is optional for this additive test-only card; no production was changed.
Required exact-head host/3DS and applicable CI must pass before merge.

Retained setup corrections: nonexistent resetBattleData was replaced with the
actual resetBattleAndWaveData; source bench statistics are calculated BEFORE
setting the HP control because actual updateModifiers legitimately recalculates
them. C draft used nonexistent sw_slot_index/stack names; corrected to the read
header's sw_slot/stack_count. Zeroed draft engine omitted its required initial
level; initialized to the real engine_reset contract (one level,current-1,
command targets-1) before queue helpers, then all6 matched. A request guard
initially redeclared seed and the source transform failed; renamed requestSeed
and regenerated/completed the full2000. Original thrown dictionary is checked
by Object.keys, not array length. Initial malformed invocation used a wrong
scratch trace path and refused the intact control; corrected to the actual
wrapper evidence directory and all53 corruptions completed. A diagnostic fresh
old/new whole-trace byte comparison was false because initial run participant
IDs vary; actual source/C state controls still match, and no IDs/expectations were
normalized. No failed run or missing case was waived.

Reproduction: source activate-parity.sh; copy unchanged turntest.producer.test.ts
into upstream test/tests/zz-dump; run TURN_TRAINER=1 TURN_N=2000 TURN_SEED=<seed>
TURN_OUT=<trace> node node_modules/vitest/vitest.mjs run that file --maxWorkers=1;
TURNTEST_LOCALES=<upstream>/locales/en build/turntest <trace>. For dedicated
baseline use tools/trainerswitchdiff.sh UP 2000 SEED. Owning wrapper above;
malformed script takes typedEXE,typedTRACE,fixtureEXE,fixtureTRACE; mutation
script takes typedTRACE,fixtureTRACE, with TURNTEST_LOCALES set in both.
Evidence: ignored battle-core/build/optional-phase-evidence.Wt9t0P/ and
/workspace/work/remaining-2026-10-11/d15-*. GCC14.2.0/Node22.23.3.
Production and old producer/comparator hashes are verified unchanged against base.

## Batch card inventory

The bounded pack's first14 cards are merged. This final PR supplies D15's
separate representation proof, preserving the old failure. D14 was explicitly
an investigation; the proposed future ITEMS message repair remains separate.

|Card|Merged owning PR(s)|
|---|---|
|D01 Psych Up/repair194|#194/#195|
|D02 Power/Guard Split|#197|
|D03 Camouflage|#199|
|D04 party status cures|#200/#202|
|D05 Fell Stinger|#203|
|D06 Bad Dreams|#204|
|D07 Cheek Pouch|#205|
|D08 Toxic/Flame Orb acquisition|#198|
|D09 Soothe Bell acquisition|#201|
|D10 battle friendship|#206|
|D11 Lure acquisition|#208|
|D12 Lure expiry|#214|
|D13 encounter chance input|#215|
|D14 ITEMS text investigation|#196|

|Evidence|SHA256|
|---|---|
|turntest.producer.test.ts|`625ff61d15b22dc6410eef6343369498041231edbf06a36abf23667c590a52c0`|
|turntest.c|`5663fe8d0d37acc4b90de0b299ba38d22122ac83364a46d65c2deba62a4f381f`|
|engine.c|`dfa476af9af6c78b6a170d9c846aa8067c3b116e6f8df52d2c8f4683c022dee2`|
|phases.c|`0ae1a4f888fb44471261a84e9189241b99fa889ab7fca70e326d5825c01cd036`|
|optional-phase-fixtures.producer.test.ts|`14b641ea251ec4dfa04b7560fa28ef969247f3719cccd45293e96fe0a5376362`|
|build-optional-phase-source.py|`d928389321ba4a97e0fce97c6230dd3d2162a941df50edc5b792a716b630079b`|
|typed-2000-4242.txt|`785a5b18e5b08d79c8f4a15e801e56588385f708db709b09e69a0fdb44a4d51b`|
|fixtures.txt|`7983a7db92f8c231642c3f24f7ca1ac3f296bcf0bc2d3e59bdad0f9afb025fc3`|
