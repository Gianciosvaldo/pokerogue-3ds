# reward-core: the rewards after a wave

Native C port of PokeRogue's player rewards: the reward options after a wave (pools, weights, luck upgrades, rerolls, the Lock Capsule's locked tiers, the copy of the screen after a TM), the fixed rewards some waves give, and next what each reward does when it is picked. Like run-core, it is diff-tested against upstream `pagefaultgames/pokerogue` @ `8555c08`. It reads battle-core's and run-core's RNG, run state and generated tables **read-only**; `make` builds both libraries into reward-core's own build folder.

The API is [`include/pr/reward.h`](include/pr/reward.h). The run loop (game-core) fills a `pr_reward_ctx` from the run before each reward screen: the party as the rolls see it, the player's items, ball counts, unlocks, the arena. A few of its fields are other ports' answers, marked "caller" in the header: `canSetStatus` (battle-core), `canApplyAbility`, the learnable level moves and the level-move list the TM filter leaves out (learnsets, not ported yet), each attack move's types for item spawns. The diff test takes those from upstream, so reward-core is checked given correct inputs.

## What works, and how it was checked
| Piece | Upstream | State |
|---|---|---|
| The item catalogue: 109 `modifierTypes`, every type a generator can make (624: berries, vitamins, X items, type boosters, species boosters, TMs, evolution and form change items, mints, Tera Shards, Shuckle Juice, Black Sludge) with its English name and group, stack and held limits, the PLAYER pool (86 entries), the TM tiers and each species form's TM list, the item evolutions and item form changes with their conditions | `src/modifier/modifier-type.ts`, `init-modifier-pools.ts`, species data | Dumped by `gen/dump-rewards.test.ts` into `gen/out/`; the dump fails if upstream adds a weight function or a form change condition the port doesn't know |
| The 55 weight functions of the PLAYER pool (potions, revives, PP items, lures, balls at 99, vouchers by reroll count, Eviolite, Leek, the orbs, Mystical Rock, Memory Mushroom, DNA Splicers, Tera items, the wave-199/189 skips...) | `init-modifier-pools.ts` | Matches |
| `regenerateModifierPoolThresholds(party, PLAYER, rerollCount)`: each generator rolls in pool order; an item the player holds at its stack limit weighs 0, except held and form change items (a generator that came up empty counts as neither) | `modifier-type.ts` | Matches, including the RNG state |
| The generators: berries, vitamins, X items, attack type boosters (3 per type at most), species stat boosters, the three TM tiers (`getCompatibleTms(true, true, true)`), evolution items (paused evolutions, forms, conditions), form change items (Mega Bracelet and Dynamax Band, conditions, the held ones left out, Necrozma's rule), mints, Tera Shards | `modifier-type.ts` | Matches |
| `getNewModifierTypeOption` (the tier roll, luck upgrades, the retry after an empty generator, a tier down after 100), `getModifierTypeOptionWithRetry` (duplicate names or groups, where two undefined groups count as the same, as upstream; Hardcore turning down revives), `getPlayerModifierTypeOptions` with `CustomModifierSettings` (guaranteed options, items and tiers, fill remaining, luck upgrades off), `withTierFromPool` (with upstream's `if (defaultTier)` quirk: a Common-only item keeps an undefined tier) | `modifier-type.ts` | Matches |
| `SelectModifierPhase.start`: `resetSeed` on a first roll, the thresholds (kept for a copy), `getModifierCount` (3, Golden and Silver Poké Balls, custom counts) | `src/phases/select-modifier-phase.ts` | Matches |
| The fixed rewards of a won wave: Lock Capsule at classic wave 165, EXP Share, EXP Charm, Super EXP Charm (with `offsetGym`), Golden Poké Ball, vouchers every 50 endless waves, the daily run's, and whether a reward screen follows | `src/phases/victory-phase.ts` | Matches |

### Test runs
`tools/difftest.sh <upstream> <N> <seed> rewards`: each case sets a random game mode (challenge runs get Fresh Start, Hardcore and other challenges at random), run seed, wave (a sixth of them on the waves the rules single out), unlocks, the fusion event, the dex entries the conditions read, biome, weather and ball counts (some at 99), and a real party of 1 to 6 with random species (half from the species the rules name), forms, fusions, levels, HP (full, fainted, in between), statuses, natures, genders, abilities, passives, movesets with used PP and PP Ups, used TMs and shininess, then random held items and persistent items (stacks up to their limits, lures part-way through). One case in 8 stacks attack moves of one type. Then it runs up to three screens in a row as `SelectModifierPhase` would: a first roll or a reroll, a reroll (rerollCount 1 to 3, with the Lock Capsule's tiers half the time), a copy of the screen (the old options, or random custom settings), a sixth of them with random `CustomModifierSettings`. Every option (item, generated argument, tier, luck upgrade, English name), the option count and the RNG state after each screen are compared.

| Seed | Cases | Screens | Pass | Mismatches | Unported |
|---|---|---|---|---|---|
| 7 | 1,500 | 3,068 | 1,480 | 0 | 20 |
| 99 | 4,000 | 8,052 | 3,950 | 0 | 50 |
| 777 | 4,000 | 8,020 | 3,964 | 0 | 36 |

The unported cases are all Moveset Randomizer runs (its TM shuffle isn't ported; the port reports it instead of guessing).

`tools/difftest.sh <upstream> 20000 <seed> victory` runs upstream's own `VictoryPhase.start()` with the phase queue stubbed, for a random mode, wave (a third on the waves the rules name, some up to 1,300), `offsetGym` and battle type: seed 5, 20,000 cases, 0 mismatches (9,984 items, 12,402 reward screens).

Planted bugs (the seed-99 cases replayed through a changed port): 16 of 17 are caught, from the Poké Ball weight ignoring the game mode (958 cases) and two undefined groups not counting as duplicates (1,901) to the attack type booster's cap of 3 (5), the Mystical Rock's full stack (4) and a held item's stack limit (4). The one not caught is Necrozma's rule, which can't change anything with today's data: the Ultranecrozium Z and the two N-items it looks for are never offered from the same form (checked in `gen/out/reward_data.c`).

## Memory Mushroom / move relearning

Task 43 adds the two-stage Memory Mushroom path. `pr_reward_memory_moves` mirrors
`PlayerPokemon.getLearnableLevelMoves()`: previous level/evolution/pre-evolution/relearner rows,
eligible unlocked egg moves, and used TMs are combined, currently-known moves are removed, and the
stable upstream source ordering is preserved. `pr_reward_memory_begin` represents
`PartyUiMode.REMEMBER_MOVE_MODIFIER`; `pr_reward_memory_choose` represents the queued
`LearnMovePhase(..., LearnMoveType.MEMORY, cost)` and records that the reward screen is copied
before the consumable is applied.

The caller supplies parsed learnset rows, unlocked egg moves and challenge egg-relearn availability
in `pr_reward_mon`; reward-core owns the Memory Mushroom filtering/order and target state, not the UI rendering.

## Not done yet
- Trainers' `genModifiersFunc` and fixed battles' `customModifierRewardSettings` (the rival's and bosses' reward screens); a caller can pass custom settings already.
- What each reward does when it is picked or used (healing, revives, PP, TMs, evolution items, vitamins, Rare Candy, held items into the battle's modifier list, lures, charms, exp items). Next in this folder.
- The Moveset Randomizer's TM shuffle (reported as `PR_REWARD_UNPORTED_CHALLENGE`), timed events' fixed battle rewards, and the dev override `ITEM_REWARD_OVERRIDE`.
- Rare items (form change items' effects, DNA Splicers, Mega and Tera) wait for M2, as the beta plan says; they already appear in the options.

## Build and test
```
make                                   # build/libreward.a, build/rewardstest, build/victorytest
tools/redump.sh <upstream>             # regenerate gen/out/ from upstream
tools/difftest.sh <upstream> 4000 99   # rewards and victory cases, replayed through the port
```
The upstream checkout is the one `battle-core/tools/setup-upstream.sh` prepares. To look into a mismatch: rerun the producer with `DIFF_ONLY=<case>` (it prints every pool entry's weight and every generator result for that case to stderr), and replay with `DEBUG_CASE=<case>` against a build made with `CFLAGS="-O0 -g -std=c99 -DPR_REWARD_DEBUG"`, which prints the port's.
