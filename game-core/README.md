# game-core: the run between the battles

A headless port of upstream PokeRogue's run loop (upstream 8555c08): a new run and its starters, each
wave's encounter, the battle's commands, victory, experience, level ups, new moves, evolution, money,
rewards, biome changes and game over. It has no graphics, sound or file access. The 3DS app shows its
events and screens and hands back the player's choices.

It calls battle-core for the turns, run-core for the waves and the enemies, and goes through
`include/gc/ports.h` for the enemy AI, the enemy movesets and the rewards. Until ai-core, reward-core and
run-core's movesets land, stand-ins in `src/stubs.c` keep a run playable. Each stand-in is counted, so
the soak runs show how much of a run is still standing in.

## The API (`include/gc/run.h`)

```c
static gc_run run;                            /* about 180 KB: keep it static or on the heap */
gc_run_new(&run, GAME_MODES_CLASSIC, seed, entropy);   /* waits on GC_SCREEN_STARTERS */
gc_run_set_starters(&run, starters, n, &why);          /* runs to the first screen of wave 1 */
for (;;) {
    gc_event ev;
    while (gc_run_next_event(&run, &ev)) show(&ev);    /* messages (upstream i18n keys + args), HP,
                                                          faints, exp, level ups, money, waves... */
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

Messages carry upstream's i18n key (`battle:expGain`) and named arguments (`pokemonName`, `exp`), so
the app looks up the English text in `data.pak` the way upstream does.

## Building and playing on a PC

```sh
make                                        # builds run-core and battle-core's libraries too
./build/gcplay --starters BULBASAUR,CHARMANDER,SQUIRTLE     # play by typing the choices
./build/gcplay --auto 1000 --jobs 8                          # seeded soak runs
./build/gcplay --auto 1 --first 42 --verbose                 # replay soak run 42 with every event
```

The soak runs make random legal choices (their own RNG, seeded by run number) with a random team of
starters within the cost budget. Each run is its own process, so a crash or a hang is reported with the
seed that caused it. The report lists where runs ended, the stand-ins they used, and battle-core's
unported effects by how many runs hit them.

## What upstream does that game-core doesn't do yet

Stand-ins (counted in `run->stubs`, named in `gc_stub_name`):

| Stand-in | Waits on |
|---|---|
| Enemy moveset: the last four level moves | run-core |
| Enemy AI: a random usable move | ai-core |
| Rewards: a fixed placeholder pool (potions, revives, ethers, Rare Candy, balls) | reward-core |
| Double battles are played as singles (M1 rule) | doubles in battle-core |
| Mystery encounters and fixed classic battles (rival, evil team, gyms run-core can't make) become wild battles of the wave's level | run-core, later the encounters |
| A trainer's next Pokémon goes straight onto the field, without its summon phases | battle-core battle API, ai-core (which one it sends) |
| The rest of a turn after a victory (end-of-turn effects) is lost | battle-core: carry on past a stop |
| The summon phases at a battle's start (entry abilities, hazards) aren't run | battle-core battle API |
| No weather roll for a new biome | port of `getRandomWeatherType` |
| Evolution conditions on items, held items or the dex aren't met | reward-core, the dex |
| No catching or fleeing | battle-core (AttemptCapturePhase, AttemptRunPhase) |
| A trainer's own rewards and money multiplier aren't given (x1 money) | a trainer config dump |
| Pickup, Honey Gather, Pay Day, lapsing items | reward-core |

Not started: saving and loading (`gc_run_save` returns 0 for now; it's the next slice), the dex and
starter unlocks, eggs, the "Will you switch?" prompt at a trainer's new Pokémon, move restrictions on
the command screen (Disable, Taunt, Encore, Imprison), form learnsets, and the prevolution-move messages.

## Data

`gen/out/gc_evolutions.c` (each species' evolutions and their conditions) and `gen/out/gc_starters.c`
(starter costs and the default unlocked starters) are dumped from upstream. To dump them again from an
upstream checkout prepared by `battle-core/tools/setup-upstream.sh`: `tools/redump.sh <upstream dir>`.
