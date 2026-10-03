# PokeRogue 3DS Parity Ledger and Upstream Drift Matrix

**Port Target:** `pagefaultgames/pokerogue` pinned commit [`8555c0818ae46555cc6a78ceb4ef1866ef11d5f3`](https://github.com/pagefaultgames/pokerogue/commit/8555c0818ae46555cc6a78ceb4ef1866ef11d5f3) (short `8555c08`).
**Public Beta Baseline (Inspected Separately):** `pagefaultgames/pokerogue` commit [`4f0969875845fb412c5b0af8ed4bc006484a8f2a`](https://github.com/pagefaultgames/pokerogue/commit/4f0969875845fb412c5b0af8ed4bc006484a8f2a) (42 commits ahead of target `8555c08`). Public beta mechanics are outside the port target and are not mixed into pinned parity claims.
**Port Baseline Commit:** [`58030d6`](https://github.com/Gianciosvaldo/pokerogue-3ds/commit/58030d6) (`main` branch head after Task 57 #97 merge).

---

## 1. Subsystem Parity Matrix

| Subsystem | Function / Feature | Supported Scope | Source Path | Commit Target | Test Command | Count / Result | Exclusions / Excluded Scope |
|---|---|---|---|---|---|---|---|
| `battle-core` | Singles Turn Engine & Damage Path | Damage path, 87/95 battler tags, move effects, stat calculation, JS math (`Math.pow` V8), weather/terrain, called moves, header moves, delayed moves, switching | `battle-core/src/*.c`, `include/pr/*.h` | `8555c08` | `battle-core/build/turntest /tmp/turns.txt` (modes: plain, RICH, STAT, STATUS, ONHIT, CHARGE, TAGS, HEAL, WEATHER, BOOST, TRAPS, PROTECT, ARENA, EFFECTS, CALLED, HEADER, DELAYED, SWITCH, SUMMON) | Plain 1,534/1,534, RICH 3,640/3,640, STAT 3,762/3,762, STATUS 3,818/3,818, ONHIT 11,767/11,767, CHARGE 3,948/3,948, TAGS 7,876/7,876, HEAL 7,716/7,716, WEATHER 7,824/7,824, BOOST 7,859/7,859, TRAPS 7,873/7,873, PROTECT 7,630/7,630, ARENA 7,385/7,385, EFFECTS 7,794/7,794, CALLED 7,781/7,781, HEADER 7,692/7,692, DELAYED 7,610/7,610, SWITCH 7,913/7,913, SUMMON 7,678/7,678 (all 0 mismatches) | Doubles (Task 73), Illusion state (Task 67), Weather form triggers (Task 69), Item theft/removal (Task 70), Special berries (Task 71), Dancer/Tera precommands (Task 72) |
| `battle-core` | Command Screen Probes (`pr_cprobe`) | Command validations (`pr_fight_move`, `pr_move_usable`, `pr_move_selectable`, `pr_is_trapped`, `pr_can_switch`, `pr_can_run`) | `battle-core/src/commands.c` | `8555c08` | `TURN_CPROBE=1` with `turntest` | 56,155 probes, 0 mismatches | None for command probe API |
| `battle-core` | Battle Initialization (`pr_start_battle`) | Encounter setup, wild and trainer battle start sequences, Set/Switch style switch prompt checks | `battle-core/src/phases.c` | `8555c08` | `TURN_START=1` (wild & trainer) with `turntest` | Wild 7,646/7,646, Trainer 7,673/7,673, 0 mismatches | Doubles battle start |
| `battle-core` | Held Items & Berries (Slice 1) | Leftovers, Shell Bell, Focus Band, Quick Claw, King's Rock, White Herb, Sitrus Berry, Lum Berry, Leppa Berry | `battle-core/src/modifiers.c`, `src/phases.c` | `8555c08` | `TURN_ITEMS=1` with `turntest` | 2,000+ cases per seed (777, 9001, 4242, 31337), 0 mismatches | Stat-pinch berries (Liechi, Ganlon, Petaya, Apicot, Salac), Enigma/Lansat/Starf berries |
| `battle-core` | Mid-battle Form Changes | Aegislash Stance Change, Darmanitan Zen Mode, Minior Shields Down, Morpeko Hunger Switch | `battle-core/src/pokemon.c` | `8555c08` | `TURN_FORMS=1` with `turntest` | 2,000 cases per seed, 0 mismatches | Weather-driven form triggers (Forecast, Flower Gift, Ice Face - Task 69) |
| `battle-core` | Mega Evolution & Dynamax | Mega Evolution and Dynamax/Gigantamax battle triggers, stat multipliers and move overrides | `battle-core/src/engine.c` | `8555c08` | `TURN_MEGA=1`, `TURN_DYNAMAX=1` with `turntest` | 2,000 cases per seed, 0 mismatches | Multi-target Max Move spread dispatch |
| `battle-core` | Fused Pokemon | Fused stats, typing, ability, and move execution in battle | `battle-core/src/pokemon.c` | `8555c08` | `TURN_FUSION=1` with `turntest` | 2,000 cases per seed, 0 mismatches | DNA Splicers reward lifecycle in game-core (Task 62) |
| `ai-core` | Enemy AI Move & Matchup Scoring | Move benefit scores, target selection, switch evaluation, Stuff Cheeks berry restriction (Task 58) | `ai-core/src/*.c` | `8555c08` | `ai-core/tools/difftest.sh <upstream> 5000 <seed>` | 5,000 cases per seed; Stuff Cheeks condition check 268/268 identical | Matchup floating-point tail mismatches documented in Task 88 (777: 2, 9001: 9, 4242: 9, 31337: 12); Disguise-aware Illusion scoring (Task 68), Doubles AI selection (Task 74) |
| `run-core` | Run Generation Suites | Waves, biomes, trainer species, wild encounters, shop prices, exp, party exp, upkeep, luck, daily run rules, gym/E4 fixed teams, rival teams (Task 48), Eternatus & Paradox fixed teams (Task 50 / PR #92) | `run-core/src/*.c`, `gen/out/*.c` | `8555c08` | `run-core/tools/difftest.sh <upstream> <N> <seed> <suite>` | Wave (20k), shop (20k), biome (20k), encounter (20k), newbattle (20k), exp (20k), partyexp (20k), upkeep (20k), trainerspecies (20k), luck (20k), daily (20k), battlegen (10k), pokemongen (8k), trainerparty (3k), rival (3k / 12k total in PR #75), gym_e4 (3k), wildenemy (3k), enemyitems (3k), enemybuffs (2k), challenge (2k) - all 0 mismatches | Enemy moveset generation rules (Task 63) |
| `reward-core` | Reward Options & Consumables | Option generation per wave, tier weights, reroll costs, luck grades; potions, revives, PP items, Rare Candy, vitamins, held items, Memory Mushroom, Candy Jar, Lock Capsule state | `reward-core/src/*.c`, `gen/out/*.c` | `8555c08` | `reward-core/tools/difftest.sh <upstream> 4000 99 rewards\|victory\|apply\|all`, `tests/candytest`, `tests/memorytest` | 4,000 cases per suite, 0 mismatches | Full party state validation after application including queued evolution/form/fusion lifecycle (Task 89) |
| `game-core` | Headless Classic Run Loop & Save System | Starter selection, wave progression, money, exp, save/load system (`PR3DSSYS` system data and `PR3DSRUN` active run save), Mystery Encounters logic tree (Task 46), Held-item mapping and consumable loss persistence (Task 57 / PR #97) | `game-core/src/*.c` | `8555c08` | `make -C game-core test`, `build/gcplay --auto 1000 --jobs 8`, `build/gcplay --auto 1000 --jobs 8 --save-test` | 14 save checks pass; soak test run completion 1,000 runs, 0 crashes/hangs/timeouts | Save-soak test divergence documented in Task 87 (no-save vs saved digest divergence under 1000-run soak); Reward option integration (Task 59), Consumables application integration (Task 60), Evolution/form item lifecycle (Task 61), Fusion lifecycle (Task 62), Enemy movesets integration (Task 64), Fixed trainer team integration (Task 65), Final boss transition & shields (Task 66), Biome weather integration (Task 76), Flee in run loop (Task 77), Starter unlock gates (Task 78), Classic win progression (Task 79), Egg generation & vouchers (Task 80), Egg hatching (Task 81), Mystery encounter validated money/party comparator (Task 82), Mode entries (Task 83), System save metadata (Task 84) |
| `game` | Dual-Screen UI & Battle Presentation | Dual screen UI layout, battle rendering, sprite/tone shaders, move animations, Shop UI / Luck Grade displays, Lock Capsule visuals, party/summary/biome/learnmove screens | `game/source/*.c`, `game/menus/*.c` | `8555c08` | Host unit tests (`make -C game/tests build/...` for `animtest`, `form_anim_test`, `evtest`, `switchtest`, `stat_banner_test`, `map_ui_test`, `shop_ui_test`, `summary_ui_test`, `splicer_ui_test`, `menutest`) | Unit tests pass ASan-clean on host | Real 3DS hardware / Azahar emulator run driver acceptance (Task 85 - human/console gated) |

---

## 2. Inventory of Untested Move, Ability, and Item Classes

This inventory lists known unported or partially supported classes at pinned target `8555c08` without claiming an exhaustive audit:

### Move Classes
- **Transform & Sketch:** In-battle species/moveset copying.
- **Revival Blessing:** Party revival handling.
- **Stuff Cheeks:** Turn execution and berry consumption in `battle-core`.
- **No Retreat:** Once-per-switch-in constraint.
- **Gigaton Hammer:** Consecutive turn usage restriction in turn history.
- **Spread moves in doubles:** Multi-target damage dispatch.
- **Theft and Removal:** `Thief`, `Knock Off`, `Covet` item ownership transfers.

### Ability Classes
- **Imposter:** Automatic Transform on entry.
- **Illusion:** Disguise state and apparent identity/type reads in `battle-core` (Task 67).
- **Weather Form Triggers:** `Forecast`, `Flower Gift`, `Ice Face` form transformations (Task 69).
- **Passive Trace:** Passive ability copy on entry.
- **Neutralizing Gas:** Field-wide ability suppression.
- **Commander & Cramorant Gulp Missile:** Special form and battle triggers.
- **Dancer:** Copied move queueing (Task 72).

### Item and Modifier Classes
- **Stat-pinch Berries:** `Liechi`, `Ganlon`, `Petaya`, `Apicot`, `Salac` berries.
- **Special Berries:** `Enigma`, `Lansat`, `Starf` berries (Task 71).
- **Evolution, Form, and Fusion Items:** In-run item lifecycle and party mutation in `game-core` (Tasks 61, 62).
- **Mints and TMs:** In-run stat nature changes and TM move learning choices.

---

## 3. Public Upstream Drift Inspection

**Inspected SHA:** `pagefaultgames/pokerogue` commit [`4f0969875845fb412c5b0af8ed4bc006484a8f2a`](https://github.com/pagefaultgames/pokerogue/commit/4f0969875845fb412c5b0af8ed4bc006484a8f2a) (42 commits ahead of target `8555c08`).

The public beta branch contains new upstream features and balance changes that differ from pinned `8555c08`. These changes are **outside the named port target** and are documented here for drift tracking:

1. **AI Move Selection & Weighting:** Priority move weighting and forcing two damage moves (`src/ai/ai-moveset-gen.ts`).
2. **Fixed Trainer Moveset Generation:** Updated trainer moveset selection functions (`src/data/balance/moves/moveset-generation.ts`).
3. **Passive Trace Behavior:** Modified passive ability interaction for Trace (`src/data/trainers/trainer-config.ts`).
4. **Trapping & Damage-Over-Time Tags:** Updated battler tags for trapping and DoT effects (`src/data/battler-tags.ts`).
5. **No Retreat Constraint:** Enforced once-per-switch-in usage limit (`src/data/moves/move.ts`).
6. **Revival Blessing Held Item Handling:** Adjusted held item interaction on revive (`src/field/pokemon.ts`).
7. **Uncommon Breed Balance Adjustments:** Modified stats/abilities for uncommon breeds.
8. **Summary Screen Move Deletion:** UI handler support for deleting moves in summary screen.
9. **Tutorial Speaker Boxes & Pokedex UI:** UI text box fixes and Pokedex display updates (`src/ui/handlers/`).
10. **Assets & Locales Pins:** Updated asset and localization pointers.

---

## 4. Landed PRs and Merged Task Index

- [PR #97](https://github.com/Gianciosvaldo/pokerogue-3ds/pull/97) (`outside/held-item-bridge`, commit [`58030d6`](https://github.com/Gianciosvaldo/pokerogue-3ds/commit/58030d6)): Task 57 (Held-item mapping and consumed-stack persistence in `game-core`).
- [PR #92](https://github.com/Gianciosvaldo/pokerogue-3ds/pull/92) (commit [`506d20e`](https://github.com/Gianciosvaldo/pokerogue-3ds/commit/506d20e)): Task 50 (Eternatus & Paradox Fixed Generation in `run-core`).
- [PR #86](https://github.com/Gianciosvaldo/pokerogue-3ds/pull/86) (final head [`e7f54b3`](https://github.com/Gianciosvaldo/pokerogue-3ds/commit/e7f54b3)): CI check & diagnostic cleanup audit.
- [PR #75](https://github.com/Gianciosvaldo/pokerogue-3ds/pull/75) (commit [`3966f42`](https://github.com/Gianciosvaldo/pokerogue-3ds/commit/3966f42)): Task 48 Rival differential test suite validation (12,000 cases).
- Task 58 (commit [`836d10c`](https://github.com/Gianciosvaldo/pokerogue-3ds/commit/836d10c)): Stuff Cheeks berry restriction in `ai-core`.
- Task 55 (commit [`f5a7dff`](https://github.com/Gianciosvaldo/pokerogue-3ds/commit/f5a7dff)): Luck, Passives & Shiny Tier UI Displays.
- Task 51 (commit [`d603c37`](https://github.com/Gianciosvaldo/pokerogue-3ds/commit/d603c37)): Shop UI rendering and Lock Capsule visuals.
- Task 49 (commit [`cfd3bf3`](https://github.com/Gianciosvaldo/pokerogue-3ds/commit/cfd3bf3)): Differential runner closures opt-in.
- Task 46 (commit [`47b61df`](https://github.com/Gianciosvaldo/pokerogue-3ds/commit/47b61df)): Mystery Encounter choices and action logic tree.
- Task 41 (commit [`7a8806a`](https://github.com/Gianciosvaldo/pokerogue-3ds/commit/7a8806a)): Mid-battle form changes in `battle-core`.

---

## 5. Unavailable Checks and Recorded Limitations

1. **Task 85 (3DS Hardware / Emulator Acceptance):** Human/console gated; requires physical Old 3DS or Azahar emulator execution to evaluate UI rendering, frame rates, audio, and SD card persistence.
2. **Task 87 (Save-Soak Digest Divergence):** Under 1,000-run `gcplay` soak testing, no-save digest `5c084cbb` differs from saved digest `f12eb771` at baseline `2a740ab`. Both finish with 0 crashes, 0 hangs, and 0 timeouts.
3. **Task 88 (AI Matchup Differential Tails):** `ai-core` 5,000-case differential runner prints floating-point tail mismatches on MATCHUP scores (seed 777: 2, seed 9001: 9, seed 4242: 9, seed 31337: 12) due to JS vs C float rounding differences.
4. **Task 89 (Reward Full-State Validation):** `reward-core` apply differential test validates HP, status, and PP changes, but full party evolution/form/fusion metadata mutation lifecycle validation remains in `game-core`.
