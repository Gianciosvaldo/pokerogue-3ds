# D13 canonical Lure encounter input

D12 #214 merged eaef939194f6ab1fb91cf7d83f3e48ffd8892851; lane C only.
Upstream8555c08c823b856cbec4eb99ca84ea52a955836d, Node22.23.3.
Evidence before production edits:

|Source symbol|C behavior|Fixture|
|---|---|---|
|BattleScene.getDoubleBattleChance battle-scene.ts1233–1241|count canonical records, preserve existing ability input, run-core divides once|0/1/2/3 records, stack1/2, mixed durations, partial timers|
|DoubleBattleChanceBoosterModifier.apply modifier.ts|divide per matching record, not stack|same record stack1/2, denominator clamp|
|BattleScene.newBattle1258/checkIsDouble1484–1510/resetSeed2010|delegate real caller, compare seeded decision before Battle constructor|waves9/10/11/19/20/21/49/50/51, boss gate|
|utils/common.ts102 randSeedInt; Phaser integerInRange|range<=1 consumes no draw, otherwise two raw rnd calls|exact pre/post RNG and raw values|
|BattleEndPhase.start and merged gc_lure_lapse|expired records disappear before chance projection|actual prior boundary on both source/C copies|
|gc_new_battle->pr_run_new_battle; run-core/newbattle.c67–131, wave.c234|feed count only, keep gates/draw order/GC_STUB_DOUBLE_AS_SINGLE|delegating linker observer, stop before enemy-generation outputs|
|gc_run_set_starters/can_save/save/load|unchanged legal encounter save, before supplied prior-boundary/generation inputs|uninterrupted/resumed caller comparison|

Read-only run-core/reward-core. Invalid/mixed legacy projection refuses atomically;
caller keeps ctx0 fallback and counts GC_STUB_REWARD_INPUT. No doubles execution,
trainer generation, full Task64, hardware or Classic-clear claim. Final executed
proof and required exact-head CI to be recorded before merge.

## Executed proof

`source /workspace/shared/activate-parity.sh`; `game-core/tools/lure-encounter-diff.sh
/workspace/shared/upstream-lane-c 2000 '777 9001 4242 31337'`. All8000 source
cases complete, zero skipped/thrown, exact C replay on uninterrupted and saved/
loaded copies. Source executes actual getDoubleBattleChance and newBattle,
observer delegates checkIsDouble then stops with a unique sentinel before
Battle constructor/enemy generation. Every other exception fails the producer.
C invokes actual gc_new_battle and delegates actual pr_run_new_battle; observer
stops after that call, before the game caller's enemy-generation outputs.
This is chance-input/decision parity, not a complete encounter-generation proof.

Per seed active record counts0/1/2/3:583/501/499/417. Input fixtures have500 each
before prior expiry; actual BattleEnd removes250 records per seed. Every count
uses stacks1/2 and mixed Lure/Super/Max, partial1..max lifetimes; waves9/10/11,
19/20/21,49/50/51.216 actual endless-boss no-double gates,2288 actual raw RNG draws
per seed, including no-draw clamped denominators. Source doubles989/987/958/942
and singles1011/1013/1042/1058. Denominator, count, zero/nonzero decision, global
RNG before/after and both raw values match. No extra chance roll or pre-division.
The reward API is used only to validate the canonical shadow projection; its
unused result does not enter the run-core numerical input. Existing ability
input is unchanged (0); fixtures have no double-chance abilities. Fixed/trainer/
mystery branches are read-only and unchanged, not newly covered generation.

All54 Pokemon fields, wallet and stock/order/flags/timers are unchanged by the
observed generation boundary and match source. Source's current Battle remains
the previous battle because of the pre-constructor stop, so no new battle RNG
is claimed; C prepared battle RNG remains unchanged at the observed boundary.
Other queue/event output is empty there; preceding real BattleEnd stock and
unchanged global RNG are compared separately. Source gameStats/presentation
bookkeeping and complete profile/session construction remain outside this proof.

8000 C saves use the untouched real encounter COMMAND boundary after actual
set_starters, with supplied Pokemon/stock/previous-wave generation inputs.
No queue/save flag/screen is rebuilt to enable saving. Save happens BEFORE
optional actual prior BattleEnd and next-generation invocation. Both copies
receive that boundary once, when requested, then the same real encounter caller.
Load display events are explicitly asserted/drained before the compared output;
the supplied participant control is explicit on each copy (not a claim that
session participant rehydration is absent). Inventory expiry/chance/draws match
uninterrupted execution. The discarded old enemy generation is unrepresented.

43 host guards pass: atomic legacy timer0, out-of-range timer/stack/arg/owner,
virtual/detached/challenge/unknown projection/capacity and mixed canonical/legacy
refusal; unknown no-Lure stock is an unchanged opaque0-count. Actual delegated
caller refusal retains inventory, feeds0 and increments GC_STUB_REWARD_INPUT.
An uninstrumented real encounter additionally proves GC_STUB_DOUBLE_AS_SINGLE
still increments and produces the existing single stand-in; no doubles claim.
51 malformed transports exit2 after intact0, including missing/duplicate records,
unknown class, invalid count/stack/decision/draws/fractional Pokemon ID, strict
header/footer/order. Rebuilt real caller mutants: missing count1417 failures,
summing stacks709 failures, exit1 each; intact2000/2000 exit0. Final four-seed
replay after observer/parser hardening passes8000 again. Established producers/
comparators and all other core folders are unchanged.

Existing D11/D12/raw Lure each8000 replay unchanged; raw26 guards pass.
Acquisition/candy/apply/ball/Bell/Orb/Wide/Multi retained regressions all pass.
Those regressions reuse pinned retained traces, rather than claiming fresh
source generation. Initial D11 replay without GC_LOCALES refused2; all four
were rerun with the required locale setup and pass.
H: make -j2 -C game-core test build/gcplay passes, host pair019412a9;
1000 normal/save ended each,0 clears/stuck/crashes/timeouts,49966turns,
matching874370f1,14629roundtrips,largest11472bytes. Before:50236turns/0414bcf0,
14675roundtrips. Chance inputs legitimately change later run trajectories.

Retained setup corrections: first120 replay had60 differences because only the
run RNG was set while actual BattleEnd copies the battle RNG back; the supplied
initial state now sets both actual RNGs, not expected output. Initial strengthened
source54 fields exposed undefined framework passive/pause flags as NaN and all
four replays refused2; final fixtures explicitly set validfalse inputs and all
8000 were freshly generated/completed. Fixture stacks were independently varied
across all record counts; source values were not changed to match C. Delegating
setjmp observers are isolated from local loop variables to avoid clobber risks.

Evidence: ignored game-core/build/lure-encounter-evidence.ZFVpvk/ and
/workspace/work/remaining-2026-10-11/d13-*. Reproduce malformed/mutations with the
new owning scripts and caller-2000-777.txt; raw/regression paths are recorded in
D11/D12. Broader trainer4242case1051 and ITEMS1/1/2/1 remain FAILED; legacy damage
31337 coverage1999, no Classic clears or hardware run. Exact-head host/3DS and
other applicable CI must pass before merge; conditional source skips aren't passes.

|Evidence|SHA256|
|---|---|
|lure-encounter.producer.test.ts|`830bc8859f8ea07b1dee973365e80012e703d6b7d414b7399f2645897910fe00`|
|caller-2000-31337.txt|`10c39da02238cc82521dde81d9f34f952fa0dedb82a5737dd105692a0490c481`|
|caller-2000-4242.txt|`3a5bcd399d356dc96989f10ee7547ed7b05b4aa47b7f94e3bf0948cfb9b34bb9`|
|caller-2000-777.txt|`15263fb3a292fcf8fb9fdd5bcb9f3a4c40b49fc8b9a7491cbc555c806130fd1f`|
|caller-2000-9001.txt|`e3205e66d3fc722c6a37ebb97e4cb7109d70e5c514d64bf0f315bc29c6c7de5e`|
