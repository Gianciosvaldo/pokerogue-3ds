# Paused: the dex and starter unlocks slice (2026-09-30)

Paused by lorenzo's call (battle screen first, fewer open threads). No code is written yet: this branch is
PR #28's branch (`claude/project-thread-6mhynv`, not merged when paused) plus this note. Start the next
thread by rebasing onto main once #28 lands, and delete this file in its PR.

## Why this slice

For M1 (docs/beta-plan.md) game-core still owes: the unlocked starters and their save ("save and resume a
run and the unlocked starters"), the starter select screen's rules, and the dex side of catching. Catching
itself, Ball/Run, the enemy switch and battle start wait on battle-core. The full Pokédex, game stats,
achievements, vouchers, eggs and candy upgrades are M3.

## Plan (what to build)

1. `gc_system` (upstream `GameData`, owned by the app, lives across runs), attached to the run. Per species
   (all of `speciesDataRegistry`): `seenAttr`, `caughtAttr` (u64: form bits go to bit 7 + formIndex),
   `natureAttr` (bit n + 1), `seenCount`, `caughtCount`, `hatchedCount`, `ivs[6]`. Per starter (572):
   `abilityAttr`, `passiveAttr`, `valueReduction`, `candyCount`, `friendship`, `eggMoves`, `moveset`,
   `classicWinCount`. Plus `unlocks` (ENDLESS_MODE, MINI_BLACK_HOLE, EVIOLITE; reward-core's pools read
   the last two). Skip `gameStats`, achievements, vouchers and ribbons for now (M3).
2. New save defaults: `initDexData` / `initStarterData` in `src/system/game-data.ts` (line ~1497): default
   starters get `NON_SHINY|MALE|FEMALE|DEFAULT_VARIANT|DEFAULT_FORM`, IVs 15, one neutral nature each from
   `executeWithSeedOffset(randSeedItem([HARDY, DOCILE, SERIOUS, BASHFUL, QUIRKY]) per starter, 0, "default")`,
   `abilityAttr = ABILITY_1`.
3. Dex updates in a run (all in game-data.ts ~1564-1880 unless noted):
   - `setPokemonSeen(enemy, true, trainer)` for every enemy at the encounter when not loaded
     (`src/phases/encounter-phase.ts:149`).
   - Evolution (`src/field/pokemon.ts:6263`): `updateSpeciesDexIvs(species, ivs)`, `setPokemonSeen(p, false)`,
     `setPokemonCaught(p, false)` (only when the root species is caught; recurses into prevolutions; unlocks
     abilities and natures on the starter; can show `battle:addedAsAStarter`, e.g. Pichu into Pikachu).
     Form changes do the same (`changeForm`, pokemon.ts:6370).
   - Catch (`src/phases/attempt-capture-phase.ts:272` and `:313`): `updateSpeciesDexIvs(root(true), ivs)`,
     `setPokemonCaught(p)` with counts, candy (`addStarterCandy` on the root: shiny 5 * 2^variant, boss x2)
     and the `battle:addedAsAStarter` message for a new starter. Do the dex part now, hook it to battle-core's
     catch when it lands; the party-full release flow belongs with catching.
   - `addFriendship` (pokemon.ts:6080) replaces game-core's two direct `friendship +=` (gc_phases.c:97,
     gc_stubs.c:126): candy progress on the root, `* CLASSIC_CANDY_FRIENDSHIP_MULTIPLIER` (3; no timed
     events), cap by `getStarterValueFriendshipCap(cost)`, Rare Candy capped at 200. Losses (battle-core's
     faint, -5) don't touch candy.
   - The `SPECIES_CAUGHT` evolution condition (gc_mon.c:271, now a stub) reads `dexData[id].caughtAttr`.
   - Classic win (`src/phases/game-over-phase.ts`): `incrementRibbonCount` per party member (and its
     `getRootSpeciesId(true)` if different), then `handleUnlocks`. Signal a system save at game over.
4. Starter select rules for the app (`src/ui/utils/starter-select-ui-utils.ts`, `initStarterPrefs` and
   `addToParty` in `src/ui/handlers/starter-select-ui-handler.ts` ~620 and ~2186): unlocked = caughtAttr != 0;
   cost = `getSpeciesStarterValue` (valueReduction halves below 1); shiny/variant/gender/form from caughtAttr
   (form must be `isStarterSelectable`); abilities from abilityAttr (the single-ability ABILITY_2 quirk);
   natures from natureAttr; defaults per `getStarterDetailsFromPreferences`; the Starter's `ivs` are the dex
   IVs, `passive` = passiveAttr is exactly ENABLED|UNLOCKED, `pokerus` from `getPokerusStarters()` (5 by
   the UTC date seed), moves from `getStarterMoves` (level 1-5 moves + unlocked egg moves). SelectStarterPhase
   also sets `luck = getDexAttrLuck(caughtAttr)`, which game-core doesn't yet. `gc_run_set_starters` should
   check all of this against the system data.
5. System save: same named-field format as `gc_save.c`, sparse (only non-default entries), own buffer.
   Upstream saves it with the session at each encounter start (`GC_EV_SAVE_POINT`) and after game over.
6. Data to dump from upstream (new `gen/dump-dex.test.ts`): per species `hasVariants()` (variant
   masterlist), forms (`isUnobtainable`, `isStarterSelectable`, form-change targets for the Castform rule),
   `malePercent`, `getPrevolution`, species `ability2`; `speciesEggMoves`; the default natures of item 2.
7. Test: a diff test that runs `setPokemonSeen` / `setPokemonCaught` / `updateSpeciesDexIvs` / `addFriendship`
   on random Pokemon in upstream and compares the dex and starter data with the C port.

Upstream checkout: `battle-core/tools/setup-upstream.sh <dir>` works from a session here (8555c08).
