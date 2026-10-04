# game-core: the run between the battles

A headless port of upstream PokeRogue's run loop (upstream 8555c08): a new run and its starters, each
wave's encounter, the battle's commands, victory, experience, level ups, new moves, evolution, money,
rewards, biome changes and game over. It has no graphics, sound or file access. The 3DS app shows its
events and screens and hands back the player's choices.

It calls battle-core for the turns, run-core for the waves and the enemies, ai-core for the enemy's
commands (and the player's command restrictions and trapping), and goes through `include/gc/ports.h`
for the enemy movesets and the rewards. The reward screens are reward-core's (`src/gc_reward.c`, Task 59).
Until run-core's movesets land and rewards act on the run (Task 60), stand-ins in `src/gc_stubs.c` keep a run
playable. Each stand-in is counted, so the soak runs show how much of a run is still standing in.

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

Screens: starters, command (Fight / Pokémon; Ball and Run come with catching), switch, learn move,
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
| Enemy moveset: the last four level moves | run-core |
| A trainer's switch-out picked by the AI does nothing (the enemy loses its turn) | battle-core: the enemy side's SwitchSummonPhase |
| A reward taken does only what the placeholder knows (potions, revives, ethers, Rare Candy, balls, held and permanent items); any other is added to the items and counted (`reward taken without its own effect`) | Task 60 |
| Double battles are played as singles (M1 rule) | doubles in battle-core |
| Mystery encounters and fixed classic battles (rival, evil team, gyms run-core can't make) become wild battles of the wave's level | run-core, later the encounters |
| The rest of a turn after a victory (end-of-turn effects) is lost | battle-core: carry on past a stop |
| No weather roll for a new biome | port of `getRandomWeatherType` |
| Evolution conditions on items, held items or the dex aren't met | reward-core, the dex |
| No catching or fleeing | battle-core (AttemptCapturePhase, AttemptRunPhase) |
| A trainer's own rewards (the config's reward items, a boss's voucher) aren't given | reward-core |
| A trainer's name in a message goes without its title ("Aaron would like to battle!" for "Ace Trainer Aaron would like to battle!") | battle-core: a message argument made of several locale keys |
| Pickup, Honey Gather, Pay Day, lapsing items | reward-core |

Not started: the dex and starter unlocks, eggs, the "Will you switch?" prompt at a trainer's new Pokémon, Imprison and the
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
generated file unchanged). Applying a reward is still the placeholder (Task 60).

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
  `usedTMs` stays empty (Task 60).
- Fusion gender and luck aren't tracked (reward-core's item-evolution gender condition uses fusion gender).
  Lure and Silver Poké Ball battle counts aren't tracked either (the differential leaves lures out).
  `metSpecies` is derived from the current species' root. Challenges aren't modelled: no level-up moveset hook,
  `EGG_MOVE_RELEARN_AVAILABILITY` and `isAllowedInChallenge` keep their defaults.
- `gc_level_up_moves` (level-up learning) still ignores form learnsets, though the dump now has them.
- The app (`game/`): the Lock Capsule toggle isn't wired into the UI and how it draws the new item keys is
  unchecked. The shop, fixed-battle and mystery encounter custom reward settings, and fleeing (a non-victory
  is always the restricted screen) are untouched.
- No hardware or emulator result; the 3DS build is CI's (no ARM toolchain here).
