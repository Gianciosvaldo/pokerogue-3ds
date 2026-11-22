# Game Stats recording (upstream `GameStats`, `src/system/game-stats.ts`)

Pinned source `8555c08c823b856cbec4eb99ca84ea52a955836d`, Node 22. This slice records the profile's game statistics where upstream
records them, so the Game Stats screen (a later PR) and the achievement checks read real numbers. Nothing here draws a screen.

## Which upstream counter is which field

Upstream has 35 counters. Where this port already had a counter with the same meaning it is reused (no second copy to drift):

| upstream | port | written where (upstream site -> port site) |
| --- | --- | --- |
| `battles` | `stats.total_battles` | `BattleEndPhase` -> `battle_end()` in `gc_phases.c` |
| `pokemonCaught` | `stats.total_catches` | `setPokemonSpeciesCaught` (not from an egg) -> `gc_system.c` |
| `highestDamage` | `stats.highest_damage` | `MoveEffectPhase`, player side -> battle-core callback `PR_GAME_STAT_DAMAGE` |
| `sessionsWon` | `stats.classic_wins` | `GameOverPhase` (already ported) |
| `ribbonsOwned` | `stats.ribbons_owned` | already ported |
| `eggsPulled` and the five tiers | `stats.eggs_pulled*` | already ported |
| `playTime` | `gstats.play_time` | the one-second timer -> `sysd_play_tick` in the app (see below) |
| `classicSessionsPlayed`, `endlessSessionsPlayed` | `gstats.*` | `SelectStarterPhase` -> `gc_run_set_starters` |
| `dailyRunSessionsPlayed`, `dailyRunSessionsWon` | `gstats.*` | recorder exists; the Daily Run is not in the app, so these stay 0 |
| `highestEndlessWave` | `gstats.highest_endless_wave` | `BattleEndPhase`, Endless and Spliced Endless only, wave + 1 |
| `highestLevel` | `gstats.highest_level` | `LevelUpPhase` -> `level_up()` |
| `highestMoney` | `gstats.highest_money` | **only** when a session is loaded (`rs_resume`), exactly as upstream (it never rises during play) |
| `highestHeal` | `gstats.highest_heal` | `PokemonHealPhase`, player side -> callback `PR_GAME_STAT_HEAL` |
| `pokemonSeen` + rarity + shiny seen | `gstats.*` | `setPokemonSeen` (a trainer's Pokemon do not count the rarity ones) |
| `pokemonDefeated` | `gstats.pokemon_defeated` | `VictoryPhase.start`, skipped when the encounter prevents game stats updates (Training Session: `me_no_stats`) |
| `pokemonHatched` + rarity + shiny hatched | `gstats.*` | `setPokemonSpeciesCaught` from an egg |
| `subLegendary/legendary/mythical/shiny` caught | `gstats.*` | `setPokemonSpeciesCaught` |
| `pokemonFused` | `gstats.pokemon_fused` | `PlayerPokemon.fuse` -> `gc_fusion_scene_apply` |
| `trainersDefeated` | `gstats.trainers_defeated` | `BattleEndPhase`, a won battle with a trainer |

`stats.total_hatches` stays per egg (the custom hatch achievements use it); `gstats.pokemon_hatched` is upstream's (counted per prevolution stage).
The legacy fields `stats.total_shinies`, `highest_wave`, `total_money_earned` and `endless_wins` have no upstream counter and are untouched.

Upstream quirks kept on purpose: `highestMoney` only moves on load; `setPokemonSpeciesCaught` recurses through the prevolutions and counts at each
stage (a caught Charizard is 3 `pokemonCaught`); the rarity chain is sub-legendary, then legendary, then mythical, never two; a fusion's half counts
as shiny through `isShiny()`. Only `ribbonsOwned` and `sessionsWon` feed upstream achievements through `gameStats`; the rest are for the screen.

Where upstream does not use the phase, nothing is recorded: Potions, Pain Split and Regenerator do not go through `PokemonHealPhase`, so they never
raise `highestHeal`; the port's callback sits in the same place as upstream's phase.

## Save format

A new section `gstats` (26 fields) is written only when at least one is non-zero, before `achv`; a profile that counted nothing saves the same bytes as
before and an older profile loads with all of them 0. `sizeof(gc_system)` goes from 768,440 to 768,544 bytes (+104, the 26 fields); `gc_system` is held
in 4 static copies and 1 transient one in the app, so the cost is about 520 bytes in all.

## Play time

`profile_tx.c` hashes the serialized profile at begin and again at execute and refuses with `ptx:profileChanged` if it moved, so play time must not tick
inside `gc_system` while a pull is open. `system_data.c` keeps the pending milliseconds outside `g_sys` (`sysd_play_tick`, called once per frame by the
device main loop with the real time that passed, each step capped at 5 s so a sleep or a long SD load is not play time) and `sysd_save` folds the whole
seconds in after the gate check, before serializing. `sysd_play_seconds` returns profile + pending (what the screen will show). Tested by
`game/tests/play_time_test.c`. Not measured on a 3DS.

## Validation

All commands from `game-core/`, upstream checkout at the pin, Node 22.

- Differential of seen / caught / hatched, by rarity and shine, with fusions: `tools/gamestatsdiff.sh <upstream> 2000 <seed>`; the producer is
  `tests/gamestats.producer.test.ts` (random ops: seen, unlock, caught or hatched with `showMessage` random, fusions with a shiny half) and the comparator
  `tests/gamestats_diff_test.c` compares 15 counters after every op. TOTAL mismatches 0 on seeds 777, 4242, 31337 and 9001 (2000 ops each).
- `tools/gamestatsplant.sh build/gamestats-777.txt`: 12 planted bugs in `gc_system.c`, all caught (exit 1). One more was tried and is an equivalent mutant
  (a fusion half's shine is only ever set together with a fusion species), so it is not listed.
- Hooks (`tests/gamestats_host_test.c`, `make build/gamestats_host_test && build/gamestats_host_test`): recorders, session modes, Endless wave + 1,
  trainers, a real headless wave (victory, level up, battle end, the lead's Tackle for damage, Recover for heal, a foe's Tackle and Recover not counted),
  the Training Session skip, the save format. `tools/gamestatshostplant.sh`: 16 planted bugs in game-core and battle-core, all caught.
- The damage and heal values are bounds-checked in the host test, not compared with upstream turn by turn (the callbacks sit on the exact lines upstream
  writes `highestDamage` / `highestHeal`).
