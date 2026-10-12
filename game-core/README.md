# game-core: the run between the battles

A headless port of upstream PokeRogue's run loop (upstream 8555c08): a new run and its starters, each
wave's encounter, the battle's commands, victory, experience, level ups, new moves, evolution, money,
rewards, biome changes and game over. It has no graphics, sound or file access. The 3DS app shows its
events and screens and hands back the player's choices.

It calls battle-core for the turns, run-core for the waves and the enemies, ai-core for the enemy's
commands (and the player's command restrictions and trapping), and goes through `include/gc/ports.h`
for the enemy movesets and the rewards. The reward screens are reward-core's (`src/gc_reward.c`, Task 59).
Healing, revive, PP restore and PP Up rewards act on the run through reward-core (Task 60), and so do Rare Candy and
Rarer Candy (Task 96). Bounded Nature Mint application and save compatibility are
documented in [MINT-VALIDATION.md](MINT-VALIDATION.md). Until run-core's
remaining movesets land and the other rewards act on the run, stand-ins in `src/gc_stubs.c` keep a run
playable. Each stand-in is counted, so the soak runs show how much of a run is still standing in.

Golden Punch loaded-inventory and double-wallet transport, ordered Amulet Coin
dependencies and bounded command/save proof are documented in
[GOLDEN-VALIDATION.md](GOLDEN-VALIDATION.md).

Toxic Orb and Flame Orb inventory mapping, full-turn comparisons and save
boundaries are documented in [ORB-VALIDATION.md](ORB-VALIDATION.md).

Vitamin application, the pinned purchase-error behavior and remaining guarded
paths are documented in [VITAMIN-VALIDATION.md](VITAMIN-VALIDATION.md).

## The API (`include/gc/run.h`)

```c
static gc_run run;                            /* about 300 KB: keep it static or on the heap */
gc_run_new(&run, GAME_MODES_CLASSIC, seed, entropy);   /* waits on GC_SCREEN_STARTERS */
gc_run_set_starters(&run, starters, n, &why);          /* runs to the first screen of wave 1 */
for (;;) {
    gc_event ev;
    while (gc_run_next_event(&run, &ev)) show(&ev);    /* messages, a turn's battle events, exp,
                                                          level ups, money, waves... */
    const gc_screen *s = gc_run_screen(&run);          /* what the player must answer, and what's legal */
    if (s->kind == GC_SCREEN_GAME_OVER || s->kind == GC_SCREEN_CLEAR) break;
    gc_run_choose(&run, &choice, &why);                /* false + why when it isn't legal */
}
```

Screens: starters, command (Fight / Ball / Pokémon; Run comes with fleeing), switch (also the party-full prompt of a catch), check switch
("Will you switch?", only with `run->ask_switch`), learn move,
evolve, reward (options, shop, reroll, skip), biome (with a Map), game over, clear. The app may read the
party, money, wave, biome and the battle's field straight from `gc_run`, but must not change them.
`party[]` never moves: an index in an event or a choice always means the same Pokémon, and
`party_order[]` is upstream's order for the party screen.

Messages are battle-core's `pr_msg` (`battle-core/include/pr/text.h`): upstream's i18n key
(`battle:expGain`) with its named arguments (`pokemonName`, `exp`), fixed when the message is made, so
`pr_msg_format` gives the text upstream shows. A turn's own moves, damage, faints, switches and texts are
battle-core's `pr_event` list (`pr/events.h`), handed over as one `GC_EV_BATTLE` event that points into
`run->battle_events` (valid until the next `gc_run_choose`). The app plays both the same way. Some of
upstream's texts carry its control codes (`@d{15}` waits, `@s{se/pb_bounce_1}` plays a sound, as in
the "1, 2, and… Poof!" of a forgotten move): the app acts on them rather than printing them.

### A turn, from its start to the next command

`gc_run_choose` on a Fight runs battle-core's turn (`pr_run_turn`) up to its next stop. When an enemy
faints, battle-core stops at the VictoryPhase with the rest of the turn still queued; game-core gives out the
experience (`RP_VICTORY`, `RP_EXP`, level ups, new moves, evolutions) and then lets the engine carry on
(`RP_CONTINUE`, battle-core's `pr_continue`): the other moves, the turn's end-of-turn effects (burn damage,
weather, berries), and, at a trainer's, the next Pokémon, which battle-core's own SwitchSummonPhase sends out
with the slot `Trainer.getNextSummonIndex` gives (ai-core's `pr_ai_next_summon_index`, through the
`next_summon_index` callback). When no enemy is left, `pr_continue` ends in the BattleEndPhase stop and the
wave's chain (trainer victory, money, rewards, the biome, the next battle) follows. A fainted player lead gets
the replacement prompt (a SWITCH screen that can't be skipped) after the trainer's next Pokémon is out. The
enemy's own switch-out (ai-core's COMMAND_POKEMON) is left in the battle's commands and battle-core's
TurnStartPhase runs it before the moves.

A wild battle's start (battle-core's `pr_start_battle`) stops at CheckSwitchPhase only in the SWITCH battle
style. game-core's default is the SET style (`run->ask_switch = false`: no question, the start is carried
through at once, the enemy's and the player's entry phases included). With `run->ask_switch = true` (a setting
the app sets after `gc_run_new` and `gc_run_load`, not saved) the run waits on a `GC_SCREEN_CHECK_SWITCH`
screen, answered with `GC_CH_CONFIRM` (`a` = 1 yes, 0 no); yes opens the SWITCH screen (which can be
cancelled), no goes on to the first command. Pinned upstream asks nothing at a trainer's next Pokémon:
`CheckSwitchPhase` is pushed only for a battle that isn't a trainer's (`encounter-phase.ts`), the title's
continue, a mystery encounter and the game over's retry.

### Boss segments

An enemy that is a boss has battle-core's `pr_pokemon.boss_segments` (run-core's count, from
`getEncounterBossSegments`: legendary, sub-legendary and mythical species, every tenth wave, the endless
random bosses, +1 at level 100 and +1 at a base stat total of 670) and `boss_segment_index`, the shields
left, one under the count to start with, which is upstream's `setBoss`. `gc_mon_new_enemy` sets them from
the generator's `boss_segments` (0 = not a boss), a mystery encounter's boss from the encounter's own count,
and battle-core does the rest: a hit stops at the end of the next segment (a big one passes several), each
broken segment gives a wild boss a random stat boost (a trainer's Pokémon keeps the shield and gets no boost),
and the classic final boss's first-segment rule (wave 200, form 0) is battle-core's too; Task 66 owns the rest of that fight. The app reads the two fields on the Pokémon it
converts; game-core keeps no copy of them. They are saved with the rest of `pr_pokemon` (`is_boss`,
`boss_segments`, `boss_segment_index`). A caught boss comes into the party as a `PlayerPokemon`, which is
never a boss: its shields are cleared and it loses the passive a boss has by being one.

Only wild bosses and mystery encounters' bosses happen in a run today: run-core gives segments to a trainer's
Pokémon only in the classic rival's fights 5 and 6, and those waves are still wild stand-ins (fixed battles,
below). `tests/boss_segments_test.c` runs the rival's party through `gc_mon_new_enemy` for that reason.

### Catching

A Ball command (`GC_CH_BALL`, `a` = PokeballType) first asks `gc_catch_ball_refusal` (`src/gc_catch.c`) what upstream's
`CommandPhase.checkCanUseBall` and `handleBallCommand` ask: a trainer's battle, a mystery encounter that
doesn't allow it, the final biome's forced-catch and final-boss rules (classic, Fresh Start, endless, daily),
and a boss that still has a shield (it takes only a Master Ball, and the classic final boss takes none in a run with
challenges).
A refusal is the locale key upstream shows (`battle:noPokeballStrong`...). The roll and the ball counts are
battle-core's `AttemptCapturePhase`; when it stops at its VictoryPhase with the enemy still standing, the run
queues `RP_CATCH` ahead of the run's own victory, which does what `AttemptCapturePhase.catch()` does after the roll:
`updateSpeciesDexIvs(getRootSpeciesId(true))` and `setPokemonCaught` (the dex, the species' candy, ability and
nature, the "added as a starter" message for each newly unlocked species, in a Daily Run candy only for a catch
that unlocks something), then either `addToParty` (the Pokémon takes the ball, `metLevel`, `metBiome`, `metWave`
and `metSpecies`, joins as a `PlayerPokemon`, so a caught boss loses its shields and passive, and the enemy's held
items become the player's) or, with six in the party, the prompt: a SWITCH screen with `can_skip`, where
`GC_CH_PICK` of a member releases it (the farewell text, its held items go with it, the new Pokémon takes its
place, in upstream's order including the splice quirk of releasing the last slot) and `GC_CH_PICK` -1 is "No". Either way
the enemy is removed (HP 0, fainted, its items cleared) and the run's victory follows.

Checked by `make test` (`catch_test`: the rules, every ball type, a full party, a caught boss, the dex, candy and
items, a save and resume with the new member) and against upstream by two differentials that need an upstream
checkout at the pinned commit and Node 22: `tools/difftest.sh <upstream>` (the dex side of a catch, 1500 catches
against `GameData.setPokemonCaught`, the starter messages included) and `tools/catchdiff.sh <upstream> [cases]
[rule states] [seed]` (3000 states of the ball rules against the real `handleBallCommand`, and wild battles with a
ball thrown through the real command UI, a full party answered through the real option menu and party screen:
the party, items, ball counts, dex, candy, messages and RNG compared field by field). The upstream test harness
skips the shake rolls (every throw catches), so the producer only makes throws the formula guarantees (a Master
Ball, or a modified catch rate of 255 or more); the roll itself is battle-core's differential. It also leaves out
waves that are a multiple of 10: the harness stops before the boss wave's rewards, which card 93 covers.

## Saving

A run is saved where upstream saves it: at the start of each encounter (`GC_EV_SAVE_POINT`), which in
game-core means from that event until the battle's first turn runs, while the first COMMAND screen waits
(`gc_run_can_save`). `gc_run_save` fills a buffer of at most `GC_SAVE_MAX` (128 KB; about 7 to 9 KB in
practice), and `gc_run_load` comes back to that COMMAND screen with the run exactly as it was: the
party with its summon data (a lead that stays out between waves keeps its stat stages and effects), the
enemy party, the items, the arena, the money, the seeds and the RNG states. What follows a load is what
would have followed without it: the soak runs check that at every save point (`--save-test`).

The format outlives changes to the structs it comes from. Each value is stored under its field's name and
size; a build loads each field it knows from a saved field with the same name and size, keeps the
default for the others and counts them in `run->load_dropped`. A damaged or cut save (CRC-32) or one
in a newer format is refused with a reason. Upstream's own reload differs in one way: it plays the
encounter's summon phases again; game-core has no battle-start summon phases yet (see the stand-ins).

`make test` runs the save checks (`tools/savetest.c`: round trip, damaged, cut, newer and older saves)
and then 200 soak runs with and without a save/load at every save point, whose digests must match.

## Building and playing on a PC

```sh
make                                        # builds run-core and battle-core's libraries too
./build/gcplay --starters BULBASAUR,CHARMANDER,SQUIRTLE     # play by typing the choices
./build/gcplay --auto 1000 --jobs 8                          # seeded soak runs
./build/gcplay --auto 1 --first 42 --verbose                 # replay soak run 42 with every event
./build/gcplay --auto 1 --first 42 --verbose --locales <upstream>/locales/en   # as English text
./build/gcplay --auto 1000 --jobs 8 --save-test              # save and load back at every save point
make test                                                    # the save checks and the digest comparison
```

The soak runs play a random team of starters within the cost budget with a plain player of their own
(their own RNG, seeded by run number): the strongest move by power, STAB and type, switch out when
low, heal the hurt and revive the fainted from the rewards, Rare Candy on the lowest level, potions
from the shop, learn a move when it is stronger, always evolve. 15% of its answers are random legal
ones, so the runs stay varied. Each run is its own process, so a crash or a hang is reported with the
seed that caused it. The report lists where runs ended, game-core's and run-core's stand-ins, the
battle texts battle-core hasn't ported yet (by source line) and battle-core's unported effects, each
by how many runs hit it.

## On the 3DS

The game app (`game/Makefile`) compiles `src/` and `gen/out/` with the other cores' sources into one
build folder, where objects are named by file name only, so every source file here starts with `gc_`
(a `phases.c` or `save.c` would clash with battle-core's and the app's own). Checked with
arm-none-eabi-gcc 13 and the app's flags (`-march=armv6k -mfloat-abi=hard -std=gnu11 -Wall`, plus
`-Wextra`): no warnings, about 106 KB of code and read-only tables. A host build of all the cores with `-funsigned-char`
(ARM's `char`) gives the same soak digest as the normal one.

## What upstream does that game-core doesn't do yet

Stand-ins (counted in `run->stubs`, named in `gc_stub_name`):

| Stand-in | Waits on |
|---|---|
| Enemy moveset: the last four level moves for unsupported constructors. Task 64's first slice calls Task 63 for supported clean wild enemies; trainer streams and broader pools still use the counted fallback | run-core |
| A reward taken that isn't a ball, Full Heal, Rare Candy, Rarer Candy (Task 96 / PR #136), held item or one of the healing, revive and PP items Task 60/96 ported (TMs, Memory Mushroom, Mints, money, evolution items, DNA Splicers) is added to the items and counted (`reward taken without its own effect`) | later reward tasks |
| Double battles are played as singles (M1 rule) | doubles in battle-core |
| Mystery encounters and fixed classic battles (rival, evil team, gyms run-core can't make) become wild battles of the wave's level | run-core, later the encounters |
| A boss's battle score: `addFaintedEnemyScore` (and the catch's own copy of it) multiplies it by `sqrt(bossSegments)`; Task 93 sets `boss_segments` on Pokemon encounters and `gc_phases.c` calculates score using `boss_segments` | Task 93 (PR #122) |
| In a wave whose number is a multiple of 10, once every enemy has fainted, upstream's CheckInterludePhase (`onInterlude`) drops the rest of that turn's weather, berry and status-effect phases and flags the TurnEndPhase; battle-core has it unported (`(phase CheckInterludePhase)` in the soak's unported list: 218 of 1000 runs), so those effects run | battle-core: CheckInterludePhase |
| The biome's weather and terrain are set on the arena without the engine's phases: no start animation, and no PostWeatherChange abilities for a Daily Run's per-wave roll with a Pokémon already out; the weather text comes out as game-core's own messages before "A wild ... appeared"; timed-event pools are left out | `gc_arena_new_battle` |
| Evolution conditions on items, held items or the dex aren't met | reward-core, the dex |
| A catch doesn't count `gameStats`, validate achievements (hidden ability, shiny party, legendary...) or run `Pokemon.addToParty`'s form change trigger; the Limited Catch challenge (`POKEMON_ADD_TO_PARTY`) isn't ported | later cards |
| The shake rolls are battle-core's, and by reading they differ from upstream's critical-capture chance (`getCriticalCaptureChance`): battle-core's multiplier is 2.5 in a Daily Run and 0 otherwise, where upstream's grows with the species caught (0 to 2.5) and with a Catching Charm, and battle-core gives any Challenge Mode run 0 where upstream does for the Fresh Start challenge only. Not checked by a differential: the upstream test harness skips the rolls. A caught boss's score has the missing multiplier (the row above) | battle-core |
| A trainer's name in a message goes without its title ("Aaron would like to battle!" for "Ace Trainer Aaron would like to battle!") | battle-core: a message argument made of several locale keys |
| Pickup, Honey Gather, Pay Day, lapsing items | reward-core |

Not started: the dex and starter unlocks, eggs, Imprison and the
restriction texts of their own (a restricted move answers `battle:moveCannotBeSelected` until
battle-core's `pr_move_usable` lands), the trainer config's boss flag and Terastallization for the fixed
battles' trainers, form learnsets for level-up learning (`gc_level_up_moves`; the TM and Memory Mushroom
lists read them since Task 59), and the prevolution-move messages.

## Data

`gen/out/gc_evolutions.c` (each species' evolutions and their conditions), `gen/out/gc_starters.c`
(starter costs and the default unlocked starters), `gen/out/gc_trainers.c` (each trainer config's
money multiplier, boss flag, reward count and names) and `gen/out/gc_learnsets.c` (every species' form-specific
level moves, `formLevelMoves`: 67 lists, 307 moves) are dumped from upstream. To dump them again from an
upstream checkout prepared by `battle-core/tools/setup-upstream.sh`: `tools/redump.sh <upstream dir>`.

Trainer names are upstream's own `Trainer.getName`, run by the dump on every config and variant for the
calls the game makes (`GC_TNAME_*`: the titled name of the appeared and defeated messages, the send-out
names, the victory speaker) with `i18next.t` swapped for one that returns `$t(key)`. So a name is text
with `$t(key)` segments, i18next's nesting syntax: `$t(titles:gymLeader) $t(trainerNames:brock)`, or
`$t(trainerClasses:aceTrainer) @N` where game-core puts the personal name run-core rolled
(`$t(trainersCommon:aceTrainer.male.3)`). `run->trainer_names` has them. A double-only pair (Twins) has
one name, "Amy & May", which upstream splits for each twin's send-out; here both send-outs name the pair.

## Task 57: held item adapter and consumption persistence

The run now maps LEFTOVERS, SHELL_BELL, FOCUS_BAND, QUICK_CLAW, KINGS_ROCK,
WHITE_HERB and BERRY to their battle-core classes. Berry/White Herb losses are
reconciled to both inventories after battle start and after each turn/switch
continuation, before the inventory can be rebuilt or serialized. Matching retains
owner and berry kind. Entries beyond battle-core's 96-entry capacity are retained;
this PR does not remove that capacity limit or add transfers/new item effects.

Actual upstream `8555c08` construction and `Pokemon.loseHeldItem` were compared
using the new additive producer. Existing producers/comparators are unchanged.

```sh
# From a prepared upstream checkout outside this repo:
ITEM_BRIDGE_OUT=/tmp/itembridge.txt pnpm exec vitest run test/tests/zz-dump/itembridge.producer.test.ts
# From the port after copying game-core/tests/itembridge.producer.test.ts there:
make -C game-core test
game-core/build/itembridge_test /tmp/itembridge.txt
```

Measured 2026-10-03: 648 cases, 648 identical, 0 mismatches; 39 host lifecycle
checks (both sides' Sitrus/Lum/Leppa effects, White Herb, distinct holders/kinds,
repeated synchronization, overflow preservation and legal save/reload). The
standard 200-run test passed: both digests `1d0a6108`; original main's pair was
`46f970df`. Activating previously opaque enemy held items changes run outcomes.

Expanded checks remain **failing the save equality gate**, despite zero hangs,
crashes or timeouts across 1,000 finished runs in each run:

| Source | No-save digest | Save digest | Equality |
|---|---|---|---|
| Main `2a740ab` | `5c084cbb` | `f12eb771` | FAIL |
| Task 57 | `47b699cc` | `ec350294` | FAIL |

Commands: `game-core/build/gcplay --auto 1000 --jobs 8` and the same command with
`--save-test`, before and after. This demonstrates a baseline persistence gap;
it does not prove every changed-branch divergence has the same cause. Task 57
remains a draft until the expanded gate is resolved. The focused comparison
covers modifier identity and item loss, not complete move/turn/reward parity.
The legacy mystery test's reported 76 choices compare no upstream result fields
and are smoke coverage only; its producer/comparison was not changed here.

## Task 87: resume state fixes; expanded gate still blocked

At main `58030d63ef3090b7d69865b66431bb4e8e015656`, reload omitted mystery
history and the active mystery node, replayed trainer summon effects (Intimidate
lowered Attack twice), and reset battle RNG already consumed by Forewarn.
The resume path now restores field pointers without executing battle-start effects,
serializes mystery history/node fields, and preserves the battle RNG and live flag.
The normal new-encounter path is unchanged. Historical saves missing these fields
load with empty history, no active mystery node and an uninitialized battle RNG;
missing historical state cannot be recovered or claimed faithful.

New additive `tests/resume_test.c` uses the existing, unchanged gcplay chooser.
It checks empty/full history, encounter selection and RNG, trainer stat stages,
field ownership and invalid history. Four regression seeds match after loading:

| Seed | Final wave | Turns | Score | Saved/ordinary |
|---|---|---|---|---|
| soak00000235 | 42 | 118 | 1553 | identical |
| soak00000431 | 45 | 121 | 1451 | identical |
| soak00000558 | 14 | 44 | 281 | identical |
| soak00000849 | 14 | 39 | 316 | identical |

Seed 849 exposed another adapter bug: a revived lead sent out through game-core's
own replacement prompt retained `switch_out_status=true`. It was displayed as
active but battle-core correctly treated it as leaving the field, producing
empty turns indefinitely. That send-out now clears the flag, just as the regular
summon path does. The established soak driver is unchanged, including its
200,000-step limit, chooser, seeds and digest. Its limit-exhaustion reporting is
still unreliable in general; an additive completion check therefore independently
requires an observed GAME_OVER/CLEAR screen for every seed 0..999, with and without
saving. It fails on exhausted budgets or rejected chooser actions.

Exact commands from the repository root:

```sh
make -C game-core clean
make -C game-core test
game-core/build/gcplay --auto 1000 --jobs 8
game-core/build/gcplay --auto 1000 --jobs 8 --save-test
```

The completion check observes 1,000/1,000 terminal runs in each mode, maximum
450/472 steps, and the resume regression reports **36,471 checks passed**.
The 1,000-run legacy checks both report 1,000 ended, zero cleared/stuck/crashed/
timed out, **57,502 turns**, digest **`0cba4e49`**.

| Historical 1,000-run build | Ordinary digest / turns | Saved digest / turns | Equality |
|---|---|---|---|
| main 58030d63 | e94dc9ac / 257987 | 42ce52f4 / 257964 | FAIL |
| initial Task 87 slice | e94dc9ac / 257987 | 6917434c / 257973 | FAIL |
| Task 87, replacement flag corrected | 0cba4e49 / 57502 | 0cba4e49 / 57502 | PASS |

The historical failures included the silently exhausted seed 849 and cannot be
credited as completed runs. No existing producer/comparator was changed. The
mystery test's printed “76 differential cases” still represents smoke coverage,
not upstream result comparison. These checks validate deterministic host resume;
no new upstream differential or hardware/emulator result is claimed.

## Task 59: reward screens from reward-core

At main `6ac3e321288ef543517279354bf96d7e5c6e5420`, `gc_port_reward_options` drew from a fixed placeholder pool.
It now builds a `pr_reward_ctx` from the run as it is (the party in upstream's party order, held items with
their holders, unlocks, balls, learnsets, the arena, luck) and calls reward-core's `pr_reward_select_start` on
`run->run.rng` for each way upstream's `SelectModifierPhase` comes up (`src/gc_reward.c`, `GC_REWARD_*` in
`include/gc/ports.h`):

| Screen | What it does |
|---|---|
| first roll | `resetSeed()`, the pool's thresholds, 3 options plus one per Golden Poké Ball (`getModifierCount`) |
| reroll | a new phase with `rerollCount + 1`, no reseed; the previous options' tiers are kept only while the Lock Capsule's lock is on (the luck upgrade can still raise them: `upgrade_count` is in `gc_offer`) |
| copy (after a TM or Memory Mushroom) | `SelectModifierPhase.copy()`: the same options and `rerollCount` as guaranteed options, no luck upgrade, no thresholds, no RNG draw |
| no victory | `{fillRemaining: false, rerollMultiplier: -1}`: no options and no reroll, but it still reseeds and rolls the thresholds |

The reroll price is `getRerollCost` (reward-core's `pr_reward_reroll_cost`). The Lock Capsule's lock is a
screen choice (`GC_CH_LOCK_RARITIES`). A screen reward-core flags or can't roll comes up
empty and is counted (`reward screen reward-core could not roll`), as is an input game-core can't give
(`reward-core input game-core can't give`); both are 0 in 1,000 runs. The learnsets reward-core reads
(`src/gc_learnset.c`, `getLevelMoves` with prevolution and fusion rows, `getUnlockedEggMoves`) use
`gen/out/gc_learnsets.c`, the form-specific level moves dumped by `gen/dump-learnsets.test.ts` through
`tools/redump.sh` (67 lists, 307 moves; running `tools/redump.sh` again on the 8555c08 checkout leaves every
generated file unchanged). Applying a reward was still the placeholder; Task 60 (below) replaced it for healing, revives and PP.

Exact commands from the repository root, with an upstream checkout from `battle-core/tools/setup-upstream.sh`:

```sh
make -C game-core clean
make -C game-core test
game-core/build/gcplay --auto 1000 --jobs 8
game-core/build/gcplay --auto 1000 --jobs 8 --save-test
for seed in 777 9001 4242 31337; do game-core/tools/rewarddiff.sh <upstream dir> 1500 $seed; done
reward-core/tools/difftest.sh <upstream dir> 4000 99 all          # unchanged reward-core regression
```

Results:

- `make -C game-core test` exits 0, no warnings. New: `build/reward_test` **3,512 checks, 0 failures** (hand-built
  screens replayed straight through reward-core: first roll 192 options, 1 luck upgrade; 120 rerolls with and
  without the lock; the reroll price; the copy; the no-victory screen; the phase flow; the inputs) and
  `build/switch_prompt_test` **25 checks, 0 failures**. The 200-run digest is `f12526b9` without and with saves.
- Differential against upstream (`tests/reward.producer.test.ts` runs upstream's own calls, `tests/reward_diff_test.c`
  replays them): each of the four seeds, 1,500 cases, **0 mismatches**, 0 cases skipped. It compares every value
  `gc_reward_ctx` hands reward-core with what upstream returns (about 530,000 input fields per seed) and
  every screen's options, arguments, tiers, upgrade counts, reroll price and RNG state after, for the first roll,
  rerolls with and without the lock, the copy and the no-victory screen (3,530 to 3,645 screens and 11,687 to 12,129
  options per seed). The cases write upstream's own stats over game-core's; stats are not part of this check.
- Mutation check of that differential (one change in the adapter at a time, same 4 case files): 18 of 20 changes
  are caught. The two that aren't: *TM list from level 1* is equivalent (with `includeRelearnerMoves` the
  starting level never changes the rows), and *copy loses rerollCount* is redundant in every flow game-core can
  reach (the count is captured when the copy is queued and nothing rolls a screen in between). Caught: the
  Lock Capsule's tiers ignored, Aura Wheel as one type, egg-move unlock bits, `canSetStatus` without `ignoreField`,
  party luck, time of day, weather, the Memory list not explicit, the reroll price ignoring the lock,
  move-type abilities, the fusion partner in item-spawn types (by a single case in the 6,000), party order, held items not matched to holders,
  passive always usable, Golden Punch holders, Tera Blast without the Tera Orb, the unlocked Eviolite, the balls.
- 1,000 soak runs, with and without saves: 1,000 ended, 0 cleared/stuck/crashed/timed out, **55,280 turns**, digest
  **`bdfe170b`** both (16,151 save/load round trips, largest save 10,198 of 131,072 bytes). A host build of every core with
  `-funsigned-char` gives the same digest. Stand-ins: the placeholder reward list (1,000 runs, 14,047 times on
  main) is gone; the reward screen that couldn't be rolled and the unsupported input are 0; **a reward
  taken without its own effect (Task 60): 831 runs, 1,466 times**.
- Size (host x86-64 objects, the ARM size was not measured): `gc_reward.o`, `gc_learnset.o` and `gc_learnsets.o` add
  about 12 KB of code and tables and 34.8 KB of static memory (the reward context and the learnset scratch rows);
  `gc_run` is 310,608 bytes and its `internal_` still holds `gc_internal` (12,064 of 16,384 bytes).
- reward-core's own established regression, unchanged: rewards **3950 pass, 0 fail, 50 unported**, victory 4000/0,
  application 4000/0, Memory 4000 cases/0 mismatches.

Why the soak digest moved from main's `dd076d77` (57,502 turns): the runs now play real reward screens, so
potions, balls, held items and Rare Candies differ from the first reward on, and two fixes change the runs they
touch (a starter's luck is `shiny ? variant + 1 : 0`, as `Pokemon`'s constructor sets it, and the Teleport
prompt below). I did not split the change by cause. The unported counter for held items on the turn rose
(886 runs and 34,445 times to 999 runs and 51,528): real held items now reach battles, and battle-core hasn't
ported all of them.

Two game-core fixes the real rewards forced:

- A Pokémon's own move that switches it out (Teleport) offered the Pokémon that was leaving; picking it left
  `switch_out_status` set and the battle never went on (soak seed 782). `gc_battle_switch_screen` leaves it out.
- The four seeds of `tests/resume_test.c` play different runs now, so they no longer reach what they were chosen
  for: with Task 87's `switch_out_status = false` in the replacement prompt removed, `resume_test` still passed
  (Task 87 added seed 849 for exactly that). That is a loss of coverage by the same, unchanged check, so `tests/switch_prompt_test.c`
  pins both prompts directly; removing either fix makes it fail. `resume_test` itself is unchanged: its counts
  (36,417 checks, max steps 250/296; main 36,471 and 450/472) follow the runs it now plays.

Not done or not checked (reported, not widened into this PR):

- `pr_reward_reroll` keeps `sel->custom` into the rerolled phase; the adapter uses reward-core's primitives instead.
- battle-core's `pr_apply_variable_move_type` ignores the fusion partner for Judgment, Multi-Attack, Techno
  Blast, Raging Bull and Ivy Cudgel; the adapter works around it (`with_partner_as_primary`, which also copies
  upstream's quirk of reading the partner species' own form 0).
- Nothing applies a TM or the Memory Mushroom yet, so the copy screen is unreachable from a real run and
  `usedTMs` stays empty.
- Fusion gender and luck aren't tracked (reward-core's item-evolution gender condition uses fusion gender).
  Lure and Silver Poké Ball battle counts aren't tracked either (the differential leaves lures out).
  `metSpecies` is derived from the current species' root. Challenges aren't modelled: no level-up moveset hook,
  `EGG_MOVE_RELEARN_AVAILABILITY` and `isAllowedInChallenge` keep their defaults.
- `gc_level_up_moves` (level-up learning) still ignores form learnsets, though the dump now has them.
- The app (`game/`): the Lock Capsule toggle isn't wired into the UI and how it draws the new item keys is
  unchecked. The shop, fixed-battle and mystery encounter custom reward settings, and fleeing (a non-victory
  is always the restricted screen) are untouched.
- No hardware or emulator result; the 3DS build is CI's (no ARM toolchain here).

## Task 60: healing, revive and PP rewards through reward-core

At main (after Task 59, #113), `gc_port_apply_reward` (`src/gc_stubs.c`) applied potions, revives, ethers and
elixirs by hand, accepted a Potion at full HP, ignored the Healing Charm and did no PP Up at all. It now hands the 14 items reward-core ports (Potion, Super, Hyper and Max Potion, Full Restore,
Revive, Max Revive, Sacred Ash, Ether, Max Ether, Elixir, Max Elixir, PP Up, PP Max) to `gc_reward_apply`
(`src/gc_reward.c`), which calls reward-core's `pr_reward_apply` on the run as game-core holds it and writes
back what changed:

- The context is the same `gc_reward_ctx` builds for the screens (party in `party_order`, items and holders);
  the choice's party index is a storage index, mapped to the context's index with `gc_battle_slot_of`.
- Written back: HP, status (and the toxic, sleep and freeze counters when the status clears), and each move's
  PP used and PP Ups. An empty move slot is skipped, as reward-core's moves are compacted. `GC_EV_HP` is emitted
  for every member whose HP changed and for the target.
- A free reward and a purchase differ the way upstream's `SelectModifierPhase.applyModifier` does: a reward at
  cost -1 is consumed whatever `addModifier` returned (only Sacred Ash can reach that with a false result); a
  purchase is taken only if it did something. The party menu's filter (`selectFilter`, `moveSelectFilter`) is
  reward-core's, so a refusal is `partyUiHandler:anyEffect`, and no target is "pick a Pokemon" or "pick a move".
- A move with a fixed maximum PP (`max_pp_override`) refuses PP Up and PP Max, as
  `PokemonPpUpModifierType`'s filter does: reward-core's moves don't carry it, so game-core checks it.
- Left explicit, still the stand-in: Poké Balls, Full Heal (reward-core had no application for it; game-core's
  own rule was HP above 0 and a status; since Task 96 it goes through reward-core, see "Task 96: Full Heal" below),
  Rare Candy (no Candy Jar), held items, and everything that is added to the items as it is. `gcplay` now tallies the 14 items plus Full Heal, taken and refused, so the soak shows what is applied.

Exact commands from the repository root, with an upstream checkout from `battle-core/tools/setup-upstream.sh`:

```sh
make -C game-core clean
make -C game-core test
game-core/build/gcplay --auto 1000 --jobs 8
game-core/build/gcplay --auto 1000 --jobs 8 --save-test
for seed in 777 9001 4242 31337; do game-core/tools/applydiff.sh <upstream dir> 4000 $seed; done
for seed in 777 9001 4242 31337; do game-core/tools/rewarddiff.sh <upstream dir> 1500 $seed; done   # Task 59 regression
reward-core/tools/difftest.sh <upstream dir> 4000 99 all                                          # unchanged reward-core regression
```

Results:

- `make -C game-core test` exits 0, no warnings. New: `build/reward_apply_test`, **638,355 checks, 0 failures**: 58
  table cases (full HP, fainted targets, statuses, empty PP, PP Ups, PP at the 3-Up cap, a move with under 5 PP,
  invalid targets, Healing Charm stacks, free against bought, rejection), Sacred Ash, a shuffled `party_order`,
  the items left unhandled, an empty move slot, `max_pp_override`, and 30,000 random states checked against a
  model written from upstream's `PokemonHpRestoreModifier`, `PokemonPpRestoreModifier`,
  `PokemonPpUpModifier` and `ReviveModifier` rules (14,682 taken, 15,318 refused; each item from 395 to 1,887 taken
  and from 280 to 1,729 refused, listed in the test's output). The 200-run digest is `da825571` without and with saves.
  `build/switch_prompt_test` still 25 checks. `build/resume_test` is unchanged: **36,441 checks**, completion
  1000/1000 both ways, max steps 274/331 (Task 59: 36,417 and 250/296; the runs it plays changed again).
- Differential against upstream, `tools/applydiff.sh`: reward-core's apply producer (`reward-core/tests/apply.producer.test.ts`,
  unchanged, run by upstream's own modifier classes at 8555c08) feeds `tests/reward_apply_diff_test.c`, which replays
  each healing, revive and PP case through `gc_port_apply_reward` on a real run, once free and once bought, at a
  rotated `party_order`. It compares taken or refused, the refusal text, every member's HP, status, move count,
  PP used and PP Ups, and the maximum PP of every move (`getMovePp()`). Four seeds of 4,000 cases:

  | seed | cases replayed (x2: free, bought) | skipped: Hardcore, other items | comparisons | mismatches |
  |---|---|---|---|---|
  | 777 | 2,000 | 670, 1,330 | 4,000 | **0** |
  | 9001 | 1,964 | 706, 1,330 | 3,928 | **0** |
  | 4242 | 2,000 | 670, 1,330 | 4,000 | **0** |
  | 31337 | 2,008 | 662, 1,330 | 4,016 | **0** |

  Seed 777, per item (cases, taken, refused, free and bought together): POTION 143/168/118, SUPER_POTION 141/172/110,
  HYPER_POTION 148/212/84, MAX_POTION 131/142/120, FULL_RESTORE 147/218/76, REVIVE 146/46/246, MAX_REVIVE 143/38/248,
  SACRED_ASH 148/203/93, ETHER 133/180/86, MAX_ETHER 144/178/110, ELIXIR 141/232/50, MAX_ELIXIR 144/252/36,
  PP_UP 148/220/76, PP_MAX 143/190/96; 563 HP values, 109 statuses, 612 PP used and 205 PP Ups changed by free
  rewards; 1,035 cases carry a Healing Charm. Hardcore cases are skipped (game-core models no challenge). The
  trace doesn't carry `max_pp_override`, so that one is covered by the unit test only.
- Mutation checks: the unit test caught 12 of 12 changes to the adapter. The differential caught 7 of 8: HP, status, PP used and PP Up
  not written back, the storage index in place of the party-order index, Sacred Ash taken when bought with no effect,
  a free reward refused; the one it can't see is the `max_pp_override` check, for the reason above. `switch_prompt_test`
  fails when either of Task 87's and Task 59's switch-prompt fixes is removed (re-checked, since the runs changed again).
- 1,000 soak runs, with and without saves: 1,000 ended, 0 cleared/stuck/crashed/timed out, **55,547 turns**, digest
  **`b3bdf746`** both (16,163 save/load round trips, largest save 10,552 of 131,072 bytes). Task 59's `bdfe170b` (55,280 turns)
  moved. The old `gcplay.c` (HEAD's, unchanged digest definition) built against the new library gives `b3bdf746` too, so the
  move is game-core's behaviour, not the tally I added to `gcplay`. The behaviours that differ from the old stand-in: a
  Potion, Ether or Elixir on a Pokémon that can't use it is now refused (the reward screen stays), the Healing Charm
  raises potion healing, PP Up and PP Max do something, and the party menu's filter picks which targets are valid. I
  did not split the digest change by cause. The stand-in counter is **811 runs, 1,330 times**, from 831 and 1,466.
  What it still counts is outside this card: TMs, money rewards (Nugget, Big Nugget, Relic Gold), vouchers, Memory
  Mushroom, Mints, Rarer Candy, evolution items and DNA Splicers; the counter never counted potions, so applying
  them is not what it measures.
- Applied in the soak (runs, taken, refused): POTION 890 runs, 2,439 taken, 285 refused; SUPER_POTION 61, 75, 8;
  FULL_RESTORE 1, 1, 0; FULL_HEAL 9, 9, 52 (game-core's own then); REVIVE 542, 861, 141; MAX_REVIVE 247, 280, 36;
  SACRED_ASH 3, 3, 0; ETHER 14, 14, 58; MAX_ETHER 3, 3, 12; ELIXIR 3, 3, 4; MAX_ELIXIR 2, 2, 1; PP_UP 115, 125, 15;
  PP_MAX 18, 18, 0. HYPER_POTION and MAX_POTION never came up in these 1,000 runs, so only the tests cover them.
- Regressions, unchanged: `rewarddiff.sh` 1,500 cases each on seeds 777, 9001, 4242 and 31337, **0 mismatches**; reward-core
  `difftest.sh` 4,000 cases, seed 99, all: apply 4,000 pass 0 fail, Memory 4,000 cases 0 mismatches, the rest pass.

Not done or not checked (reported, not widened into this PR):

- No Hardcore handling (game-core has no challenges); no Candy Jar for Rare Candy; Full Heal stayed game-core's own (until Task 96, below).
- Enemy movesets are Task 63's, so how often these rewards come up depends on the stand-in moveset.
- The app (`game/`) isn't changed; no hardware or emulator result. The 3DS build is CI's.

### Task 61: bounded reward evolution/form lifecycle

The reward-screen adapter now executes two families from pinned upstream `8555c08`:
Growlithe's Fire Stone evolution into Arcanine, and Giratina's Griseous Core
transition from altered to origin. `gc_reward_lifecycle.c` calls `pr_reward_apply`
for selection/application and owns the subsequent run phases. The stone is
consumed before the cancellable evolution screen; cancellation changes neither
species, stats, moves nor dex and may pause later evolutions. Acceptance uses the
existing evolution/learn-move screens, ability/stat recalculation and dex updates.
The Core is retained with its holder's stable Pokemon ID before the form phase;
form changes update seen/caught records before recalculating stats and preserve
moves/PP. An active Core on Origin Giratina is installed without activating the
reverse rule. A full held Core stack substitutes the PLAYER pool's default ball
reward for the offer tier, retaining the original Core stack and owner.

This is one evolution family and one form-item family, not general evolution/form
parity. Other Fire Stone recipients, fusion evolutions, other form items, manual
activation/deactivation and reversal remain outside this slice; their existing
counted stand-ins remain. The graphical FormChangePhase animation is represented
by an immediate run phase, not a new cancellable form-animation screen. Console
presentation and broad held-item battle behavior are not validated here.

The separately added `tests/reward-lifecycle.producer.test.ts` runs real pinned
selection, `addModifier`, `PlayerPokemon.evolve` / `changeForm`, stat, dex and
`setMove` methods on real Pokemon. Asset/info loading and animation completion
are headless boundaries. Free reward selection completes regardless of
`addModifier`'s Boolean; the test distinguishes ownership from activation. It
compares species/form, level/nature/ability, pause state, HP/stats, moves/order,
PP used/PP Ups, seen/caught/nature/IV dex records, transition queue counts, held
stacks/ownership, substituted balls and global RNG; upstream battle RNG is also
asserted unchanged. Rendered message text is not a differential claim.

```
make -C game-core
NODE=/path/to/node22 game-core/tools/lifecyclediff.sh /external/pokerogue-upstream 280 777
NODE=/path/to/node22 game-core/tools/lifecyclediff.sh /external/pokerogue-upstream 280 9001
NODE=/path/to/node22 game-core/tools/lifecyclediff.sh /external/pokerogue-upstream 280 4242
NODE=/path/to/node22 game-core/tools/lifecyclediff.sh /external/pokerogue-upstream 280 31337
```

`LC_OUT` selects the trace path; producer flags are `LC_N`, `LC_SEED`, `LC_OUT`.
The C comparator is `build/reward_lifecycle_test`. Validation used Node 22.23.3.
Each seed: **280 cases, 280 identical, 0 mismatches**; seven paths each have 40
cases: accepted evolution, cancelled evolution, invalid target, activated form,
Core retained without activation, reward skipped, and full-stack replacement.
`reward_lifecycle_host_test`, included in `make test`, adds **46 checks** for the
public reward/evolve/learn choices, full-moveset replacement, duplicate stacks,
invalid targets and ownership/stats/moves persisted at a legal save boundary.
A temporary mutant retaining altered form produced **40 mismatches**, exit 1;
a trace with the `Z` completion footer removed was rejected with exit 2.

Before/after main `ff35abf`: `make -C game-core test` retained 36,417 resume
checks, 39 item-bridge checks, 3,512 reward-adapter checks, 25 switch checks and
638,355 reward-application checks with zero failures. Its paired 200-run digest
remained `8ee22b13`. Both `gcplay --auto 1000 --jobs 8` and the same command with
`--save-test`: 1,000/1,000 ended, 0 stuck/crashed/timed out, 55,491 turns, digest
`5fd93475`; saved runs made 16,151 round trips (largest save 10,552 bytes).
All printed stub counts were unchanged; these rare families are established by
the focused fixtures, not by a fallback reduction in the random soak. No 3DS
build or console test is claimed.

## Task 96: Rare Candy and Rarer Candy through reward-core

Rare Candy was a hand-made stand-in in `src/gc_stubs.c` (one level, friendship, no Candy Jar) and Rarer Candy was
only added to the items. Both now go through reward-core's `pr_reward_apply_candy` (`reward-core/src/candy.c`, unchanged)
in `gc_reward_apply` (`src/gc_reward.c`), which writes back what changed:

- Levels: `1 + Candy Jar stacks` on the target (Rare Candy, a storage index like the other party items) or on every
  member (Rarer Candy, no target). The level is capped at 255 because battle-core's level is a byte (a Candy Jar
  stack that pushed past it would otherwise wrap to a low level).
- Exp: the new level's total (`getLevelTotalExp`), as upstream does: `getMaxExpLevel(true)` has no limit.
- Friendship: reward-core's: +6 once per candy (the Candy Jar adds levels, not friendship), up to 200, an existing
  friendship above 200 stays.
- The starter's candy progress, through `gc_system_add_friendship`: the candy's 6 (times 3 in Classic), **not** the
  change in the Pokemon's own friendship. Upstream's `addFriendship(6, true)` adds the whole 6 to the starter even when
  the Pokemon is at 199 (own gain 1) or at 200 and above (own gain 0). The first version of this task passed the
  change, which gave the starter 3 instead of 18 at friendship 199 and nothing at 200.
- One `RP_LEVEL_UP` per member that got levels, in party order, old level to new level; the level-up screens, moves and
  evolutions are the existing phases'.
- **No level cap check.** The first version refused a Rare Candy at the wave's level cap and a Rarer Candy when every
  member was at it (`partyUiHandler:anyEffect`, as the old stand-in did). Upstream at 8555c08 has no such check:
  `PokemonLevelIncrementModifierType`'s `selectFilter` always returns null and Rarer Candy has none, and `getMaxExpLevel()`
  only decides whether the exp follows the level. The differential below replays upstream's own candy on members at,
  below and above the cap and every case is applied. The only refusal left is game-core's own: no Pokemon picked
  for a Rare Candy (`pick a Pokemon`).
- Not done: a held Soothe Bell raises `addFriendship`'s amount upstream (`PokemonFriendshipBoosterModifier`); neither
  reward-core nor `gc_system_add_friendship` applies it yet (`src/gc_phases.c` has the same gap for the battle friendship gain).
  Fused Pokemon (a second starter's progress) are not modelled either.
- Full Heal stays in `gc_stubs.c` (until the Full Heal section below); Poké Balls still go to `run->pokeballs`; Mints, vitamins, Pickup, Honey Gather and
  Pay Day stay counted stand-ins (`GC_STUB_REWARD_APPLY` / `GC_STUB_POST_BATTLE`).

Tests: `tests/reward_apply_test.c` has a 17-row candy table (levels, Candy Jars, the cap, friendship 199, 200 and 250,
the starter's progress and its wrap into a candy, Endless, a shuffled party order, the 255 clamp, the refusals,
Rarer Candy), and `test_candy_level_up_save_reload_resume`: a real wave is won, a candy is taken from the reward
screen, the level-up and the Leech Seed prompt run, the next wave's command screen is reached, the run is saved and
loaded, and the turn after plays the same on both. A level-up that is still pending is never saved: `gc_run_can_save`
is false and `gc_run_save` returns 0 while the prompt is open (the save holds no phase queue).

Differential against upstream: `tools/applydiff.sh` also runs `tests/candy.producer.test.ts` (upstream's own
`PokemonLevelIncrementModifier` through `addModifier`, in Classic and Endless, waves 1-199, members below, at and above
the cap, friendship around 195-200 and above, 0-5 Candy Jars) and `tests/reward_apply_diff_test.c` replays it free and
bought, comparing taken or refused, every member's level, exp and friendship, the `LevelUpPhase`s queued and the
starters' friendship progress and candy count. It needs Node 22 and the pinned upstream commit (it refuses otherwise).
Endless waves past about 330 are not in the trace: their level caps are above 255.

## Task 96: Full Heal (and Full Restore's cure) through reward-core

`gc_stubs.c` no longer has a Full Heal of its own. `gc_reward_apply` hands it to reward-core's `pr_reward_apply` like the
other healing items (reward-core side: PR #142).

- **Filled in** (`fill_mon`): `confused` and `nightmare` from `summonData.tags` (`pr_get_battler_tag`), and the three
  status counters from `pr_pokemon`. The tags come through a won wave with the lead: `setup_field` only clears a
  lead's summon data when it is recalled, as upstream's ReturnPhase does, so a confusion still active on the last turn is
  on the Pokemon at the reward screen (`test_full_heal_flow` plays that wave in the real flow).
- **Written back**: status and the three counters (always, from reward-core), the CONFUSED and NIGHTMARE tags a cure ended
  (taken out of `tags`, the rest keep their order; battle-core's own `pr_remove_tag` is not used, its text is the caller's),
  HP, PP. The one HP event for the target is unchanged.
- **Text**: upstream's application calls neither `ui.showText` nor, for a status alone, `queueMessage`. Only a cure that ends a
  confusion queues `battlerTags:confusedOnRemove` ("{{pokemonNameWithAffix}} snapped out of confusion!"); game-core emits it
  as one `GC_EV_MESSAGE` naming the member (through `party_order`, so a shuffled party names the right one). Nothing is shown
  when the reward is picked otherwise, as for the Poke Balls. Nightmare lapses without a text.
- **Rules** (8555c08): Full Heal is refused for a fainted Pokemon or one with no status and no confusion; Full Restore at
  full HP is refused unless it has a status or a confusion; a Nightmare alone is no reason, and it lapses only if the cured
  status was sleep. Counters go back to 0 / -1 / -1 with the status.
- **Known timing difference**: upstream's `queueMessage` is an `unshiftPhase`, so a *bought* Full Heal shows its text after
  the shop screen ends (the phase does not end on a purchase). game-core emits the event at once. A free pick (the usual
  case) matches. There is no message phase in the run queue to defer it with.

Tests: `reward_apply_test` (Full Heal table rows, `test_full_heal`, `test_full_heal_flow`) and `tools/applydiff.sh`, whose apply
section reads reward-core's producer trace as it has been since #142 (status counters, both tags, the queued texts) and now
replays `FULL_HEAL` too. Before this change that section stopped with `malformed trace` on the new format.

## Task 96: bounded Nugget-family adapter

Nugget, Big Nugget and Relic Gold use the reward-core monetary API for supported
party/inventory inputs and consume without ghost storage. Paid/free choices,
capped credit and persistence are covered; Gimmighoul, challenges and malformed
inputs remain counted. See [scope and validation](MONEY-VALIDATION.md).

## Reviver Seed inventory bridge

Existing canonical Seed inventory now reaches battle-core FaintPhase and persists
actual consumption. Detached loot owner 0 stays distinct from wildcard -1 across
rebuild/save. Duplicate/invalid stock remains opaque; acquisition/loot pickup
policy remains separate. See [scope and evidence](SEED-VALIDATION.md).

## Wide Lens inventory bridge

Existing bounded Lens inventory reaches battle-core’s actual hit check with +5
per real/transient stack. Real stocks persist; virtual stocks reset on load as
pinned ModifierData does. Unsupported virtual caller inputs remain counted.
See [scope and validation](LENS-VALIDATION.md).

## Multi Lens inventory bridge

Canonical loaded stacks reach the existing actual hit-count and damage helpers;
real stocks persist and transient stacks reset on load. Malformed inputs stay
opaque. See [scope and validation](MULTI-VALIDATION.md).

## Canonical temporary booster caller

Bounded X-item and Dire Hit acquisition/refresh, full-stack ball replacement,
named timer persistence and actual BattleEnd lapse now call reward-core. Raw
battle helper projection is covered; full turns still flag these modifiers as
unsupported. See [scope and validation](TEMP-VALIDATION.md).

## Canonical Wide Lens acquisition caller

Canonical stock additions reach the existing Lens bridge with source selection,
paid-result and bounded Scene cleanup behavior. See
[scope and validation](WIDE-ACQUISITION-VALIDATION.md).

## Canonical Multi Lens acquisition caller

Cap-two additions now reach the inventory bridge through bounded source selection,
paid-result and Scene cleanup behavior. See
[scope and validation](MULTI-ACQUISITION-VALIDATION.md).

## Classic terminal progression

Bounded Classic terminal victories now award team-level session totals, root
counts, species-line ribbons, unlocks and vouchers. A named terminal marker
prevents duplicate awards across reloads. This proves actual source terminal
bookkeeping with legal fixture outcomes; whole-run clears remain gated. See
[scope and validation](CLASSIC-WIN-VALIDATION.md).

## Endgame diagnostic chooser

The separate host `gcstress` tool starts a documented strong fixture at wave 1
and drives actual game screens and choices to report late-game deaths, refusals
and stand-ins. It leaves the legacy chooser and digest unchanged. Its 100-run
paired diagnostic reaches wave 200 and identifies final-boss blockers, with no
clear claim. See [scope and validation](ENDGAME-DIAGNOSTIC-VALIDATION.md).

## Offline egg generation and voucher gacha

A bounded profile API now purchases and generates eggs with explicit entropy,
scene RNG and offline time inputs, including guarantees, pity, machine effects,
statistics and named-save metadata. This is a partial Task 80 slice; screens,
rehydration and hatching remain separate. See
[scope and validation](EGG-GACHA-VALIDATION.md).

## Boss voucher

`TrainerVictoryPhase` (`trainer_victory` in `gc_phases.c`) now follows upstream's
`validateVoucher` rule for the bosses that have a voucher. The first win unlocks the
voucher and adds one to the count of its own type (`gc_system_unlock_voucher`: Plus for
a moneyMultiplier below 10, Premium otherwise) and queues no reward. Only a later win,
when the voucher is unlocked already, queues the `ModifierRewardPhase`, and its item is
`[VOUCHER, VOUCHER, VOUCHER_PLUS, VOUCHER_PREMIUM][voucherType]`: a Gym Leader gives an
Egg Voucher, a Champion an Egg Voucher Plus. Running that phase adds one to the count of
the item's type (`AddVoucherModifier.apply`, `gc_port_give_reward`), which is also where
Endless mode's wave-50 Plus and wave-250 Premium vouchers now count (they were counted
when queued). Which trainers have a voucher, its type and the item come from reward-core's
`pr_reward_trainer_victory_rewards` (differential-tested in card 97); bosses without a
voucher, such as the rivals, give nothing of that kind. Without system data (`run->sys`
NULL) nothing unlocks and nothing is given. Not ported, on purpose: the timed event that
upgrades the later-win reward (`getUpgradeUnlockedVouchers`).

A voucher on the reward screen is taken as upstream takes it: `addModifier` runs
`AddVoucherModifier.apply`, which adds one to the count of its type (`gc_port_apply_reward`,
`give_voucher` in `gc_stubs.c`), shows no text, adds nothing to the run's items and is no stand-in
(`GC_STUB_REWARD_APPLY` is not counted). Given by a `ModifierRewardPhase` the text is
`AddVoucherModifierType.name`, "1× Egg Voucher Plus". `tools/voucherapplydiff.sh <upstream dir>
[N] [seed]` replays both on random starting counts of the four voucher types
(`voucher-apply.producer.test.ts`, `build/voucher_apply_diff_test`: counts, taken or not, messages,
run items, text) and `tools/voucherapplyplant.sh <trace>` shows ten one-line breakages fail it.

The trainer's own reward items (the config's `modifierRewardFuncs`: the rivals' Exp. Share,
Super Exp. Charm and Tera Orb) are queued before the voucher, one `ModifierRewardPhase`
each, in the order `pr_reward_trainer_victory_rewards` returns them, as upstream unshifts
them. Running one goes through `gc_port_give_reward`, which does what
`PersistentModifier.add` does: it stacks onto the player's item and refuses to go past
`getMaxStackCount()`. A refused modifier is not just dropped: `addModifier` queues
`battle:itemStackFull` and adds the default modifier type of the type's tier instead. A
reward phase's type has no tier, so that is the COMMON pool's first entry, 5 Poke Balls
(`gc_port_give_reward` returns that item, `gc_reward_gain` shows "You received ..." and then
"The stack for ... is full. You will receive 5× Poké Ball instead."; the message comes second
because upstream's queued `MessagePhase` runs after the reward phase, read in the source, the
replay can't see the order). The Map, Mega Bracelet,
Dynamax Band and Tera Orb are the exception: their modifiers have no `match()`, so
upstream adds each one as a modifier of its own with no cap, and here the stack just grows.
Mystery encounter rewards (`gc_mystery.c`) call the same function and ignore the message.
Timed events are not ported on purpose (lorenzo, 2026-10-12): no event calendar and no event
rule, such as the Plus voucher for later wins or the wave-115 Mega Bracelet and Dynamax Band.
`GC_STUB_TRAINER_REWARDS` is counted only if reward-core has no row for a reward the config
lists (none does today). The rivals themselves are still wild stand-ins in run-core's
encounters, so these rewards are only reachable once run-core produces the fixed battles.

Replay of upstream's own phase: `tools/trainervictorydiff.sh <upstream dir> [N] [seed]`
runs `trainer-victory.producer.test.ts` in the pinned checkout (Node 22) and replays it
through `build/trainer_victory_diff_test`. Every trainerConfigs entry is defeated once
with its voucher unlocked and once without, then random ones up to N, and the voucher
`ModifierRewardPhase`s are run. Compared: the queued phases in order, the voucher reward's
key, the unlock afterwards and the four voucher count changes; for each reward item, the
key, the stack of that item before and after (the player's items keep their stacks from
case to case, so they reach and pass the cap), the "You received" text and, past the cap,
the itemStackFull text, formatted from upstream's locales (`GC_LOCALES`, which the script
sets), and the five Poke Ball counts before and after. `tools/trainervictoryplant.sh
<trace>` replays one trace through fourteen copies of game-core with one line changed each
(the old inverted rule among them); all fourteen must fail. The unit test is
`tests/voucher_egg_test.c`, part of `make test`.

## Mystery encounter actions: upstream harness (card 82) and Department Store Sale

`tools/mysterydiff.sh <upstream 8555c08> [N=2000] [seed]` runs the real encounters' own option code through
`tests/mystery-actions.producer.test.ts` (the game manager starts the encounter; each case changes the run seed, wave
seed and wave and runs the chosen option's `onOptionPhase` exactly as `MysteryEncounterOptionSelectedPhase` does, in
`executeWithSeedOffset(.., getSeedOffset() * 500)`) and replays the trace in `tests/mystery_actions_diff_test.c`
through `gc_mystery_execute` and the shop it opens. Compared per case: the run RNG after the option (must not move),
money, items owned, that the encounter left without a battle, the shop's `customModifierSettings` (the items rolled,
`fillRemaining`, `rerollMultiplier`, `allowLuckUpgrades`, guaranteed tiers and options), the options that shop shows
(key, argument, tier, upgrade count) for four parties (Bulbasaur; Lapras, Gengar, Abra; Mew; Rattata, Magikarp,
Pikachu, Eevee), the RNG after the shop, and that the next wave is queued. The old `gen/dump-mystery-actions.test.ts`
and `tests/mystery_diff_test.c` are untouched and still only smoke coverage.

Department Store Sale, Shady Vitamin Dealer, Lost at Sea and Field Trip are ported and compared. Every other encounter's actions in
`gc_mystery.c` are still the Task 46 stand-ins; the encounter *selection* (`gc_mystery_select`) and the intro checks
are not part of this comparison.

### Shady Vitamin Dealer

The same script also writes `<trace>-shady.txt`, replayed by `tests/mystery_shady_diff_test.c`. Each case builds a party
of random levels, IVs, natures (a fifth with a Mint's custom nature), vitamins already held (up to and at the IV cap) and
HP around the 0.51 boundary, sets the wave and a wallet around both prices, and records what upstream offers
(`option.meetsRequirements()` and each party-screen filter on each Pokemon), then runs the chosen option as the phases
do: `onPreOptionPhase` in `executeWithSeedOffset(.., getSeedOffset())` with `selectPokemonForOption` captured, the party
screen's callback after it returns (traced to run outside any seed offset, so the two `randSeedInt(6)` vitamin rolls use
the run's own RNG), `onOptionPhase` in `(.., * 500)`, `MysteryEncounterRewardsPhase.doEncounterRewardsAndContinue()` (the
phases it queues), `EggLapsePhase.start()` and `onPostOptionPhase` in `(.., * 2000)`. Compared: the options offered and the
Pokemon each filter accepts, the run RNG, money, vitamin entries, every party member's HP, stats, nature and vitamin
stacks after the option and after the post-option phase, the queue (the heal-only shop, then the post-option phase, then
the next wave), the messages with the money paid, and the eggs' wave counters.

Upstream behaviours this reproduces on purpose, all found by the comparison or by reading the pinned code:
- `HealthRatioRequirement` goes through `getHpRatio()`, which rounds to two decimals: 50.5% of the HP already counts as 0.51.
- A vitamin is added whatever the IV cap when the Pokemon has none of that stat yet (`PersistentModifier.add` only checks
  the cap when it stacks), so a stat with IV 0 can hold one stack; an existing stack at the cap is left alone.
- The post-option phase runs after the heal-only shop, so the damage (half the max HP after the vitamins) and the new nature
  land after it. The nature is drawn until it differs from `Pokemon.nature`, the base one, not a Mint's.
- `setEncounterExp(.., 100)` in `onPostOptionPhase` only sets `doEncounterExp`, which `MysteryEncounterRewardsPhase` already
  ran: no experience is ever given (the producer records that no experience phase is queued).
- `applyDamageToPokemon`'s "never faint" floor cannot be reached here (the cheap deal needs more than half the HP), so no
  planted bug on it can be caught; it is kept to follow upstream.
- The party screen is the app's: until it exists, `gc_mystery_execute` takes the first Pokemon the filter accepts when the
  one it is given is refused.

### Lost at Sea

`<trace>-lost.txt` is replayed by `tests/mystery_lost_diff_test.c`. Five parties (Squirtle, Pidgeot, Ditto, Gengar,
Snorlax, Abra; Lapras, Charizard, Mew; Rattata, Ditto, Shedinja; Gliscor, Gholdengo, Miraidon, Magikarp, Zubat, Dragonite;
Bulbasaur) with random levels (half the time the exact level a Surf / Fly level move is learned), IVs, natures, HP (a fifth
fainted), Pokerus, a third fused with another species, Exp. Share / Exp. Balance / Oval Charm and Lucky / Golden Eggs, and
the wave and eggs. Upstream records which Pokemon `CanLearnMoveRequirement.queryParty` accepts for Surf and Fly, each
option's `meetsRequirements()` and the primary Pokemon it picks, then the real `onOptionPhase`, `doEncounterRewardsAndContinue`
with the `PartyExpPhase` really started (every `ExpPhase` / `ShowPartyExpBarPhase` it unshifts: party index and experience)
and `EggLapsePhase.start()`. Compared: the starting stats, the accepted Pokemon, the primary, the options offered, the run RNG
and money, every Pokemon's HP, the experience phases in order with their exact amounts (and the level cap), that the wander
option queues none, and the eggs.

Upstream behaviours reproduced on purpose:
- The option is enabled by a Pokemon that is not fainted, but the primary Pokemon is the first of the whole party that can learn
  the move, fainted or not. A fainted primary gets no experience of its own; the Exp. Share holders still do (one participant).
- "Can learn" is the union of `getLevelMoves()` with its defaults (only the move learned at exactly the current level), the
  species' TM list for its form, the fusion's, and the egg moves of the root of the evolution line (all four, unlocked or not).
- Both learn options give `setEncounterExp(primary, Lapras' base exp, true)`: the wave-scaled value, times the encounter's
  `expMultiplier` (1); no heal shop. The wander option gives nothing and damages every Pokemon not fainted by 25% of its max HP,
  never below 1 HP (this floor is reachable here, unlike in Shady Vitamin Dealer).
- With the pinned data every Pokemon that learns Surf or Fly by level also has the TM, so the level-move term can never decide the
  outcome: a planted bug on it is an equivalent mutant.
- Whether the lead is still on the field when the encounter starts (an `ExpPhase` instead of a `ShowPartyExpBarPhase`, display only)
  is not established by the harness, which starts from a fresh encounter with nobody out; the comparison keeps everyone off the field.

### Field Trip

`<trace>-trip.txt` is replayed by `tests/mystery_trip_diff_test.c`. Four parties (up to six Pokemon) with random levels, IVs,
natures, one to four random moves each (any move of the table, so the move categories are the real ones), HP (a fifth
fainted, and a fainted Pokemon can be picked: the party screen has no filter), Pokerus, Exp. Share / Exp. Balance / Oval Charm
and Lucky / Golden Eggs. Each case runs the chosen option as the phases do: `onPreOptionPhase` with `selectPokemonForOption`
captured, the party screen's callback with the Pokemon picked, the handler of the move picked (`pokemonAndMoveChosen`), the
option phase in `(.., * 500)`, `doEncounterRewardsAndContinue()` with the `PartyExpPhase` really started, the shop's own
`SelectModifierPhase` calls, and `EggLapsePhase.start()`. Compared: the starting stats and level cap, that all three options
are offered, the run RNG (never moves), money, the order of the queued phases (experience first, then the shop), every
experience phase with its exact amount, the shop's settings, the options the reward screen shows (item, argument, tier,
upgrade count) with the RNG after it, and the eggs.

Upstream behaviours reproduced on purpose:
- A move of the asked category (physical / special / status, `move.getMove().category`) is correct: experience 100 for that
  Pokemon alone, then the five fixed items (`guaranteedModifierTypeOptions`, no fill, luck upgrades allowed, no reroll
  override) and no heal shop. Any other move gives 50 to *every* party member as a participant (fainted ones count in the
  participant total, so with an Oval Charm the bonus applies) and the heal-only shop.
- The shop items are `generateModifierTypeOption`s: tier from the player pool (`withTierFromPool`), a stat for the X items.
  `pr_reward_pregen_option` (reward-core) makes them without rolling anything.
- The experience phases are taken out of the queue before the shop is opened: upstream's shop is computed without them applied.
- The app's party/move screen does not exist yet: `gc_mystery_execute(.., b, c)` takes the Pokemon (storage index) and the move
  slot, and reads a value out of range as the first Pokemon and the first move.

## Fleeing: upstream differential (Task 77 / #125)

`tools/fleediff.sh <upstream 8555c08> [N=600] [seed]` runs the real `AttemptRunPhase.start()` on a live battle in
`tests/flee.producer.test.ts` and replays it in `tests/flee_diff_test.c` through a real game-core run
(`gc_run_choose` with Run). Each case sets the game mode, wave, the lead's and the enemy's raw speed, stat stages,
paralysis, abilities (Run Away on either side), the boss shield, the escape attempts so far and the battle seed. A
third of the cases leave the roll to the seed; the others search the battle seed for a first `randSeedInt(100)` of
exactly chance-1 (it must succeed) or chance (it must fail), so a chance off by one in either direction flips the
outcome. Compared: success, the flee message, the attempts counter after a failure and the command screen that
follows, and after a success the next wave and that the biome changed exactly when upstream pushed a
`SelectBiomePhase` (`hasRandomBiomes || isNewBiome()`).

Not covered: doubles (game-core plays them as singles; the sum over two active Pokemon on a side is battle-core's
loop, read but not run), the RNG state after the roll (the turn's end resets it in C and in upstream, and the producer
stops before it), speed ties (skipped: the turn's order is drawn first), the Challenge-mode rules, held items and
weather that change an effective speed (upstream's `getStat(SPD)` is the raw stat, which is what is compared).

battle-core's own `attempt_run_phase` pushes `SelectBiomePhase` for Endless and Spliced Endless on every flee
(`phases.c`, `|| game_mode == ENDLESS || SPLICED_ENDLESS`) where upstream's `hasRandomBiomes` is never set by any
mode; game-core ignores that push and decides with `gc_is_new_biome`, so the run is unaffected. Seen by reading, not by
a test, and left alone (battle-core is not touched here).
