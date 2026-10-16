# Commander, CommandedTag and Order Up

Pinned source `8555c08c823b856cbec4eb99ca84ea52a955836d`, locales
`270ed0a2938b2c3e2e34e7de940e84aecadf548b`, Node 22.23.3. Tracked upstream,
previous producers/comparators/coverage contracts and run-core are unchanged.
Normal app/custom/save tests and 3DS CI remain required; only the dedicated
custom-only Task63 check is skipped.

## Behavior and callers

CommanderAbAttr checks doubles, the actual getAlly object, active on-field ally,
raw Dondozo species and absence of CommandedTag. Normal ability eligibility,
passives, suppression, Gas and simulation stay in the shared dispatcher. When
not simulated it lapses the user's MOVE_EFFECT tags, queues the cached-user
Commander animation, adds CommandedTag to the ally and removes one queued move
whose cached Pokemon object matches the user. It preserves the source's uncopiable
registration and known upstream non-TURN_END lapse edge case.

PriorityQueue.remove reorders before finding the first match. The C removal
helper now does the same, including timing, priority and bracket order, then
checks raw tree phases if no dynamic match exists. It keeps the empty marker,
does not append last-turn order and updates the queued-mover bridge. Cached
object matching remains distinct from a changed field-slot lookup.

The owning PostSummonPhase now uses the real dispatcher in getAlliesGenerator's
speed order, replacing the old placeholder. The Reviver Seed callback loop uses
the same real dispatcher. Its ordinary fainted-holder path still queues healing
before the holder is alive, as in the pin; this change does not move revival ahead
of the callbacks.

CommandedTag constructs with CUSTOM lapse, turn count zero, no move and the given
source ID. On add it caches the source's raw getFormKey (default curly if absent)
and queues +2 ATK/DEF/SPATK/SPDEF/SPD from the holder. Base custom lapse removes it;
on removal an active source causes a cached-holder Commander removal animation.
The cache stores an interned raw form-table index plus one, with zero representing
the absent-source curly default. The exact key is retained, including the empty
key; it is not reduced to just the three recognized boost choices.

OrderUpStatBoostAttr's override omits parent HP/condition/chance gates. It reads
the cached key: droopy -> DEF, stretchy -> SPD, curly/other -> ATK, then queues +1
from the user. A later source form change does not change that choice.

The new save field is explicit in the named tag schema. The existing format and
older save migration remain; an index beyond the pinned form table is rejected.
No later-turn saving capability is introduced.

## Native proof

`tools/commander-diff.sh UPSTREAM` runs the additive producer and replay. It
captures native battle RNG and native PhaseManager.shiftPhase before constructing
GameManager, restores battle RNG after natural doubles startup, and calls actual
registered attributes, Pokemon tag methods, PostSummonPhase and the native phase
controller. It stops at native TurnInit, including the source's transition when
an empty dynamic marker is reached. It does not skip empty markers to execute
later queued phases.

The final matrix has 16,960 complete cases: 16,384 base API/tag/Order Up cases,
512 PostSummon controls and 64 additional positive one-HP user controls. The
preceding complete 16,896-case capture is also retained separately. No original
base cases were removed. Startup-generated actor IDs can differ between runs;
replays compare each capture's actual IDs without normalization.

Contexts cover twelve Rattata/Tatsugiri/Dondozo actors, both sides, party lengths
2..6, singles/doubles, raw reserve callers, all three Tatsugiri forms, missing
sources, later source form changes, capped/partial/negative stat stages,
Simple/Contrary, primary/passive Commander, suppression/Gas, simulation, HP zero
and one, switching and physically absent actors, semi-invulnerable tags, Trick
Room, duplicate queued movers, timing/priority and changed field lookup indexes.
PostSummon cases include two Dondozo with assigned Commander to distinguish speed
order. Those assigned-ability and raw queue states are augmented contexts, not a
claim that every context arises naturally from ordinary species abilities.

The replay compares the established 125-field actor projection plus physical
field presence and exact commanded key/source/count/semi-tag metadata before the
leaf, after it, after cached-form Order Up, after lapse and after execution.
It compares constructor queues, levels, dynamic order, text, cached Pokemon and
stat sources, requested changes/options, move timing, trapping, world presentation
state and both RNG streams. Remaining queued moves are explicitly removed using
the actual source API before the execution portion; cancellation is observed
before those additional removals. Actual stat and ability phases execute. Asset,
info, frame-rate, animation, phase-start presentation and UI boundaries are mocked;
PokemonAnim is observed as a constructor and treated as presentation during drain.
The actor projection omits abilityRevealed AI/UI metadata. Source effect booleans
are recorded, but C's void effect API does not establish boolean-return parity.

The matrix exposed a shared sorting prerequisite: one consecutive group never
invokes native Array.sort's Speed comparator, including a run of phases whose
getPokemon() is undefined. C previously rejected those valid no-op stat phases.
It now preserves that case; mixed real/missing getters and distinct null/undefined
groups remain explicit native-exception boundaries. This is not blanket support
for arbitrary missing Pokemon.

Preliminary captures with an invalid timing enum or a manual drain that skipped
empty markers are diagnostic only. The final proof uses valid Last/Normal/First
values and native shiftPhase. A redundant positive-HP check is not a distinct
Commander fault: normal ability eligibility already excludes fainted users for
this registered ability. The added one-HP controls test a genuinely stronger gate.
The Commander's own predicate remains faithful to source, and Order Up's distinct
raw fainted-user behavior remains covered.

The runtime guard catches 29 compiled faults, including wrong owning dispatch and
speed order, cached-object cancellation, simulation side effects, loss of cached
form, a stronger user HP gate, tag lifecycle and undefined-group rejection. The
simulation fault crashes on a valid simulated input and is recorded as caught;
other runtime faults return a mismatch. Two separate save faults are caught.
Thirty-four malformed complete captures are rejected, including integer overflow,
nonfinite fields, physical field flags, invalid timing and footer errors.

## Integration and remaining scope

The host test installs augmented CommandedTag state at the supported first-command
save point, executes the initial boosts, changes the source form, saves/loads and
runs actual Order Up commands. It checks all three cached choices, HP/stages and
three RNG states. It also rejects saves with a valid checksum but invalid cache
index. All 69 host checks pass. The full game-core suite passes and the paired 200-run
saved/unsaved digest remains 5e19bd8a. This is singles command/save evidence, not a
natural doubles save proof.

Retained broad doubles remain 1,962 / 1,949 / 1,965 exact with every previously
covered ID retained; 124 inputs remain explicitly unsupported. Mixed captures
retain 1,976 / 1,970 / 1,991 / 1,995 exact IDs. Partner healing 12,800, attention
16,768, all prior API/numeric proofs and 8,194 Acupressure owning turns pass.
Four unchanged Reviver Seed owning captures also retain all 8,000 exact turns
and their exact text. These bounded counts are not completion percentages.

The complete source inventory, fusion/Transform interactions, full natural
Commander/faint/switch/revival arrangements and wide-level progression/combat
still require their final audits. This bounded matrix does not establish whole
battle-core completion.
