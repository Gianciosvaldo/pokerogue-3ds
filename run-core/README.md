# run-core: the run around the battles

Native C port of PokeRogue's run-level logic: the waves, biomes and the shop's economy so far; the reward pools next. Like battle-core, it is diff-tested against upstream `pagefaultgames/pokerogue` @ `8555c08`. It reuses battle-core's RNG, V8 `Math.pow`, `js_round` and generated tables (enums, species) **read-only**: nothing in `battle-core/` is changed, and `make` builds battle-core's library into run-core's own build folder.

## What works, and how it was checked
| Piece | Upstream | State |
|---|---|---|
| Game-mode rules: `getWaveForDifficulty` (daily curve included), `isWaveFinal`, `isBoss`, `isBattleClassicFinalBoss`, `isEndlessBoss` and its minor/major kinds, `isTrainerBoss`, `isFixedBattle` (classic fixed waves), `getClearScoreBonus`, `getEnemyModifierChance`, `getStartingLevel`, `getStartingMoney` | `src/game-mode.ts` | Matches |
| `getLevelForWave`: base curve, boss level with its random offset and the x25 rounding at the classic final boss and every 250th wave, the "gauss" level offset of other waves | `src/battle.ts` | Matches, including the RNG state it leaves |
| `isWaveTrainer`: gym waves and `offsetGym`, the X1 waves, the ±2-wave window that earlier waves' trainer rolls (on the run seed) and fixed battles can close, the final roll against the biome's `trainerChance` | `src/game-mode.ts` | Matches, including the RNG state |
| The seeding of a new battle: `resetSeed(wave)` (wave seed, `Phaser.Math.RND`), the Battle built inside `executeWithSeedOffset(wave << 3, waveSeed)`, `isWaveTrainer` on the reset RND | `src/battle-scene.ts newBattle` | Matches |
| `getMaxExpLevel` (level cap), `getEncounterBossSegments` (legendaries, level 100, base total 670, endless random bosses from wave 250, +1 per 250 waves), `getDoubleBattleChance` with Lure, Super Lure and Max Lure | `src/battle-scene.ts` | Matches |

Runs (`tools/difftest.sh <upstream> 20000 <seed>`; each case picks a random game mode, run seed, wave (1 to 210, or up to 3,000 one time in 4), biome, gym offset, RNG state, species (or none), boss level, forced boss and 0 to 3 Lures, and checks every function above against upstream's live objects):

| Seed | Cases | Checks passed | Mismatches | Not covered |
|---|---|---|---|---|
| 777 | 20,000 | 404,052 | 0 | 15,948 |
| 9001 | 20,000 | 404,125 | 0 | 15,875 |
| 4242 | 20,000 | 404,059 | 0 | 15,941 |

Experience (`src/exp.c`): `getLevelTotalExp` and `getLevelRelExp` for all six growth rates. Below level 100 upstream reads a table of its own that isn't exported, so `gen/dump-exp.test.ts` dumps its answers for levels 1 to 99 into `gen/out/run_exp.c`; from level 100 the formulas are ported. The test (`tools/difftest.sh <upstream> 20000 <seed> exp`) checks every rate at every level from 0 to 3,000 and 20,000 random levels up to 100,000: 138,006 checks, 0 mismatches. 5 of 6 deliberate bugs are caught; the 6th (rounding instead of flooring level³) can't change anything.

Mystery encounters and trainer parties (`src/wave.c`):

| Piece | Upstream | State |
|---|---|---|
| `isMysteryEncounterValidForWave`, `isWaveMysteryEncounter`: legal waves, spacing after the last encounter, the anti-variance rate (capped at +128), the roll on the run seed at offset `wave * 3000` | `src/battle-scene.ts` | Matches |
| `Trainer.getPartyLevels`: per-strength multipliers, the weaker members' wave bonus capped at 1.225 and level offset, a double battle widening a 1-member template | `src/field/trainer.ts` | Matches |

New-battle runs (`tools/difftest.sh <upstream> 20000 <seed> newbattle`; random mode, run seed, wave, battle type and mystery-encounter save data (spawn chance, earlier encounters' waves), and a real Trainer of any type and variant with its party template pinned): seeds 777, 9001 and 4242, 20,000 cases each, 0 mismatches (2,210 encounters spawned). upstream's test framework forces the encounter rate to 0 (a dev override); the test switches that off, as a real run has it. The encounter constants (`AVERAGE_ENCOUNTERS_PER_RUN_TARGET`...) are quoted from `src/constants.ts`. 10 of 11 deliberate bugs are caught; the 11th, excluding the lowest legal wave, can't change anything (wave 10 is a boss wave, already excluded).

Biomes (`src/biome.c`, with `gen/out/run_biomes.c` dumped from upstream's `allBiomes` by `gen/dump-biomes.test.ts`: each biome's links with their weights and its trainer chance):

| Piece | Upstream | State |
|---|---|---|
| `initBiomeDepths`: the depth table walked from Town at startup (a jump to a random biome on reaching End is ported too, though no biome links to End today) | `src/init/init-biome-depths.ts` | Matches |
| `generateRandomBiome(wave)`: depth weights over the 250-wave cycle | `src/battle-scene.ts` | Matches, including the RNG state |
| `SelectBiomePhase`: the seed reset, End before the classic final boss and every 50 waves in endless, weighted links' rolls, the Map's menu (its choices in order), the random pick | `src/phases/select-biome-phase.ts` | Matches, including the RNG state |

Biome runs (`tools/difftest.sh <upstream> 20000 <seed> biome`; each case rebuilds the depth table on a random RNG state, asks `generateRandomBiome` for a random wave, and runs a real `SelectBiomePhase` for a random mode, run seed, wave, current biome and Map): seeds 777, 9001 and 4242, 20,000 cases each, all identical (49,337 direct picks, 10,017 Map menus, 646 transitions to End). 11 of 12 deliberate bugs are caught, from End's depth off by one (121,606 mismatches) to rounding weights down instead of up (982) and End coming one wave late before the classic final boss (178). The one not caught, depths ignoring a lower link weight, gives the same table with today's biome data. An undefined upstream answer (all of a biome's weighted links failing, `randSeedItem([])`) is flagged `PR_RUN_UNDEFINED_UPSTREAM`; no biome today has only weighted links.

Encounters (`src/biome.c`, with the biomes' species pools per rarity tier and time of day and their trainer pools in `gen/out/run_biomes.c`):

| Piece | Upstream | State |
|---|---|---|
| `getGeneratedWaveCycleOffset`, `getGeneratedOffsetGym` (from the run seed) | `src/battle-scene.ts` | Matches |
| `getTimeOfDay` (the 40-wave cycle, Abyss always night) and the pools for it (`updatePoolsForTimeOfDay`: the all-day list first) | `src/field/arena.ts` | Matches |
| `randomSpecies` up to the evolution step: boss or not (boss segments, the boss pool, the End rule), the tier roll with luck lowering its ceiling (fractionally for bosses), empty tiers falling back, the pick, legendary rerolls before wave 55/80 (up to 10), and the species and isBoss handed to `getWildSpeciesForLevel` | `src/field/arena.ts` | Matches, including the RNG state |
| `randomTrainerType`: boss trainers on gym waves or on request, the tier roll, fallbacks, Breeder for an empty pool | `src/field/arena.ts` | Matches, including the RNG state |
| The evolution step (`src/evolution.c`, with `gen/out/run_evolutions.c` dumped by `gen/dump-evolutions.test.ts`): `determineEnemySpecies` and `getWildSpeciesForLevel`: forced prevolutions below their level thresholds, split evolutions, the per-kind thresholds (strong, normal, wild) and the random level margin (x1, x1.1, x1.2), the chain through later evolutions, the wave-20 classic trainer battle that blocks evolving | `src/ai/ai-species-gen.ts`, `src/data/pokemon-species.ts` | Matches, including the RNG state |

Encounter runs (`tools/difftest.sh <upstream> 20000 <seed> encounter`; random mode, run seed, biome, wave, wave-cycle and gym offsets, level, luck (0 to 14 in a third of the cases) and isBoss (undefined, true or false), on a real arena): seeds 777, 9001 and 4242, 20,000 cases each, 0 mismatches. 48,103 species rolls are covered (14,768 of them bosses); the other 11,897 are daily-run cases, which report `PR_RUN_UNPORTED_DAILY`. The test checks the roll up to the evolution step (intercepted upstream, so a mismatch points at the right half), the whole `randomSpecies` with the real evolution step on the same RNG (the evolution changed the species in 28,736 of the 48,135 wild rolls), and `determineEnemySpecies` on its own for a random species, level, kind, evolving or not and forced prevolutions or not (19,140 changed species, 2,990 wave-20 classic trainer blocks), all identical. 15 of 16 deliberate bugs in the roll are caught, from the gym offset inverted (60,000 mismatches) to the legendary wave limit at 50 instead of 55 (16) and 5 rerolls instead of 10 (16). The one not caught, truncating a boss's fractional roll ceiling (63.5 with odd luck), only changes the tier when the roll lands in a band about 0.25% wide at a tier boundary, which none of the cases did. All 11 deliberate bugs in the evolution step are caught, from thresholds ignored (12,814 mismatches) to the prevolution's min() dropped (135).

The shop's economy (`src/shop.c`):

| Piece | Upstream | State |
|---|---|---|
| `getWaveMoneyAmount` | `src/battle-scene.ts` | Matches |
| The money reward: the battle's multiplier, each Amulet Coin in modifier-list order (each floors its bonus, so the order changes the total), Happy Hour | `src/phases/money-reward-phase.ts` | Matches |
| The always-available shop items (`getPlayerShopModifierTypeOptionsForWave`): which rows a wave opens, none on boss waves, prices, `getShopStatus` (no shop in the daily run) | `src/modifier/modifier-type.ts` | Matches |
| Black Sludge (`HealShopCostModifier`, first one held, any shop multiplier), both as the shop screen shows prices (applied to the base cost) and as a purchase charges them (applied to the item's cost). Upstream computes the two differently, and they differ in 14,357 of the 60,000 cases below, so both are kept | `src/ui/handlers/modifier-select-ui-handler.ts`, `src/phases/select-modifier-phase.ts` | Matches |
| `getRerollCost`: locked tiers' values, undefined tiers, the LUXURY tier (NaN, as upstream), doubling per reroll, custom multipliers (-1 when negative), Black Sludge | `src/phases/select-modifier-phase.ts` | Matches |

Shop runs (`tools/difftest.sh <upstream> 20000 <seed> shop`; each case picks a random game mode, wave (up to 5,000), 0 to 2 Black Sludges, 0 to 3 Amulet Coins with random stacks at random list positions, Happy Hour, money multiplier, reroll count, custom multiplier and option tiers): seeds 777, 9001 and 4242, 20,000 cases each, all six checks identical in every case (360,000 checks, 0 mismatches). All 16 deliberate shop bugs are caught, from the money curve's exponent (207,513 mismatches) to Black Sludge rounding instead of flooring (21,533) and the LUXURY tier counting 0 instead of NaN (1,062, the thinnest).

"Not covered" is the daily-run mode (a fifth of the cases): its seed config (trainer waves, starting money, final boss level and segments) isn't ported, so those functions report `PR_RUN_UNPORTED_DAILY` instead of guessing. Challenges' hooks report `PR_RUN_UNPORTED_CHALLENGES` the same way.

Mutation check: 22 deliberate bugs, 20 of which can change an answer; all 20 are caught, from a level curve dividing by 24 instead of 25 (76,806 mismatches) and the battle RNG seeded from the run seed instead of the wave seed (42,150) to the endless random-boss chance using floor instead of ceil (29), level 100 off by one (81), and the x25 rounding at every 250th wave dropped (83). The 2 not caught can't change anything: no random level offset on the final wave (every final wave in the ported modes already takes the x25 path) and a missing `abs()` on a sum of random fractions (never negative).

## Not done (in rough order)
1. The rest of `newBattle`: which mystery encounter spawns, the trainer's variant and party template choice (`new Trainer`), the trainer's party members (`partyMemberFuncs`, species pools), fixed battles' trainers, and the order these rolls happen in on the wave's RNG.
2. `globalScene.randomSpecies` for an empty biome pool (no biome has one today), and the rest of a wild Pokemon's generation (form, gender, shininess, moves, abilities).
3. The reward options: modifier pools and their weights (closures over the party's state), tier rolls and luck upgrades (`getNewModifierTypeOption`, `getPartyLuckValue`), the enemy's items.
4. The daily run and challenges (the `PR_RUN_UNPORTED_*` flags).

Dev overrides (`LEVEL_CAP_OVERRIDE`, `ENEMY_HEALTH_SEGMENTS_OVERRIDE`...) aren't modelled; they are upstream's testing switches, off in a real run.

## Layout
- `include/pr/run.h` API, `src/wave.c` the wave functions, `src/biome.c` biomes and encounters, `src/evolution.c` the evolution step, `src/exp.c` experience, `src/shop.c` the shop's economy (each function quotes the JS it replaces); `gen/dump-*.test.ts` write `gen/out/run_*.c` (generated, don't edit; `tools/redump.sh <upstream dir>` refreshes them all)
- `tests/*.producer.test.ts` upstream halves of the tests, `tests/*test.c` C halves
- `tools/difftest.sh <upstream dir> [N] [seed] [wave|shop|all]` (the upstream checkout comes from `battle-core/tools/setup-upstream.sh`)

Build: `make` (outputs to `build/`).
