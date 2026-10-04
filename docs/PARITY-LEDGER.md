# PokeRogue 3DS Parity Ledger and Upstream Drift Matrix

**Port Target:** `pagefaultgames/pokerogue` pinned commit [`8555c08c823b856cbec4eb99ca84ea52a955836d`](https://github.com/pagefaultgames/pokerogue/commit/8555c08c823b856cbec4eb99ca84ea52a955836d) (short `8555c08`).
**Public Beta Baseline (Inspected Separately):** `pagefaultgames/pokerogue` commit [`4f0969875845fb412c5b0af8ed4bc006484a8f2a`](https://github.com/pagefaultgames/pokerogue/commit/4f0969875845fb412c5b0af8ed4bc006484a8f2a) (42 commits ahead of target `8555c08`). Public beta mechanics are outside the port target and are not mixed into pinned parity claims.
**Initial ledger baseline:** `58030d6` after Task 57 merged. **Coordinator update:** main `cdac97d859cd39ef96200570287efb28b77bc287` after Task 88 / #102 merged, 2026-10-03. Historical matrix counts are recorded owning-README scopes, not newly rerun comparisons; dated rows below distinguish actual new validation and unmerged work.

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
| `ai-core` | Enemy AI Move & Matchup Scoring | Recorded singles coverage and merged Stuff Cheeks restriction | `ai-core/src/*.c`; `ai-core/README.md` Task 88 | `8555c08` | Existing 5000-case producer per seed 777/9001/4242/31337, Node 22.23.3; exact commands in [#102](https://github.com/Gianciosvaldo/pokerogue-3ds/pull/102) | 4938/4946/4942/4936 covered/identical, 0 mismatches; total 19762, 238 uncovered | Node 24: historical 2/9/9/12 failures; declared upstream Node >=24.9.0 conflicts with HANDOFF Node 22. Illusion Task 68 blocked by 67; doubles remain |
| `run-core` | Run Generation Suites | Waves, biomes, trainer species, wild encounters, shop prices, exp, party exp, upkeep, luck, daily run rules, gym/E4 fixed teams, rival teams (Task 48), Eternatus & Paradox fixed teams (Task 50 / PR #92) | `run-core/src/*.c`, `gen/out/*.c` | `8555c08` | `run-core/tools/difftest.sh <upstream> <N> <seed> <suite>` | Wave (20k), shop (20k), biome (20k), encounter (20k), newbattle (20k), exp (20k), partyexp (20k), upkeep (20k), trainerspecies (20k), luck (20k), daily (20k), battlegen (10k), pokemongen (8k), trainerparty (3k), rival (3k / 12k total in PR #75), gym_e4 (3k), wildenemy (3k), enemyitems (3k), enemybuffs (2k), challenge (2k) - all 0 mismatches | Enemy moveset generation rules (Task 63) |
| `reward-core` | Options, victory and application | Main option/victory APIs; unmerged bounded evolution request/state slice in [#104](https://github.com/Gianciosvaldo/pokerogue-3ds/pull/104), head `85ae453d378870c8bb6a5bcd8aef79374a7ce50b` | `reward-core/src/*.c`; additive evolution-state producer/comparator | `8555c08` | Exact commands/outputs in #104 and owning README | New draft slice: 8000/8000 identical. Existing seed-99 rewards: 3950/4000 covered, 0 fail, 50 unported; victory 4000/4000; seed-5 victory 20000/20000. Apply: 3865 pass / 135 fail on original and changed C | Draft: unrecorded Math.random fusion status draw; real fusion HP/stat gaps. Complete form/fusion/evolution execution and game-core integration remain. Combined all script stops before memory |
| `game-core` | Headless Classic Run Loop & Save System | Starter selection, wave progression, money, exp, save/load system (`PR3DSSYS` system data and `PR3DSRUN` active run save), Mystery Encounters logic tree (Task 46), Held-item mapping and consumable loss persistence (Task 57 / PR #97) | `game-core/src/*.c` | `8555c08` | `make -C game-core test`, `build/gcplay --auto 1000 --jobs 8`, `build/gcplay --auto 1000 --jobs 8 --save-test` | Task 87 / #101 merged: 36,471 resume checks and 1000 normal + 1000 saved strict terminal completions. Tested helper/battle-repair head: 1000-run digests fea61f28/fea61f28, 57,502 turns on each path; 0 stuck/crashed/timed out | Reward option integration (Task 59), Consumables application integration (Task 60), Evolution/form item lifecycle (Task 61), Fusion lifecycle (Task 62), Enemy movesets integration (Task 64), Fixed trainer team integration (Task 65), Final boss transition & shields (Task 66), Biome weather integration (Task 76), Flee in run loop (Task 77), Starter unlock gates (Task 78), Classic win progression (Task 79), Egg generation & vouchers (Task 80), Egg hatching (Task 81), Mystery encounter validated money/party comparator (Task 82), Mode entries (Task 83), System save metadata (Task 84) |
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
- [Actions run #75](https://github.com/Gianciosvaldo/pokerogue-3ds/actions/runs/37114586055), head `3966f42930a8e0a399b808a47a786f328419e7f3`: historical Task 48 rival validation (12,000 fights, zero mismatches). This is a workflow run, not PR #75; no final-head/current-main rerun is claimed.
- Task 58 (commit [`836d10c`](https://github.com/Gianciosvaldo/pokerogue-3ds/commit/836d10c)): Stuff Cheeks berry restriction in `ai-core`.
- Task 55 (commit [`f5a7dff`](https://github.com/Gianciosvaldo/pokerogue-3ds/commit/f5a7dff)): Luck, Passives & Shiny Tier UI Displays.
- Task 51 (commit [`d603c37`](https://github.com/Gianciosvaldo/pokerogue-3ds/commit/d603c37)): Shop UI rendering and Lock Capsule visuals.
- Task 49 (commit [`cfd3bf3`](https://github.com/Gianciosvaldo/pokerogue-3ds/commit/cfd3bf3)): Differential runner closures opt-in.
- Task 46 (commit [`47b61df`](https://github.com/Gianciosvaldo/pokerogue-3ds/commit/47b61df)): Mystery Encounter choices and action logic tree.
- Task 41 (commit [`7a8806a`](https://github.com/Gianciosvaldo/pokerogue-3ds/commit/7a8806a)): Mid-battle form changes in `battle-core`.

---

## 5. Unavailable Checks and Recorded Limitations

1. **Task 85 (3DS Hardware / Emulator Acceptance):** Human/console gated; requires physical Old 3DS or Azahar emulator execution to evaluate UI rendering, frame rates, audio, and SD card persistence.
2. **Task 87 / merged [#101](https://github.com/Gianciosvaldo/pokerogue-3ds/pull/101):** Mystery history/current node, repeated summon effects, battle RNG and retained switch-out flag repaired. 36,471 resume checks pass. Independent strict checks observe terminal screens for 1000 normal and 1000 saved runs, maximum 450/472 steps of 200000. Original legacy check/chooser remains unchanged. Merged-head 1000-run digest 0cba4e49/0cba4e49; later Task 90/helper head fea61f28/fea61f28, 57,502 turns each, zero stuck/crashed/timed out. This establishes host consistency, not upstream full-run parity.
3. **Task 88 / merged [#102](https://github.com/Gianciosvaldo/pokerogue-3ds/pull/102):** Node 22.23.3 comparisons: 19762/19762 covered/identical out of 20000 generated, 238 uncovered, zero mismatches. Original Node 24.19.0 2/9/9/12 failures remain: Math.pow's last bits differ, propagating through Stealth Rock matchup scores. This is a runtime-contract conflict, not a tolerance waiver or complete AI parity.
4. **Task 89 / draft [#104](https://github.com/Gianciosvaldo/pokerogue-3ds/pull/104):** Fire/Water/Thunder/Dawn stone application, primary and fusion evolution requests: 8000/8000 identical, zero skipped/thrown, 63 named party fields/inventory/RNG/queue compared, mutation detection passes. Existing apply regression is 3865 pass / 135 fail on both main and changed C; unseeded status randomness is not recorded. Implementation stopped under HANDOFF2. EvolutionPhase execution, game-core persistence/bookkeeping and broader form/fusion lifecycle remain outside the passing slice.

5. **Task 67 / [#103](https://github.com/Gianciosvaldo/pokerogue-3ds/pull/103), main follow-up [#108](https://github.com/Gianciosvaldo/pokerogue-3ds/pull/108):** #103 is merged into the prerequisite branch; helpers are absent from checked main because #106 merged earlier. #108 is ready for review with CI #104/#105 host/3DS PASS on exact c9f93ae head. Actual dormant setters/getters/break/reset: 8000/8000, all 32 type-flag combinations, mutation/truncation detected. Natural Illusion still unimplemented: 9 TODO / 0 executed. Nicknames/fused display names/fusion gender/visual presentation and natural dispatch remain deferred; reset cleanup is not a natural switch-disguise differential.
6. **Task 90 / merged [#106](https://github.com/Gianciosvaldo/pokerogue-3ds/pull/106):** Before baseline plain/rich/switch/summon failures 1/7/2/30, after 0/0/0/0, covered 1940/1826/1885/1856. Removes form-based Max immunities absent from the pin, evaluates ability form callbacks and queues enabled Daily NONE passives. Existing producers/comparators/exclusions unchanged; no coverage loss. CI #102 host/3DS pass.
7. **Task 91 / draft [#107](https://github.com/Gianciosvaldo/pokerogue-3ds/pull/107):** Separate actual TM exclusion/relearner input lists and base-moves filtering under summon overrides. New comparisons 8000/8000 and 24,000 TM/RNG results match; mutation detection/boundaries pass. Existing seed99/N4000 before/after outputs byte-identical: options3950/0/50unported, victory4000/0, application3865/135fail. Existing Memory producer fails at line80 with ValueHolder-not-constructor under Node22 and24, before emitting cases. It is untouched; no legacy Memory result credited. CI #106 host/3DS pass does not clear that blocker. Task59 remains blocked.

#104 and #107 remain drafts. #108 lands an explicitly bounded reviewed helper scope; it does not complete natural Illusion. Lower Task68 requires #108 on main; Task60 requires59; Task71 requires69/70. No three-task Jules wave is unlocked. HANDOFF rule2 forbids changing established checks to obtain a pass. Run-core owner and console/hardware gates remain in force.
