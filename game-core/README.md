# game-core: the run between the battles

A headless port of upstream PokeRogue's run loop (upstream 8555c08): a new run and its starters, each
wave's encounter, the battle's commands, victory, experience, level ups, new moves, evolution, money,
rewards, biome changes and game over. It has no graphics, sound or file access. The 3DS app shows its
events and screens and hands back the player's choices.

It calls battle-core for the turns, run-core for the waves and the enemies, ai-core for the enemy's
commands (and the player's command restrictions and trapping), and goes through `include/gc/ports.h`
for the enemy movesets and the rewards. Until reward-core and run-core's movesets land, stand-ins in
`src/stubs.c` keep a run playable. Each stand-in is counted, so the soak runs show how much of a run is
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

## Building and playing on a PC

```sh
make                                        # builds run-core and battle-core's libraries too
./build/gcplay --starters BULBASAUR,CHARMANDER,SQUIRTLE     # play by typing the choices
./build/gcplay --auto 1000 --jobs 8                          # seeded soak runs
./build/gcplay --auto 1 --first 42 --verbose                 # replay soak run 42 with every event
./build/gcplay --auto 1 --first 42 --verbose --locales <upstream>/public/locales/en   # as English text
```

The soak runs play a random team of starters within the cost budget with a plain player of their own
(their own RNG, seeded by run number): the strongest move by power, STAB and type, switch out when
low, heal the hurt and revive the fainted from the rewards, Rare Candy on the lowest level, potions
from the shop, learn a move when it is stronger, always evolve. 15% of its answers are random legal
ones, so the runs stay varied. Each run is its own process, so a crash or a hang is reported with the
seed that caused it. The report lists where runs ended, game-core's and run-core's stand-ins, the
battle texts battle-core hasn't ported yet (by source line) and battle-core's unported effects, each
by how many runs hit it.

## What upstream does that game-core doesn't do yet

Stand-ins (counted in `run->stubs`, named in `gc_stub_name`):

| Stand-in | Waits on |
|---|---|
| Enemy moveset: the last four level moves | run-core |
| A trainer's switch-out picked by the AI does nothing (the enemy loses its turn) | battle-core: the enemy side's SwitchSummonPhase |
| Rewards: a fixed placeholder pool (potions, revives, ethers, Rare Candy, balls) | reward-core |
| Double battles are played as singles (M1 rule) | doubles in battle-core |
| Mystery encounters and fixed classic battles (rival, evil team, gyms run-core can't make) become wild battles of the wave's level | run-core, later the encounters |
| A trainer's next Pokémon is its first one still able to fight, straight onto the field without its summon phases | battle-core battle API, ai-core's getNextSummonIndex |
| The rest of a turn after a victory (end-of-turn effects) is lost | battle-core: carry on past a stop |
| The summon phases at a battle's start (entry abilities, hazards) aren't run | battle-core battle API |
| No weather roll for a new biome | port of `getRandomWeatherType` |
| Evolution conditions on items, held items or the dex aren't met | reward-core, the dex |
| No catching or fleeing | battle-core (AttemptCapturePhase, AttemptRunPhase) |
| A trainer's own rewards and money multiplier aren't given (x1 money) | a trainer config dump |
| Pickup, Honey Gather, Pay Day, lapsing items | reward-core |

Not started: saving and loading (`gc_run_save` returns 0 for now; it's the next slice), the dex and
starter unlocks, eggs, the "Will you switch?" prompt at a trainer's new Pokémon, Imprison and the
restriction texts of their own (a restricted move answers `battle:moveCannotBeSelected` until
battle-core's `pr_move_usable` lands), the trainer config's boss flag and Terastallization for the fixed
battles' trainers, form learnsets, and the prevolution-move messages.

## Data

`gen/out/gc_evolutions.c` (each species' evolutions and their conditions) and `gen/out/gc_starters.c`
(starter costs and the default unlocked starters) are dumped from upstream. To dump them again from an
upstream checkout prepared by `battle-core/tools/setup-upstream.sh`: `tools/redump.sh <upstream dir>`.
