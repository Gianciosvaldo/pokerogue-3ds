# run-core: the run around the battles

Native C port of PokeRogue's run-level logic: the waves now; the shop and rewards next. Like battle-core, it is diff-tested against upstream `pagefaultgames/pokerogue` @ `8555c08`. It reuses battle-core's RNG, V8 `Math.pow`, `js_round` and generated tables (enums, species) **read-only**: nothing in `battle-core/` is changed, and `make` builds battle-core's library into run-core's own build folder.

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

"Not covered" is the daily-run mode (a fifth of the cases): its seed config (trainer waves, starting money, final boss level and segments) isn't ported, so those functions report `PR_RUN_UNPORTED_DAILY` instead of guessing. Challenges' hooks report `PR_RUN_UNPORTED_CHALLENGES` the same way.

Mutation check: 22 deliberate bugs, 20 of which can change an answer; all 20 are caught, from a level curve dividing by 24 instead of 25 (76,806 mismatches) and the battle RNG seeded from the run seed instead of the wave seed (42,150) to the endless random-boss chance using floor instead of ceil (29), level 100 off by one (81), and the x25 rounding at every 250th wave dropped (83). The 2 not caught can't change anything: no random level offset on the final wave (every final wave in the ported modes already takes the x25 path) and a missing `abs()` on a sum of random fractions (never negative).

## Not done (in rough order)
1. The rest of `newBattle`: mystery encounters (`isWaveMysteryEncounter`), random trainer type and variant (`arena.randomTrainerType`, the biome trainer pools), the trainer's party levels, fixed battles' trainers.
2. Biomes: species pools per biome and time of day, biome transitions and their rolls.
3. The shop and rewards: modifier pools and weights, tiers and luck, reroll cost, the always-available shop items.
4. The daily run and challenges (the `PR_RUN_UNPORTED_*` flags).

Dev overrides (`LEVEL_CAP_OVERRIDE`, `ENEMY_HEALTH_SEGMENTS_OVERRIDE`...) aren't modelled; they are upstream's testing switches, off in a real run.

## Layout
- `include/pr/run.h` API, `src/wave.c` the wave functions (each quotes the JS it replaces)
- `tests/wave.producer.test.ts` upstream half of the test, `tests/wavetest.c` C half
- `tools/difftest.sh <upstream dir> [N] [seed]` (the upstream checkout comes from `battle-core/tools/setup-upstream.sh`)

Build: `make` (outputs to `build/`).
