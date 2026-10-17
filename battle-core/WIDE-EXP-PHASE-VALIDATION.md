# Wide EXP award phases and display events

Pinned upstream `8555c08c823b856cbec4eb99ca84ea52a955836d`, locales
`270ed0a2938b2c3e2e34e7de940e84aecadf548b`, Node22.23.3.

ExpPhase and ShowPartyExpBarPhase apply ExpBoosterModifiers in inventory order,
floor the result, call the actual PlayerPokemon.addExp and queue LevelUpPhase
with the old/new levels. ExpPhase interpolates that full numeric award in its
message. The previous port narrowed message/event gains to int32. The app also
subtracted nominal queued gains to reconstruct old EXP, which fails when the
level cap discarded some of the award.

The phase now records the exact double award and pre-award EXP in runtime
GC_EV_EXP fields. The app uses the first pending event for that party member to
recover its prior EXP/level. The legacy b field mirrors gains only when int32
can represent them, otherwise zero; current playback reads the wide fields.
These runtime events are not saved; no save schema changes are introduced.

A new integer message argument stores low/high words in the existing payload,
without enlarging pr_msg_arg or borrowing temporary string storage. Numeric
formatting and plural selection read the complete signed64-bit integer. Source
Number interpolation is asserted only for safe-integer awards0..2^53-1. Invalid,
negative or larger raw awards remain explicitly unsupported with
PR_REWARD_UNPORTED_LEVEL before the phase mutates state. General Number shortest
formatting above that domain remains open; this is not full Endless completion.

## Evidence

The producer executes actual native ExpPhase/ShowPartyExpBarPhase start methods.
Native BattleScene RNG is captured before GameManager and restored after start.
A default observational spy delegates addExp and records its actual argument;
no copied booster formula supplies that expectation. Only UI text transport,
EXP animation/info update and phase transport are intercepted. The off-field
notification setting is the actual SKIP option. Level-up requests are compared
before executing learning/evolution, which remain outside this proof.

- Seeds777/2034: all16,640 final cases exact, zero source exceptions.
- Final protocol v3 adds128 unboosted fractional controls to the original8,192
  matrix. Each final seed has8,320 cases with both phase kinds, all six growth
  rates, three game modes, waves1..100,000, levels through1,000,000 and around
  native caps, zero/small/32-bit-boundary/2^40/MAX_SAFE_INTEGER awards, all three
  charm families and reversed inventory order. Large-award inputs start near
  their cap to bound native addExp's loop; this is not natural encounter proof.
- The original fractional inputs always passed through a booster first, making
  the final-floor mutation ineffective. The128 appended controls expose it;
  the original cases and all comparisons remain. Earlier8,192-case captures
  remain accepted/replayable. A preliminary v1 capture calculated a supplemental
  award field; v2/v3 instead observe the native addExp argument directly.
- Exact checks include actual award, resulting EXP/level, exact English text,
  LevelUp requests, both RNG states, runtime event fields and the production app
  helper's pre-award state amid multiple pending/unrelated events.
- Ten compiled runtime faults caught and seventeen malformed captures rejected.
  Eleven normal host checks cover full signed integer text and explicit refusal
  of invalid/out-of-domain raw awards without partial mutation.
- Retained12,288 native EXP/candy progression cases and204 native modifier/save
  reconstruction cases pass. Broad/mixed covered IDs are retained. Normal
  game-core tests pass, paired saved/unsaved digests both361c1d0e.

Reproduce `game-core/tools/wide-exp-phase-diff.sh UPSTREAM 777`, then2034.
The normal app/custom/save sanitizer tests and3DS build remain required. The
custom-only Task63 check is skipped. No run-core edits are included.

Natural encounter generation, wide combat arithmetic, EXP split/award callers,
level-up learning/evolution execution, all scene inventory interactions and
number formatting outside this bounded domain still require their own source
inventory and evidence. Passing these matrices does not close battle-core.
