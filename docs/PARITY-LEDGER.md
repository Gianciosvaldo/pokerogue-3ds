# PokeRogue 3DS Parity Ledger and Upstream Drift Matrix

**Port Target:** `pagefaultgames/pokerogue` pinned commit [`8555c08c823b856cbec4eb99ca84ea52a955836d`](https://github.com/pagefaultgames/pokerogue/commit/8555c08c823b856cbec4eb99ca84ea52a955836d) (short `8555c08`).
**Public Beta Baseline (Inspected Separately):** `pagefaultgames/pokerogue` commit [`4f0969875845fb412c5b0af8ed4bc006484a8f2a`](https://github.com/pagefaultgames/pokerogue/commit/4f0969875845fb412c5b0af8ed4bc006484a8f2a) (42 commits ahead of target `8555c08`). Public beta mechanics are outside the port target and are not mixed into pinned parity claims.
**Initial ledger baseline:** `58030d6` after Task 57 merged. **Current status refresh:** main `92e56dca4b74a47d106f419c845688dfa41412bd` after [#145](https://github.com/Gianciosvaldo/pokerogue-3ds/pull/145), 2026-10-07. Merge status was checked through GitHub and scope against the owning sources/READMEs. Historical matrix counts remain recorded owning-README results, not new comparisons. The Task 64 host results below were run in this task; [its validation record](../game-core/TASK64-VALIDATION.md) retains passed, failed and unrun checks separately.

---

## 1. Subsystem Parity Matrix

| Subsystem | Function / Feature | Supported Scope | Source Path | Commit Target | Test Command | Count / Result | Exclusions / Excluded Scope |
|---|---|---|---|---|---|---|---|
| `battle-core` | Singles & Doubles Turn Engine & Damage Path | Damage path, 87/95 battler tags, move effects, stat calculation, JS math (`Math.pow` V8), weather/terrain, called moves, header moves, delayed moves, switching, Weather forms (Task 69), Item removal (Task 70), Special berries (Task 71), Dancer dispatch (Task 72), Doubles first slice (Task 73) | `battle-core/src/*.c`, `include/pr/*.h` | `8555c08` | `turntest`, `weatherformtest`, `itemremovaltest`, `berrytest`, `dancertest`, `doublestest` | Plain 1,534/1,534, RICH 3,640/3,640, STAT 3,762/3,762, STATUS 3,818/3,818, ONHIT 11,767/11,767, CHARGE 3,948/3,948, TAGS 7,876/7,876, HEAL 7,716/7,716, WEATHER 7,824/7,824, BOOST 7,859/7,859, TRAPS 7,873/7,873, PROTECT 7,630/7,630, ARENA 7,385/7,385, EFFECTS 7,794/7,794, CALLED 7,781/7,781, HEADER 7,692/7,692, DELAYED 7,610/7,610, SWITCH 7,913/7,913, SUMMON 7,678/7,678 (all 0 mismatches) | Natural Illusion ability dispatch, Tera precommands execution |
| `battle-core` | Command Screen Probes (`pr_cprobe`) | Command validations (`pr_fight_move`, `pr_move_usable`, `pr_move_selectable`, `pr_is_trapped`, `pr_can_switch`, `pr_can_run`) | `battle-core/src/commands.c` | `8555c08` | `TURN_CPROBE=1` with `turntest` | 56,155 probes, 0 mismatches | None for command probe API |
| `battle-core` | Battle Initialization (`pr_start_battle`) | Encounter setup, wild and trainer battle start sequences, Set/Switch style switch prompt checks | `battle-core/src/phases.c` | `8555c08` | `TURN_START=1` (wild & trainer) with `turntest` | Wild 7,646/7,646, Trainer 7,673/7,673, 0 mismatches | Doubles battle start |
| `battle-core` | Held Items & Berries (Slice 1) | Leftovers, Shell Bell, Focus Band, Quick Claw, King's Rock, White Herb, Sitrus Berry, Lum Berry, Leppa Berry | `battle-core/src/modifiers.c`, `src/phases.c` | `8555c08` | `TURN_ITEMS=1` with `turntest` | 2,000+ cases per seed (777, 9001, 4242, 31337), 0 mismatches | Broader held-item families; Enigma/Lansat/Starf landed separately in Task 71 / [#115](https://github.com/Gianciosvaldo/pokerogue-3ds/pull/115); pinch berries already have dispatch |
| `battle-core` | Mid-battle Form Changes | Aegislash Stance Change, Darmanitan Zen Mode, Minior Shields Down, Morpeko Hunger Switch | `battle-core/src/pokemon.c` | `8555c08` | `TURN_FORMS=1` with `turntest` | 2,000 cases per seed, 0 mismatches | Broader form families; Forecast/Flower Gift/Ice Face landed separately in Task 69 / [#110](https://github.com/Gianciosvaldo/pokerogue-3ds/pull/110) |
| `battle-core` | Mega Evolution & Dynamax | Mega Evolution and Dynamax/Gigantamax battle triggers, stat multipliers and move overrides | `battle-core/src/engine.c` | `8555c08` | `TURN_MEGA=1`, `TURN_DYNAMAX=1` with `turntest` | 2,000 cases per seed, 0 mismatches | Multi-target Max Move spread dispatch |
| `battle-core` | Fused Pokemon | Fused stats, typing, ability, and move execution in battle | `battle-core/src/pokemon.c` | `8555c08` | `TURN_FUSION=1` with `turntest` | 2,000 cases per seed, 0 mismatches | DNA Splicers reward lifecycle in game-core (Task 62) |
| `ai-core` | Enemy AI Move & Matchup Scoring | Recorded singles coverage and merged Stuff Cheeks restriction | `ai-core/src/*.c`; `ai-core/README.md` Task 88 | `8555c08` | Existing 5000-case producer per seed 777/9001/4242/31337, Node 22.23.3; exact commands in [#102](https://github.com/Gianciosvaldo/pokerogue-3ds/pull/102) | 4938/4946/4942/4936 covered/identical, 0 mismatches; total 19762, 238 uncovered | Node 24: historical 2/9/9/12 failures; declared upstream Node >=24.9.0 conflicts with HANDOFF Node 22. Natural Illusion and disguise-in-doubles remain; loaded helpers landed in Task 68 / [#117](https://github.com/Gianciosvaldo/pokerogue-3ds/pull/117), doubles command slice in Task 74 / [#127](https://github.com/Gianciosvaldo/pokerogue-3ds/pull/127) |
| `run-core` | Run Generation Suites | Waves, biomes, trainer species, wild encounters, shop prices, exp, party exp, upkeep, luck, daily run rules, gym/E4 fixed teams, rival teams (Task 48), Eternatus & Paradox fixed teams (Task 50 / PR #92) | `run-core/src/*.c`, `gen/out/*.c` | `8555c08` | `run-core/tools/difftest.sh <upstream> <N> <seed> <suite>` | Wave (20k), shop (20k), biome (20k), encounter (20k), newbattle (20k), exp (20k), partyexp (20k), upkeep (20k), trainerspecies (20k), luck (20k), daily (20k), battlegen (10k), pokemongen (8k), trainerparty (3k), rival (3k / 12k total in PR #75), gym_e4 (3k), wildenemy (3k), enemyitems (3k), enemybuffs (2k), challenge (2k) - historical scopes, not a fresh complete R pass; [Task 63 checkpoint](../run-core/TASK63-R-REGRESSION.md) explicitly retains pending commands | Broader moveset rules, trainer constructor RNG boundaries and full-enemy generation; Task 63 bounded generator merged in [#112](https://github.com/Gianciosvaldo/pokerogue-3ds/pull/112) |
| `reward-core` | Options, victory and application | Merged options, application/evolution-request repairs (Tasks 89/91), separate TM/relearner inputs, candy and status cures, trainer rewards (Card 97) | `reward-core/src/*.c`; [owning README](../reward-core/README.md) | `8555c08` | `tools/difftest.sh <upstream> 4000 99 all`; focused evolution/learnset/trainer suites in owning README | Recorded repair: options 3950 pass / 0 fail / 50 unported; victory/apply/Memory 4000 each, zero failures; bounded evolution 8000/8000. These are historical recorded results, with runtime versions in the owning README | Broader stat-item application APIs, complete form/fusion execution and inventory/phase lifecycle; whole-run upstream parity remains unvalidated |
| `game-core` | Headless Classic Run Loop & Save System | Merged save/item bridge, reward options/healing, bounded evolution/form lifecycle, turn continuation, boss segments, catching/fleeing, TM/Memory, starter gates, candy, balls/Full Heal and bounded wild movesets | `game-core/src/*.c`; [README](../game-core/README.md) | `8555c08` | `make -C game-core test`; `game-core/build/gcplay --auto 1000 --jobs 8` with/without `--save-test` | Task 64 / #145: 29,493 adapter checks; 1000 normal + 1000 saved ended, 0 cleared/stuck/crashed/timed out; 49,315 turns each; digests `5f646aee` / `5f646aee`. [Exact commands and scope](../game-core/TASK64-VALIDATION.md) | Remaining Task 96 items; broader Task 61 and fusion 62; full movesets 64, fixed trainers 65, final boss 66, doubles/weather 75/76, win progression 79, eggs 80/81, mystery comparator 82, mode entries 83 and metadata 84 |
| `game` | Dual-Screen UI & Battle Presentation | Run-driven battle/reward/shop/biome screens, title/save/results, catching/release flow and evolution prompt (Cards 110/112/113/114/116) | `game/source/*.c`, `game/menus/*.c`; [README](../game/README.md) | `8555c08` | `make -C game/tests runtest saves_test flowtest`; run the resulting binaries | Task 64 host: saves 68 checks / 0 failures, flow 45 / 0; 1000-run app check FAILED rare evolution coverage (1 completed / 1 post-evolution move); expanded 5000-run check passed with 15 completed / 31 cancelled / 15 move checks and zero model violations | Host tests do not show rendering; full starter/UI/audio/presentation and hardware acceptance remain. Keep the 1000-run coverage failure visible; [validation](../game-core/TASK64-VALIDATION.md) |

---

## 2. Inventory of Untested Move, Ability, and Item Classes

This inventory lists known unported or partially supported classes at pinned target `8555c08` without claiming an exhaustive audit:

### Move Classes
- **Transform & Sketch:** In-battle species/moveset copying.
- **Revival Blessing:** Party revival handling.
- **Stuff Cheeks:** Turn execution and berry consumption in `battle-core` (restriction in `ai-core` Task 58).
- **No Retreat:** Once-per-switch-in constraint.
- **Gigaton Hammer:** Consecutive turn usage restriction in turn history.
- **Spread moves in doubles:** Multi-target damage dispatch (first slice ported in Task 73).
- **Theft and Removal:** `Knock Off` / `Incinerate` item removal ported in Task 70; `Thief` / `Covet` transfers unported.

### Ability Classes

Entries describing landed slices are scope reminders, not blanket unported claims. See the linked PRs and owning README before scheduling another implementation.
- **Imposter:** Automatic Transform on entry.
- **Illusion:** Disguise state and apparent identity/type reads in `battle-core` (Task 67); natural ability dispatch unported.
- **Weather Form Triggers:** bounded `Forecast`, `Flower Gift`, `Ice Face` triggers landed (Task 69 / [#110](https://github.com/Gianciosvaldo/pokerogue-3ds/pull/110)); broader form coverage remains separate.
- **Passive Trace:** Passive ability copy on entry.
- **Neutralizing Gas:** Field-wide ability suppression.
- **Commander & Cramorant Gulp Missile:** Special form and battle triggers.
- **Dancer / Tera:** Dancer copied-move slice landed (Task 72 / [#119](https://github.com/Gianciosvaldo/pokerogue-3ds/pull/119)); Tera precommands remain unported.

### Item and Modifier Classes
- **Stat-pinch Berries:** `Liechi`, `Ganlon`, `Petaya`, `Apicot`, `Salac` already have trigger/dispatch paths (`battle-core/src/modifiers.c`); Task 71 explicitly calls for regression coverage, not duplicate implementation.
- **Special Berries:** bounded `Enigma`, `Lansat`, `Starf` effects landed (Task 71 / [#115](https://github.com/Gianciosvaldo/pokerogue-3ds/pull/115)); no exhaustive held-item parity claim.
- **Evolution, Form, and Fusion Items:** In-run item lifecycle and party mutation in `game-core` (Tasks 61, 62).
- **Mints:** in-run nature/stat application remains outstanding (Task 96). **TMs / Memory Mushroom:** reward learn-move flow landed in [#128](https://github.com/Gianciosvaldo/pokerogue-3ds/pull/128), with follow-up Memory/save repair in [#131](https://github.com/Gianciosvaldo/pokerogue-3ds/pull/131).

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

- [PR #145](https://github.com/Gianciosvaldo/pokerogue-3ds/pull/145): Task 64 first slice, supported clean wild movesets; trainer RNG boundaries and broader pools remain deferred. Host CI and 3DS build passed on `3c762a3`; merge `92e56dc`.
- [PR #143](https://github.com/Gianciosvaldo/pokerogue-3ds/pull/143) and [#142](https://github.com/Gianciosvaldo/pokerogue-3ds/pull/142): Task 96 Full Heal integration and reward-core status/confusion/Nightmare handling. See the known bought-item message timing difference in the game-core README.
- [PR #141](https://github.com/Gianciosvaldo/pokerogue-3ds/pull/141): Task 96 Poké Ball rewards through reward-core.
- [PR #140](https://github.com/Gianciosvaldo/pokerogue-3ds/pull/140): earlier README/ledger refresh; historical entries below are not all current task gates.
- [PR #138](https://github.com/Gianciosvaldo/pokerogue-3ds/pull/138): Card 116 app evolution prompt; host tests do not establish graphical/console acceptance.
- [PR #121](https://github.com/Gianciosvaldo/pokerogue-3ds/pull/121): Task 92 victory continuation and enemy switch handling.
- [PR #120](https://github.com/Gianciosvaldo/pokerogue-3ds/pull/120): Card 110 app run driver.
- [PR #117](https://github.com/Gianciosvaldo/pokerogue-3ds/pull/117): Task 68 loaded Illusion AI helpers; natural activation remains disabled.
- [PR #116](https://github.com/Gianciosvaldo/pokerogue-3ds/pull/116): Task 61 first Fire Stone/Griseous Core lifecycle slice, not all evolution/form families.
- [PR #114](https://github.com/Gianciosvaldo/pokerogue-3ds/pull/114) and [#113](https://github.com/Gianciosvaldo/pokerogue-3ds/pull/113): Tasks 60/59 healing/PP application and real reward options.
- [PR #107](https://github.com/Gianciosvaldo/pokerogue-3ds/pull/107) and [#104](https://github.com/Gianciosvaldo/pokerogue-3ds/pull/104): merged learnset-input and evolution/application repairs; former draft blockers are historical.
- [PR #139](https://github.com/Gianciosvaldo/pokerogue-3ds/pull/139) (commit [`639a086`](https://github.com/Gianciosvaldo/pokerogue-3ds/commit/639a086)): Task 78 / Card 78 (Starter unlocks, genderless species, and cost enforcement in `game-core`).
- [PR #136](https://github.com/Gianciosvaldo/pokerogue-3ds/pull/136) (commit [`6fd4cff`](https://github.com/Gianciosvaldo/pokerogue-3ds/commit/6fd4cff)): Task 96, Rare Candy and Rarer Candy slice only (application through `reward-core` in `game-core`).
- [PR #135](https://github.com/Gianciosvaldo/pokerogue-3ds/pull/135) (commit [`102905a`](https://github.com/Gianciosvaldo/pokerogue-3ds/commit/102905a)): Card 114 (Catching animation, full-party release flow in `game/`).
- [PR #134](https://github.com/Gianciosvaldo/pokerogue-3ds/pull/134) (commit [`07b1cfe`](https://github.com/Gianciosvaldo/pokerogue-3ds/commit/07b1cfe)): Card 113 (Title, save slots, continue, game over, clear screens in `game/`).
- [PR #132](https://github.com/Gianciosvaldo/pokerogue-3ds/pull/132) (commit [`705a79e`](https://github.com/Gianciosvaldo/pokerogue-3ds/commit/705a79e)): Card 97 (Trainers' own rewards in `reward-core`).
- [PR #129](https://github.com/Gianciosvaldo/pokerogue-3ds/pull/129) (commit [`c70691b`](https://github.com/Gianciosvaldo/pokerogue-3ds/commit/c70691b)): Card 112 (Reward, shop, biome screens driven by the run in `game/`).
- [PR #128](https://github.com/Gianciosvaldo/pokerogue-3ds/pull/128) (commit [`0ac92b9`](https://github.com/Gianciosvaldo/pokerogue-3ds/commit/0ac92b9)): Task 95 (TM and Memory Mushroom reward application in `game-core`).
- [PR #112](https://github.com/Gianciosvaldo/pokerogue-3ds/pull/112): Task 63 bounded enemy moveset API; supported clean wild constructors are now wired by #145, broader pools and trainer integration remain.
- [PR #127](https://github.com/Gianciosvaldo/pokerogue-3ds/pull/127): Task 74 (Doubles AI command selection in `ai-core`).
- [PR #125](https://github.com/Gianciosvaldo/pokerogue-3ds/pull/125): Task 77 (Fleeing through the run loop in `game-core`).
- [PR #124](https://github.com/Gianciosvaldo/pokerogue-3ds/pull/124): Task 94 (Catching in the run audit against upstream `VictoryPhase` & `AttemptCapturePhase` in `game-core`).
- [PR #123](https://github.com/Gianciosvaldo/pokerogue-3ds/pull/123): Task 73 (Doubles turn engine first slice in `battle-core`).
- [PR #122](https://github.com/Gianciosvaldo/pokerogue-3ds/pull/122): Task 93 (Boss segments in battles in `game-core`).
- [PR #119](https://github.com/Gianciosvaldo/pokerogue-3ds/pull/119): Task 72 (Bounded Dancer dispatch in `battle-core`).
- [PR #115](https://github.com/Gianciosvaldo/pokerogue-3ds/pull/115): Task 71 (Enigma, Lansat, Starf berries in `battle-core`).
- [PR #111](https://github.com/Gianciosvaldo/pokerogue-3ds/pull/111): Task 70 (Held-item theft/removal in `battle-core`).
- [PR #110](https://github.com/Gianciosvaldo/pokerogue-3ds/pull/110): Task 69 (Weather-driven form changes in `battle-core`).
- [PR #108](https://github.com/Gianciosvaldo/pokerogue-3ds/pull/108): Task 67 (Dormant Illusion identity/type helpers in `battle-core`).
- [PR #106](https://github.com/Gianciosvaldo/pokerogue-3ds/pull/106): Task 90 (Battle baseline regression repairs in `battle-core`).
- [PR #102](https://github.com/Gianciosvaldo/pokerogue-3ds/pull/102): Task 88 (AI matchup differential diagnosis in `ai-core`).
- [PR #101](https://github.com/Gianciosvaldo/pokerogue-3ds/pull/101): Task 87 (Expanded save/no-save divergence repair in `game-core`).
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
4. **Task 89 / merged [#104](https://github.com/Gianciosvaldo/pokerogue-3ds/pull/104):** bounded evolution requests and application repairs landed. The former 135 application failures and unrecorded status draw were repaired with authorization; recorded application 12000/12000, bounded evolution state 8000/8000, and Memory 4000/4000. Runtime versions and exact scopes remain in [reward-core README](../reward-core/README.md); those historical Node 24 repair results are not new Node 22 evidence. Complete evolution/form/fusion execution, persisted ownership and broader inventory lifecycle remain separate.
5. **Task 67 / merged [#108](https://github.com/Gianciosvaldo/pokerogue-3ds/pull/108):** dormant identity/type helpers are on main (`cae69338`); recorded 8000/8000 manual/helper comparisons. Loaded AI integration subsequently landed in [#117](https://github.com/Gianciosvaldo/pokerogue-3ds/pull/117). Natural Illusion dispatch remains disabled and unvalidated; no natural-activation pass is implied.
6. **Task 90 / merged [#106](https://github.com/Gianciosvaldo/pokerogue-3ds/pull/106):** Before baseline plain/rich/switch/summon failures 1/7/2/30, after 0/0/0/0, covered 1940/1826/1885/1856. Removes form-based Max immunities absent from the pin, evaluates ability form callbacks and queues enabled Daily NONE passives. Existing producers/comparators/exclusions unchanged; no coverage loss. CI #102 host/3DS pass.
7. **Task 91 / merged [#107](https://github.com/Gianciosvaldo/pokerogue-3ds/pull/107):** actual TM exclusions and relearner inputs are distinct. Recorded dual-input 8000/8000 plus 24,000 TM/RNG comparisons; legacy Memory import repaired and 4000 cases replayed at zero mismatches. The old ValueHolder/no-cases blocker is historical. Reward options and consumable integration landed in #113/#114; they are not still waiting on this PR.
8. **Task 63 / merged [#112](https://github.com/Gianciosvaldo/pokerogue-3ds/pull/112):** bounded species/form/pool generator only. [Validation](../run-core/TASK63-VALIDATION.md) and the [incomplete R checkpoint](../run-core/TASK63-R-REGRESSION.md) distinguish completed commands from pending work; a merge is not evidence that every planned R command ran.
9. **Task 64 / merged [#145](https://github.com/Gianciosvaldo/pokerogue-3ds/pull/145):** first clean wild adapter slice only. All 1000 soaks still use unsupported movesets somewhere. Trainer member RNG boundaries and broader Task 63 pools remain real gates to full Task 64 and Tasks 65/66. The app's 1000-run rare-evolution coverage failure and expanded 5000-run passing check are both retained in [the validation record](../game-core/TASK64-VALIDATION.md).

The next lane C work is the remaining Task 96 item families and the missing bounded prerequisites for full Task 64. Task 78 is already merged; Classic win progression 79 is still pending. Further Task 61 slices, fusion 62, doubles lifecycle 75, weather 76 and later modes/eggs/mystery/system work are not complete. Do not duplicate merged slices or infer whole-run upstream parity from host consistency. Run-core ownership, Node 22 reference policy and human hardware gates remain in force. Codex merge/continuation authorization is recorded in HANDOFF's October 7 update; it does not waive validation or change owner scope.
