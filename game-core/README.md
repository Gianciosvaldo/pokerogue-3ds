# game-core: the run between the battles

A headless port of upstream PokeRogue's run loop (upstream 8555c08): a new run and its starters, each
wave's encounter, the battle's commands, victory, experience, level ups, new moves, evolution, money,
rewards, biome changes and game over. It has no graphics, sound or file access. The 3DS app shows its
events and screens and hands back the player's choices.

It calls battle-core for the turns, run-core for the waves and the enemies, ai-core for the enemy's
commands (and the player's command restrictions and trapping), and goes through `include/gc/ports.h`
for the enemy movesets and the rewards. Until reward-core and run-core's movesets land, stand-ins in
`src/gc_stubs.c` keep a run playable. Each stand-in is counted, so the soak runs show how much of a run is
still standing in.

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
| Rewards: a fixed placeholder pool (potions, revives, ethers, Rare Candy, balls) | reward-core |
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
battles' trainers, form learnsets, and the prevolution-move messages.

## Data

`gen/out/gc_evolutions.c` (each species' evolutions and their conditions), `gen/out/gc_starters.c`
(starter costs and the default unlocked starters) and `gen/out/gc_trainers.c` (each trainer config's
money multiplier, boss flag, reward count and names) are dumped from upstream. To dump them again from an
upstream checkout prepared by `battle-core/tools/setup-upstream.sh`: `tools/redump.sh <upstream dir>`.

Trainer names are upstream's own `Trainer.getName`, run by the dump on every config and variant for the
calls the game makes (`GC_TNAME_*`: the titled name of the appeared and defeated messages, the send-out
names, the victory speaker) with `i18next.t` swapped for one that returns `$t(key)`. So a name is text
with `$t(key)` segments, i18next's nesting syntax: `$t(titles:gymLeader) $t(trainerNames:brock)`, or
`$t(trainerClasses:aceTrainer) @N` where game-core puts the personal name run-core rolled
(`$t(trainersCommon:aceTrainer.male.3)`). `run->trainer_names` has them. A double-only pair (Twins) has
one name, "Amy & May", which upstream splits for each twin's send-out; here both send-outs name the pair.
