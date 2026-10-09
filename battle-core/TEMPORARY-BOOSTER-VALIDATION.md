# Temporary X / Dire Hit full-turn proof (Card 104, partial)

Target: upstream `8555c08c823b856cbec4eb99ca84ea52a955836d`, Node22.
The six TempStatStageBoosterModifier factories construct stat args ATK..ACC,
boost .2 for ordinary stats and 1 for ACC, and duration5. DIRE_HIT constructs
TempCritBoosterModifier. These are side-wide lapsing persistent modifiers,
not PokemonHeldItemModifier subclasses. Game-core separately validates named
remaining duration1..5; battle-core has no invented duration field.

The existing source helpers and actual C callers already implement the math.
This change admits only known canonical records: real stack1..2, virtual0,
owner0, exact stat boost/arg, or Dire arg0. Unknown subclasses, unsupported
stacks/virtual stock, held owners, and malformed args remain unported.
No raw arithmetic, caller formula, RNG or timer implementation changes.
Mixed valid and malformed/unknown full-turn contexts are explicitly checked.

Source application is **once per record**, independent of real stack count.
Separate matching records each add their boost. The producer constructs real
factory items and probes identical source inputs to prove stack2 equals stack1
and duplicate records contribute separately; C guards independently check the
same contract. Ordinary stat multipliers clamp at4; accuracy user stage clamps
at6 before the ratio. Ignored stat stages return1 before applying Temp items.
Critical stat handling uses the actual opponent context.

## Additive source evidence

`tests/temporary-turn.producer.test.ts` runs real MoveEffectPhase and modifier
applications, with observing wrappers that delegate the original source methods.
`tests/temporary-turn-replay.inc` retains the established state/turn comparison
and adds strict Temp transport and fresh numerical snapshots: real effective
stats, ignored-stage Sacred Sword contexts, critical contexts, and raw ACC/Dire
stage projections. ACC snapshots supplement actual low-accuracy hit/miss turns;
they do not replace comparison of the real accuracy caller's outcomes.

`tools/tempturndiff.sh <upstream> 2000 <seed>` requires the exact pin and Node22.
Four seeds777/9001/4242/31337 each produce2000 requested, completed and covered
turns, with zero parse/stat/state/RNG/text differences and no unported turns.
Each includes both active sides, side-wide bench snapshots, stack1/2, duplicate
records, opposite-side stock, positive/negative/max stages, actual stat damage,
SPD ordering, low-accuracy moves and Dire/high-critical Slash. Sonic Boom fixed
damage and Splash status contexts are included; blocked-critical abilities,
doubles, fusion and broader lifecycle are not claimed. All abilities in these
fixtures are Ball Fetch.

There are105 canonical/refusal/raw numerical guard checks and12 mixed full-turn
admission checks. Strict request/completion matching rejects missing, wrong or
fractional footers/headers, trailing cases, fractional/zero stack, fractional
owner, virtual stock, wrong stat/boost and unknown class (13 malformed traces,
exit2). Fractional producer N is rejected rather than rounded. The wrapper
requires every requested turn and every text comparison to pass.

Actual C caller mutations are run against unchanged source fixtures: removing
the stat caller causes1388 numerical input mismatches, recorded under the
parse counter (612 covered/exact, wrapper exit1); removing the ACC caller
causes40 turn/text mismatches; removing ACC clamp causes5; removing the Dire
caller causes107 turn mismatches and97 text differences. Actual stat-clamp and ignored-stage mutations cause140 and2000 numerical
input mismatches respectively, also recorded under the parse counter. All failures propagate through
the owning wrapper, rather than merely printing diagnostics.

## Regression evidence and approved contract update

All64 established battle baseline/final replay outputs are byte-identical:
48 prior turn/direct-damage pairs, weather4, arena4, weather-form4 and Mystical
Rock4. Legacy direct damage retains2000/2000/2000/1999 covered cases with zero
stat mismatches; the last seed retains its existing one upstream thrown case.
No established producer, trace, comparator, tolerance or exclusion changed.

The user explicitly authorized the narrow stronger contract update in
`game-core/tests/temp_caller_test.c`: replace its former canonical unsupported
assertion with positive admission/class/stock/owner and actual numerical helper
outcomes, while adding admission refusal to existing invalid cases. Source
caller cases/paid/full/BattleEnd/results remain unchanged. Checks increase by
6000 per seed:107059→113059,105641→111641,105718→111718,107224→113224. Game Temp
host guards increase90→98; raw Temp25 remain unchanged. The other36 prior game
replay outputs and eight Wide raw/acquisition replays are byte-identical.
All retained source fixture bytes and source producers remain unchanged.

`make -C game-core test` passes. Paired1000 normal and save runs, jobs8:
1000 ended, zero stuck/crashed/timed out,50394 turns,14679 round trips, largest
save11472 bytes. Every stand-in, phase/move/ability count and outcome is exact.
Only two rows change: digest30c8d270→5b6c9ee2 and modifier reports982 runs/52572
firings→924/46316, a reduction of6256 reports from canonical admission. The
smaller host save test also preserves its normal/save digest equality
(before74adeffb, afterbc0fbf1c). Source evidence, host/3DS CI and hardware proof
are separate: this suite does not claim whole-run parity or hardware execution.

Evidence: `/workspace/validation/lane-a/five/temp`. The earlier Temp deferral
recorded in FIELD-EFFECT-VALIDATION remains historical: it preceded explicit
user authorization for this dedicated proof and stronger contract replacement.
