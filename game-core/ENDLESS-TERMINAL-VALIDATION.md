# Battle-end queue and Endless terminal flow

Reference: upstream `8555c08c823b856cbec4eb99ca84ea52a955836d`, locales
`270ed0a2938b2c3e2e34e7de940e84aecadf548b`, Node 22.

`BattleEndPhase.start` first promotes victory if **any** queued BattleEndPhase is
victorious, then removes all duplicates. The old C loop inspected the lowest
level while removing from the highest; a victorious nested duplicate could be
removed before it was examined. Scan every level before removing the phases.

After scoring, Endless and Spliced Endless at wave **5850 or later** clear the
regular phase queue and unshift `GameOverPhase(true)`, before post-battle ability
callbacks. Preserve Honey Gather and money pickup afterward. This cancels queued
rewards, trainer victory and a new battle; the game caller reaches its clear
screen. The source applies this terminal rule even when the incoming phase is
not victorious.

For a non-victory, the source still queues the normal reward when an enemy fainted,
including after a successful escape. Only the no-enemy-faint fallback is gated
by `!successfulRun`. Retain the existing reward-phase check to avoid duplicates.

## Evidence

- **5,120 exact original queue cases**, 2,560 for each seed 777/2034. The complete
  fixed inventory covers all five modes; waves 200, 5849, 5850 and 5851; victory,
  successful escape, enemy faints, mixed duplicate phases in different nested
  levels, an existing reward phase and both current queue levels. Non-Endless
  high-wave inputs are explicit synthetic controls, not naturally generated runs.
- **5,120 additional exact cases** add a native observation before the actual
  `getPokemonAllowedInBattle()` callback loop, while retaining the original final
  queue comparison. A passthrough C observer records the actual callback's victory
  flag and queue. This detects clearing too late, even if the final queue matches.
- Invoke real native `BattleEndPhase.start`; only enemy rendering teardown and
  the final phase handoff are replaced. Compare queue levels, current level,
  phase identity/order, terminal victory and reward mode, plus both RNG streams.
  Scoring/statistics, presentation playback, arbitrary phase payloads, dynamic
  queue handling and subsequent browser GameOver orchestration are not established
  by this queue-control proof.
- Native RNG is captured before constructing GameManager and restored after
  natural startup. Both source pins remain tracked-clean. Challenge controls use the empty rule set. No source exceptions or
  case filters are used. The reader enforces the original case-by-case inventory.
- The actual pre-change binary fails **1,088 of 2,560** first-seed cases.
- **320 actual game caller checks** finish terminal queues in both Endless modes,
  cancel rewards/new encounters, retain Honey Gather and wallet behavior, reach
  the clear screen, preserve RNG and refuse a terminal session save.
- **13 compiled faults caught; 16 malformed captures rejected**. Faults cover
  missing/late/early terminal clearing, wrong mode, stale future phases, clearing
  after abilities, lost nested victory, wrong escape/reward gates, duplicate
  rewards and lost callback victory.

Two native fixture failures are preserved: a placeholder reward constructor was
replaced with the actual EXP_CHARM factory, and the coverage counter now coerces
optional `isEndless` to boolean before numeric addition. The 512 terminal-case
floor is unchanged; every final capture has 512 terminal contexts, 640 promoted
victories and 1,312 reward phases. No expected queue values were rewritten.

## Final local gates

Normal game-core tests pass, including the retained Honey Gather caller and the
new terminal caller; saved/unsaved digest `8669f384` agrees. The full normal
app/custom/save sanitizer suite passes: 10,000 normal runs (`8b5d6de3`), system
saves, Save and Quit, custom runs, custom resume and custom Save and Quit.
The 3DS build passes. Battle-core sources are unchanged by this follow-up;
its last merged callback and retained-corpus results remain recorded in the
Honey Gather validation. No comparison floor or historical required case changed.

## Reproduction and remaining scope

Run `game-core/tools/endless-terminal-diff.sh /absolute/upstream` using Node 22.
It verifies source pins, generates both required versions for both seeds, replays
all inventories and runs the caller and fault guards. Normal game-core,
app/custom/save sanitizer checks and a 3DS build are also required. Only the
separate custom-only Task63 check is excluded.

This closes these specific BattleEndPhase differences. Pickup transfer and its
callback interleaving, the final source/AI inventory, all high-level arithmetic
and full Endless journey acceptance remain open. Keep M1 double-as-single and
Illusion deviations visible; these bounded tests do not establish full battle-core
completion or complete browser end-of-run parity.
