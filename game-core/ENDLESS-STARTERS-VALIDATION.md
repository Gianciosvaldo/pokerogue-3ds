# Starters in Endless and Spliced Endless

Reference: upstream `8555c08c823b856cbec4eb99ca84ea52a955836d`, Node 22.

Two starter rules of the Endless modes were missing from game-core:

1. **Cost limit.** `getRunValueLimit()` (`starter-select-ui-utils.ts`) gives 15 points in
   `ENDLESS` and `SPLICED_ENDLESS` and 10 in every other mode. `gc_starter_budget(mode)` returned 10 for
   all of them, so `gc_run_set_starters` refused any Endless party over 10 points.
2. **Spliced Endless starters are fused.** `SelectStarterPhase.initBattle` calls
   `generateFusionSpecies(true)` on every starter when `gameMode.isSplicedOnly`. The Pokemon
   constructor has already fused it once (`isSplicedOnly`, level 5 > 1) with the ordinary filter
   and then drawn its tera type, so both fusions and the tera draw are on the same Phaser RNG
   stream and the second fusion's result depends on the first one's draws.

`gc_starters_build_party` (`src/gc_run.c`) now builds the party the way `initBattle` does:

- per starter, `gc_mon_new_player` (the constructor's id draw), then in Spliced Endless the
  constructor fusion's draws (run-core's `pr_run_generate_fusion`, the same rolls) and the
  constructor's tera draw over the fused types, then `gc_mon_starter_fusion` for
  `generateFusionSpecies(true)`: the hidden-ability roll, the ability roll, the species
  (`hasEvolutions && !hasPrevolution`, not sub-legendary / legendary / mythical / trainer-forbidden,
  not the starter, not Ditto, registry order, `filterAllEvolutions`), the ability index, the
  shininess, variant and luck copied from the starter, the gender from `id % 256`, the form roll
  (`ignoreArena`), and the stats recalculated (upstream does it at the first `updateModifiers`);
- in the other modes, only the constructor's tera draw (one draw for a two-type species), so the
  ids of later members come out as upstream's.

The **Ability Charm does not apply** to either fusion: `HiddenAbilityRateBoosterModifier` is applied
only when `!this.hasTrainer()` and a player's Pokemon has a trainer. The producer holds a charm in
a fifth of its cases to show it.

## Evidence

`tools/startersdiff.sh <upstream> [N] [seed]` runs `tests/starters.producer.test.ts` (the real
`SelectStarterPhase.initBattle` on 1-6 random starters per case, in Classic, Endless and Spliced
Endless, the Phaser RNG sown per case, plus `getRunValueLimit()` in each mode) and replays it through
`tests/starters_diff_test.c`:

- the cost limit of each mode;
- per member: id, species, level, nature, ability index, gender, luck, the fusion half (species, form,
  ability index, gender, shiny, variant, luck), HP and all six stats;
- the RNG state after the whole party.

Harness changes: the scene has no current battle, as at a new run's start (the game manager's own is
a trainer battle, which limits Eevee's and Pikachu's forms); the overrides that force a starter's
shininess, nature, IVs, gender or fusion are off; `newBattle`, the arena's init, asset loading and
the music are stubbed (the tail of `initBattle`). Each starter's dex entry says it was caught as it is,
so the luck set from the dex is the constructor's own (game-core takes it from the constructor).

| seed | cases | members | fused | with a hidden fusion ability | mismatches |
| ---: | ---: | ---: | ---: | ---: | ---: |
| 777 | 600 | 2069 | 864 | 2 | 0 |
| 9001 | 1800 | 6298 | 2511 | 9 | 0 |
| 4242 | 1800 | 6399 | 2509 | 9 | 0 |

`tools/startersplant.sh <trace>` replays a trace through copies of game-core with one line changed
(the limit per mode, fusion in the wrong mode, each clause of the starter filter, the pick, the
hidden-ability rate and roll, the gender threshold and direction, the copied shininess / variant /
luck, the form roll, the stats recalculation, the constructor's fusion and tera draws): every copy
must fail the comparison.

`tests/starters_host_test.c` (`make -C game-core starters-host-test`, part of `make test`) needs no
upstream: the limit per mode; a party of 11-15 points refused in Classic and accepted in both Endless
modes, one over 15 refused everywhere; a Spliced Endless party fused with species the filter allows,
with the stats recalculated, Classic and Endless parties not fused; the same seed gives the same
party; a fused party saves at the first command screen and loads back.

Plants (`tools/startersplant.sh`, seed 9001 trace): 17 one-line changes, all exit 1. The filter's legendary,
trainer-forbidden and Ditto guards have no observable mutant (no unevolved species with evolutions is any of those in
the data; Meltan is dropped by both the mythical and the uncatchable clause, so only dropping both shows). The
`bt.game_mode` setting (Spliced rules in battle-core) is not seen by the trace and is asserted in `starters_host_test`.

Existing tests touched: `tests/endless_terminal_host_test.c` sets `fusion_custom_ability` beside `custom_ability`,
because its Spliced Endless starter is now fused and a fused Pokemon reads the fusion half's custom ability.
`gc_starters_build_party` sets `bt.game_mode` only in Spliced Endless (other modes' battle-core rules stay as before).

## Not covered

- The app still opens Classic runs only and its starter screen uses the Classic limit
  (`game/`: separate work). This change is game-core.
- Luck comes from the constructor (`shiny ? variant + 1 : 0`), upstream sets it from the dex
  (`getDexAttrLuck(caughtAttr)`): the same when the starter was caught as it is, which is how the
  producer sets the dex. This is an older difference and is not changed here.
- Challenge starter-point rules (`ChallengeType.STARTER_POINTS`) are not ported.
