# reward-core: the rewards after a wave

Native C port of PokeRogue's player rewards: the reward options after a wave (pools, weights, luck upgrades, rerolls, the Lock Capsule's locked tiers, the copy of the screen after a TM), the fixed rewards some waves give, and next what each reward does when it is picked. Like run-core, it is diff-tested against upstream `pagefaultgames/pokerogue` @ `8555c08`. It reads battle-core's and run-core's RNG, run state and generated tables **read-only**; `make` builds both libraries into reward-core's own build folder.

The API is [`include/pr/reward.h`](include/pr/reward.h). The run loop (game-core) fills a `pr_reward_ctx` from the run before each reward screen: the party as the rolls see it, the player's items, ball counts, unlocks, the arena. A few of its fields are other ports' answers, marked "caller" in the header: `canSetStatus` (battle-core), `canApplyAbility`, the learnable level moves and the level-move list the TM filter leaves out (learnsets, not ported yet), each attack move's types for item spawns. The diff test takes those from upstream, so reward-core is checked given correct inputs.


## Task 89: evolution application state (2026-10-04)

Checked against upstream `8555c08c823b856cbec4eb99ca84ea52a955836d` on
2026-10-03, port base `cdac97d859cd39ef96200570287efb28b77bc287`, Node 22.23.3 /
GCC 13.3.0. The original regression blockers were repaired with the user's authorization; current results below use Node 24.19.0 and preserve the existing assertions.

Evolution items now retain `pr_reward_ctx.evolution_events` rather than returning
success without the upstream `EvolutionPhase` request. Events carry Pokemon ID,
primary/target species, fusion status, item, pre/evolution form keys, evolution
level and last level. Entries follow enqueue order (`unshiftNew` calls); the
caller consumes them in reverse order, clears consumed entries, and resolves the
Pokemon by ID. An eight-entry capacity reports overflow and refuses the ninth
application without overwriting requests. Newly transported IVs, six stats,
ability indices, shiny/variant/luck and fusion attributes remain unchanged at
this application boundary. Regeneration adds the upstream evolution form key to
the generated evolution rows; no generated file was hand-edited.

Explicit item selection/application follows upstream validation even when
`pauseEvolutions` is set; that flag filters offered item generation, which is
unchanged. Fusion gender conditions read fusionGender during application.
**Success means queued, not evolved.** game-core still needs to consume the
request and perform EvolutionPhase/evolve, ability/form/stat/move updates,
dex/starter bookkeeping and save/resume. Form-change item execution and DNA
Splicers full-state lifecycle are not validated by this slice.

The separate producer `tests/evolution-state.producer.test.ts` calls real
EvolutionItemModifierType selection, modifier construction and `scene.addModifier`.
Only the queue boundary is intercepted; any other queued phase or exception
fails. Its C comparator is `build/evolutionstatetest`. Each record compares all
63 named numeric party fields (including species/forms, fusion metadata, ability,
IVs, stats, HP/status, EXP, nature, Tera type, friendship and move/PP data), the
entire test inventory (Leftovers and Healing Charm, including ownership), global
seeded RNG and every evolution request. Upstream battle RNG is asserted unchanged.
Version/count/footer and record ordering are strict: missing data cannot pass.
Custom Pokemon overrides, status-object counters, broader inventories and actual
phase execution are outside this bounded application-stage contract.

Four stones are exercised: Fire/Vulpix, Water/Shellder, Thunder/Pikachu and
Dawn/Snorunt, both primary and fusion evolutions, including gender rejection.

| Seed | Cases / identical | Mismatches | Accepted (Fire/Water/Thunder/Dawn) | Rejected | Fusion requests |
|---|---|---|---|---|---|
| 777 | 2000 / 2000 | 0 | 227/259/255/85 | 273/241/245/415 | 266 |
| 9001 | 2000 / 2000 | 0 | 248/267/260/79 | 252/233/240/421 | 313 |
| 4242 | 2000 / 2000 | 0 | 256/247/230/80 | 244/253/270/420 | 276 |
| 31337 | 2000 / 2000 | 0 | 255/258/272/66 | 245/242/228/434 | 287 |

Total 8000/8000 identical, zero skipped/thrown. Every stone succeeds and rejects
on every seed. Unchanged production reward_apply.c replayed through the new
transport/comparator fails 847/868/831/880 cases respectively; this baseline does
not claim that the old comparator checked these fields. Species and ability
mutations at the C application boundary each yield 2000/2000 mismatches (exit 1).
Deleting the completion footer yields exit 2. Queue capacity/overflow checks pass.


## Task 89 validation repair (2026-10-04)

The user authorized repairing established inputs without removing assertions. Fusion now uses actual form/IV/nature data through battle-core's stat calculator, assigns HP from the upstream pre-fusion maximum and clamps after modifier refresh. The application producer records the real unseeded status-helper draw; it does not replace randomness with a fixed result. C requires that input when both valid statuses can be selected and refuses missing/invalid input before mutation. A sprite-only mock prevents destroyed display fixtures from throwing; upstream item/selection/stat/status logic still runs and any thrown case fails the producer. Form-item selection matches upstream item-trigger lookup without incorrectly requiring phase activation conditions. Hardcore rejects a fainted primary. Memory imports ValueHolder from its actual module and preserves duplicate used-TM history as upstream does.

Node24.19.0/Vitest4.1.11, GCC13.3, upstream `8555c08`: established application **4000/4000 seed99**, plus **2000/2000 each seeds777/9001/4242/31337**, zero failures/thrown. The existing comparisons remain HP/status/PP/results; they do not establish complete fusion state. The separate bounded evolution suite still compares63 named party fields, inventory/RNG/queues: **8000/8000 identical**, zero mismatches/skipped/thrown. Species/ability mutations each fail2000/2000; flipped recorded status draws fail6/4000; missing draw/stat inputs and incomplete evolution footer exit2. Missing/invalid caller status-choice checks preserve the original context and both real choices pass.

The complete established `difftest.sh <upstream> 4000 99 all` now exits0: options3950pass/0fail/50unported, victory4000pass/0fail, apply4000pass/0fail, Memory4000cases/0mismatches. Host test exits0 with paired200-run digests `d1eb301b`/`d1eb301b`. CI must pass on the updated head before ready-for-review transition. Evolution success still means queued; game-core must execute evolution and persist lifecycle state. Custom/stat-modifying held inventories, complete DNA Splicers state/phase/transfer lifecycle, form-change phase execution, upstream whole-run and hardware parity remain unvalidated. No missing differential is credited as passing.

Exact commands (from port unless stated otherwise):

```sh
PATH=/root/.npm/_npx/52027bd8fc0022aa/node_modules/node/bin:$PATH sh reward-core/tools/redump.sh /tmp/pokerogue-upstream
make -s -C reward-core
cp reward-core/tests/evolution-state.producer.test.ts /tmp/pokerogue-upstream/test/tests/zz-dump/
# In /tmp/pokerogue-upstream, for each seed 777, 9001, 4242, 31337:
EVO_N=2000 EVO_SEED=<seed> EVO_OUT=/tmp/task89-evo-<seed>.txt /root/.npm/_npx/52027bd8fc0022aa/node_modules/node/bin/node node_modules/vitest/vitest.mjs run test/tests/zz-dump/evolution-state.producer.test.ts
# Back in port:
reward-core/build/evolutionstatetest /tmp/task89-evo-<seed>.txt
reward-core/build/evolutionstatetest /tmp/task89-evo-777.txt --mutate-species
reward-core/build/evolutionstatetest /tmp/task89-evo-777.txt --mutate-ability
PATH=/root/.npm/_npx/52027bd8fc0022aa/node_modules/node/bin:$PATH BUILD=/tmp/task89-regression sh reward-core/tools/difftest.sh /tmp/pokerogue-upstream 4000 99 all
PATH=/root/.npm/_npx/52027bd8fc0022aa/node_modules/node/bin:$PATH BUILD=/tmp/task89-victory5 sh reward-core/tools/difftest.sh /tmp/pokerogue-upstream 20000 5 victory
# In external upstream after copying fusion-randomness.probe.test.ts there:
/root/.npm/_npx/52027bd8fc0022aa/node_modules/node/bin/node node_modules/vitest/vitest.mjs run test/tests/zz-dump/fusion-randomness.probe.test.ts
```

Each evolution producer reports one test passed. No 3DS/emulator result or full
run evolution parity is claimed. Node 22 follows HANDOFF; upstream's declared
Node >=24.9.0/runtime conflict remains as documented by Task 88.


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

## Card 97: the trainers' own rewards (PR #132, 2026-10-05)

Ported from upstream `8555c08`, in `src/trainer_rewards.c` (API in `include/pr/reward.h`, data dumped into `gen/out/reward_data.c` by `gen/dump-rewards.test.ts`), merged in PR #132 (commit `705a79e`):

| Piece | Upstream | Port |
|---|---|---|
| The reward screen settings of the rival's and the evil team boss's fixed battles, on the victory path (`getFixedBattle(w)?.customModifierRewardSettings`) and the biome path (`isFixedBattle(w) ? ... : undefined`, when a challenge turns the party heal off) | `fixed-battle-configs.ts`, `game-mode.ts`, `victory-phase.ts`, `select-biome-phase.ts` | `pr_reward_fixed_battle_settings`. The settings per mode and wave are dumped (the dump fails on a setting other than `guaranteedModifierTiers`, `allowLuckUpgrades`, `fillRemaining`) |
| Single Generation's `FIXED_BATTLES` hook: which waves it takes over, the boss reward settings it sets, and the `randSeedItem` draws its Elite Four and champion cases make on the run RNG | `challenge.ts` `SingleGenerationChallenge.applyFixedBattle` | Same function; settings dumped per value 1 to 9, draws ported |
| `TrainerVictoryPhase.start()`: `MoneyRewardPhase(moneyMultiplier)`, a `ModifierRewardPhase` per `modifierRewardFuncs` item (rival fights 1, 2, 4), and the boss voucher. `validateVoucher` unlocks the voucher (and counts it) the first time; the voucher item is only queued once it was already unlocked, one tier down unless `getUpgradeUnlockedVouchers()` | `trainer-victory-phase.ts`, `battle-scene.ts` `validateVoucher`, `system/voucher.ts` | `pr_reward_trainer_victory_rewards`. isBoss, the voucher type (upstream's own `vouchers` table), the money multiplier and the reward items are dumped per TrainerType |
| `Trainer.genModifiers(party)`: the seven `genModifiersFunc` closures (Red, Steven, Wallace, Kieran, Guzma's rematch, rival fights 5 and 6), including where upstream throws on a short party | `trainer-config.ts`, `field/trainer.ts` | `pr_reward_trainer_items`. The dump fails if upstream gives another trainer a `genModifiersFunc` |

Not covered here: game-core does not call any of this yet (its `trainer_victory` in `game-core/src/gc_phases.c` guesses the voucher from the money multiplier and queues it on the first win, which is the opposite of upstream, and its reward screens pass no fixed battle settings); run-core's `pr_run_generate_enemy_items` still refuses the seven trainers with a `genModifiersFunc` (merging their items with the random ones is run-core's). The victory message (`randSeedItem` under `executeWithSeedOffset`, in a UI callback), the Breeders in Space achievement and timed events' fixed battle rewards are not ported.

### How it is checked
`tools/trainerdiff.sh <upstream> 2000 <seed>` (refuses to run unless `node --version` is v22) copies `tests/trainer-rewards.producer.test.ts` into the upstream checkout and replays its cases through `build/trainerrewardstest`. Three kinds of case, all on upstream's code:
- **F**, fixed battles: random mode, Single Generation value (a third of the cases are challenge runs, two thirds of those with it on), wave (two thirds on the fixed battle waves), offsetGym, path, run RNG. The victory path runs `VictoryPhase.start()` with the phase queue stubbed; the biome path evaluates select-biome-phase's expression on upstream's GameMode. Compared: every queued item in order, every field of the settings (a setting the port has no field for is a mismatch), the RNG state after.
- **V**, `TrainerVictoryPhase.start()` with a real trainer config, a random voucher unlock state and upgrade flag: every queued phase in order with its argument (any phase the port doesn't make is a mismatch), whether the voucher is unlocked after, the four voucher count changes, the RNG state.
- **G**, `Trainer.genModifiers(party)` on 0 to 6 real `EnemyPokemon` (random species, half the time the closure's species at its slot; random ability index, fusions, passives): every item, its species stat booster key, holder and stack, or that upstream threw; the RNG state.

The inputs are what upstream had before the call (RNG state, unlock state, each enemy's species, `getAbility(true)` and passive), never derived from the answers. An exception outside the call a G case tests fails the producer; the file's last line counts the cases, so a cut file exits 2. The comparator exits 3 when a family went unexercised.

Node v22.22.0, upstream `8555c08`:

| Seed | Cases | Mismatches | F / V / G cases | F with settings (victory / biome), Single Generation cases that drew | V: reward funcs (RIVAL / 2 / 4), vouchers plain / upgraded, unlocked now | G: cases with items (RED STEVEN WALLACE KIERAN GUZMA_2 RIVAL_5 RIVAL_6), threw |
|---|---|---|---|---|---|---|
| 777 | 2,000 | 0 | 808 / 598 / 594 | 74 / 36, 23 | 39 / 48 / 43, 96 / 89, 208 | 57 72 72 10 6 15 13, 13 |
| 9001 | 2,000 | 0 | 811 / 614 / 575 | 89 / 50, 17 | 46 / 44 / 52, 106 / 92, 213 | 62 62 45 10 10 18 15, 15 |
| 4242 | 2,000 | 0 | 842 / 577 / 581 | 89 / 44, 15 | 45 / 46 / 45, 100 / 74, 192 | 68 75 60 8 11 13 19, 13 |
| 31337 | 2,000 | 0 | 830 / 567 / 603 | 75 / 37, 21 | 46 / 46 / 48, 98 / 88, 187 | 60 65 80 10 11 15 14, 10 |

Every settings wave (25, 55, 95, 115, 145, 165, 195) has cases on every seed. Planted bugs, `tools/trainerplant.sh <trace>` (one line of `src/trainer_rewards.c` changed per copy), total mismatching cases on seeds 777 / 9001 / 4242 / 31337, every run exit 1:

| Planted bug | Mismatches |
|---|---|
| Voucher unlock check inverted | 393 / 411 / 366 / 373 |
| Passive ability ignored | 4 / 6 / 7 / 10 |
| Champion hook draws 5 instead of 6 | 3 / 3 / 4 / 4 |
| Rival's bird: Guts on any species | 8 / 4 / 3 / 5 |
| Fixed battles' `allowLuckUpgrades` always true | 110 / 139 / 133 / 112 |
| Money multiplier always 1 | 549 / 559 / 528 / 511 |

## Not done yet
- Wiring card 97's trainer rewards into game-core and the trainers' own items into run-core's enemy items (see Card 97).
- What each reward does when it is picked or used (healing, revives, PP, TMs, evolution items, vitamins, Rare Candy, held items into the battle's modifier list, lures, charms, exp items). Next in this folder.
- The Moveset Randomizer's TM shuffle (reported as `PR_REWARD_UNPORTED_CHALLENGE`), timed events' fixed battle rewards, and the dev override `ITEM_REWARD_OVERRIDE`.
- Rare items (form change items' effects, DNA Splicers, Mega and Tera) wait for M2, as the beta plan says; they already appear in the options.

## Build and test
```
make                                   # build/libreward.a, build/rewardstest, build/victorytest
tools/redump.sh <upstream>             # regenerate gen/out/ from upstream
tools/difftest.sh <upstream> 4000 99   # rewards and victory cases, replayed through the port
tools/trainerdiff.sh <upstream> 2000 777   # card 97: trainer rewards (Node 22 only)
tools/trainerplant.sh <trace>          # card 97: six planted bugs replayed on a trace; each must fail
```
The upstream checkout is the one `battle-core/tools/setup-upstream.sh` prepares. To look into a mismatch: rerun the producer with `DIFF_ONLY=<case>` (it prints every pool entry's weight and every generator result for that case to stderr), and replay with `DEBUG_CASE=<case>` against a build made with `CFLAGS="-O0 -g -std=c99 -DPR_REWARD_DEBUG"`, which prints the port's.

The following Task91 results describe its separate prerequisite PR#107. This branch incorporates that API; combined replay still passes12000 applications,8000 evolution-state cases and8000 dual-input cases with zero mismatches. Merge#107 before#104 to keep each task's review/merge separate.

## Task 91: Separate TM and relearner caller inputs (2026-10-04)

Task 59 cannot supply one accurate `level_moves` array for two upstream contracts. TM generation excludes `getLevelMoves({includeEvolutionMoves:true, includeRelearnerMoves:true})`; Memory Mushroom reads `startingLevel:1` plus pre-evolution moves. `pr_reward_mon` now accepts a separate, explicitly selected relearn list with level/source metadata. Explicit mode excludes the base moveset, as upstream does even under summon moveset overrides, and permits an empty list. Legacy callers retain their original input path. Used-TM history duplicates survive filtering against the earlier level/egg rows, matching actual upstream behavior.

An additive producer invokes both actual upstream getters, `getLearnableLevelMoves()` and all three TM generators in the same case. C compares the resulting Memory rows and each TM/RNG result. Node 22.23.3 / Vitest 4.1.11, GCC 13.3, upstream `8555c08c823b856cbec4eb99ca84ea52a955836d`:

| Seed | Identical / total | Mismatches | Distinct lists | Fusion | Summon moveset overrides |
| --- | --- | --- | --- | --- | --- |
| 777 | 2000 / 2000 | 0 | 322 | 978 | 995 |
| 9001 | 2000 / 2000 | 0 | 324 | 974 | 992 |
| 4242 | 2000 / 2000 | 0 | 293 | 990 | 1021 |
| 31337 | 2000 / 2000 | 0 | 284 | 961 | 998 |

8000 cases, 24,000 TM/RNG comparisons, zero thrown/skipped cases. A planted return to the shared-list contract fails 945/2000 cases; truncation is rejected (exit 2). Three host-only boundary checks cover explicit empty, legacy fallback and oversized input; the upstream random cases produced zero empty raw relearn lists.

```sh
# Copy the new producer into the external pinned checkout's test/tests/zz-dump/:
DIFF_N=2000 DIFF_SEED=777 DIFF_OUT=/abs/dual-777.txt \
  node node_modules/vitest/vitest.mjs run test/tests/zz-dump/learnset-inputs.producer.test.ts
# Repeat for 9001, 4242, 31337, then in the port:
make -C reward-core
reward-core/build/learnsetinputtest /abs/dual-777.txt
reward-core/build/learnsetinputtest /abs/dual-777.txt --mutate-shared-list
reward-core/build/learnsetinputtest --selftest
```

Established seed-99 / N=4000 producers and unchanged comparators were run before/after on the same traces: reward options **3950 pass / 0 fail / 50 unported**, victory **4000 pass / 0 fail**, application **3865 pass / 135 fail**. Each before/after output is byte-identical; application failures are the existing fusion gap, not passing validation.


## Task 91 validation repair (2026-10-04)

The user's authorization resolved the established Memory producer blocker: ValueHolder now imports from `#utils/value-holder`, its actual pinned upstream module. No assertion, case count, seed or comparison was removed. Fresh seed99/N4000 Memory replay: **4000 cases, 0 mismatches**. The separate dual-input suite was rerun on Node24.19.0/Vitest4.1.11 at upstream `8555c08`: four seeds ×2000 = **8000 identical, zero Memory/TM/RNG mismatches**, with 24,000 TM/RNG comparisons; explicit-empty/bounds checks pass, shared-list mutation fails, incomplete trace exits2.

Established seed99/N4000 options remain3950pass/0fail/50unported, victory4000pass/0fail, application3865pass/135fail (unchanged existing fusion gap, addressed separately in Task89/#104). Task91's pass gate requires no worsening of that established application baseline; those135failures are never credited as passes. Host test exits0, paired200-run digests `d1eb301b`/`d1eb301b`. CI must pass on the updated head before ready-for-review transition. Integration Task59 requires this API to merge on main; Task60 still requires Task59 and its documented prerequisites. No full reward/run/hardware lifecycle claim.


