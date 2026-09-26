# battle-core: native C port of PokeRogue's battle logic

Status 2026-09-26: **damage path with abilities verified against upstream** (singles, no held items, no battler or arena tags). Upstream is `pagefaultgames/pokerogue` @ `8555c08`.

## What works, and how it was checked
| Piece | State | Evidence |
|---|---|---|
| RNG (Phaser `RandomDataGenerator`, `randSeedInt`, `shiftCharCodes`, battle-seed handling) | Bit-exact | 15 seeds x 2,000 mixed rolls identical to Phaser itself (`tests/rng_cli.c` vs `tests/rng_ref.cjs`) |
| Game data (species, forms, moves, abilities, enums, type chart, natures) | Generated from upstream's live objects | `gen/dump-upstream.test.ts` runs inside upstream's test harness after `initializeGame()` |
| Stat calculation (`calculateStats`) | Matches | over 50,000 cases (two Pokémon each) of random species, form, level, IVs and nature; 0 mismatches |
| Damage path (`getAttackDamage`, `getBaseDamage`, effectiveness, STAB incl. Tera, crit roll, random roll, burn, weather, stat stages) | Matches where covered | see the runs below |
| Variable-power moves: weight (Low Kick, Heavy Slam), HP (Eruption, Flail, Crush Grip), stat stages (Stored Power, Punishment), speed (Gyro Ball, Electro Ball), Solar Beam, Tera Blast, and 19 power-multiplier closures (Facade, Hex, Brine, Venoshock, Weather Ball...) | Matches where covered | same runs |
| Abilities on the damage path: `applyAbAttrs` dispatch (ability then passive, `canApplyAbility`, conditions, `canApply`/`apply`) and 37 attribute classes: stat multipliers (Guts, Hustle, Swift Swim, Ruin abilities...), type changes (-ate, Normalize, Liquid Voice), power boosts (Technician, Iron Fist, Sheer Force, Blaze-type, auras, Steely Spirit...), immunities (Levitate, Volt Absorb family, Wonder Guard, Soundproof, Bulletproof, Queenly Majesty, Good as Gold...), damage modifiers (Filter, Multiscale, Thick Fat, Fluffy, Tinted Lens, Disguise, Ice Face...), crits (Super Luck, Sniper, Merciless, Battle Armor), Scrappy, Unaware, Tera Shell, Heavy/Light Metal, Cloud Nine suppression | Matches where covered | same runs; 100 ability closures ported in `src/closures_abilities.c` |

Runs (each case is a random attacker, defender, move, weather, HP, stat stages, status, Tera state and gender; half the Pokemon get a random ability):
| Upstream seed | Cases | Covered | Covered and identical |
|---|---|---|---|
| 777 | 30,000 | 25,561 | 25,561 |
| 9001 | 30,000 | 25,549 | 25,549 |
| 4242 (abilities off, regression) | 20,000 | 16,990 | 16,990 |

Earlier Ball Fetch-only runs matched 68,116 of 68,116 covered cases. "Covered" means the C code hit no unported attribute (see below). The differential test found real bugs, all fixed: Stellar Tera typing, Shedinja's forced Bug tera type, and charging moves (Fly, Dive) not counting as attacks for Wonder Guard.

What the test doesn't cover yet: held items, battler and arena tags, terrain, doubles and fusions. Side effects that belong to the turn engine (Volt Absorb healing, Sap Sipper's stat raise, Sturdy's tag, Disguise's form change) are recorded as *deferred*: the damage result is checked, the side effect isn't applied until the phase engine exists. The only ability left uncovered in this state space is Analytic, which needs the turn's move queue.

## Design (the decisions that make 180k lines tractable)
1. **Data is generated, never hand-typed.** Upstream builds moves and abilities from ~430 attribute classes (`StatStageChangeAttr`, `StatMultiplierAbAttr`...). The dumper serialises every instance with its parameters. `gen/gen_tables.py` emits one C struct per class, a const table of instances, and an is-a bitmap for the class hierarchy. Upstream's `applyAbAttrs("X")` becomes "iterate attrs where `pr_attr_is(cls, PR_AC_X)`". Each class's C handler is written once; its 1,497 uses come free.
2. **Closures are tracked, not guessed.** Hand ports live in `src/closures_moves.c` and `src/closures_abilities.c`, each registered under the owner and source hash it was written against. A changed upstream closure therefore reads as unported instead of running stale logic. 735 upstream closures (move conditions, ability predicates) can't be dumped as data. They're listed in `gen/out/closures.txt` with a hash of their source. After an upstream bump, `tools/redump.sh` says exactly which ones changed. That's the answer to "upstream updates must be ported by hand": the tool tells you what to re-port.
3. **Unported logic is loud.** Every upstream `applyAbAttrs` site calls `pr_apply_ab_attrs`, which runs the C handler for each attribute class and reports any class without one. Move attribute sites call `pr_hook_move`. If a case touches anything unported, it's logged as *not covered*, never counted as a pass. The difftest prints the top missing classes, which is the porting to-do list, ranked by how often each class matters.
4. **State that isn't modelled yet has one entry point.** Battler tags (`pr_has_battler_tag`) and wave data (`pr_wave_ability_applied`) are single functions that currently report "absent". When tags are ported, every closure that asks about them becomes correct at once instead of being hunted down.
5. **Same arithmetic as JS.** IEEE doubles everywhere upstream uses JS numbers, `-ffp-contract=off`, the same operation order. Names follow upstream (`pr_get_effective_stat` = `Pokemon.getEffectiveStat`) so each function diffs against its TS original.
6. **No platform code.** Plain C99; builds on the PC for tests and with devkitARM for the 3DS (checked: compiles warning-free with `-march=armv6k -mfloat-abi=hard`). On ARM the logic is about 33 KB of code and the data about 380 KB of read-only tables.

## Layout
- `include/pr/` public headers (`rng.h`, `battle.h`)
- `src/` ported logic (`rng.c`, `hooks.c`, `pokemon.c`, `damage.c`, `move_attrs.c`, `ab_attrs.c` ability dispatch and handlers, `closures.c` registry, `closures_moves.c` and `closures_abilities.c` hand-ported closures)
- `gen/dump-upstream.test.ts` dumper (runs in an upstream checkout), `gen/upstream/*.json` dump (not in git; `tools/redump.sh` recreates it), `gen/gen_tables.py` generator, `gen/out/` generated C (committed so the code builds without upstream; do not edit), `gen/check_closures.py` drift report
- `tests/difftest.producer.test.ts` upstream half of the differential test; `tests/difftest.c` C half; `tests/rng_*` RNG check
- `tools/setup-upstream.sh <dir>` clones and prepares upstream (no images or audio are fetched). `tools/difftest.sh <dir> [N] [seed]` runs a differential test. `tools/redump.sh <dir>` refreshes the data and reports closure drift.

Build: `make` (outputs to `build/`). `tools/closures.py` shows an owner's closures and fills registry hashes.

## Custom content (planned, not built)
Upstream ids are compiled into const tables. Custom species, moves, abilities and items will use a reserved id range (from 30000 up) and load from a small pack on the SD card at startup. They'll reuse the same attribute classes, so a custom move is data such as "StatStageChangeAttr {stats: [ATK], stages: 2}" plus, only if needed, a new C handler. Lookups already go through one function per table (`pr_pokemon_form`), which is where the custom registry plugs in.

## Known gaps in the tooling
- The dumper only reads enums from `src/enums/`; a few live elsewhere (e.g. `TerrainType` in `src/data/terrain.ts`), so those are written by hand for now.
- A closure's hash covers its own source and probed captures, not named helpers it calls (`anyTypeMoveConversionCondition`, `targetSleptOrComatoseCondition`). Check those by hand after an upstream bump.

## Not done (in rough order)
1. Remaining damage-path move attributes. By frequency in the difftest: HitsTagAttr, VariableMoveTypeAttr, the power closures that need turn history or held items (Knock Off, Payback, Acrobatics...), MoveTypeChartOverrideAttr, OHKO, category-changing moves, pledges, consecutive-use moves.
2. Held items and modifiers, battler and arena tags, terrain, weather and terrain lifecycles, status effects over turns, the other ~140 ability classes (switch-in, end-of-turn, contact effects...).
3. The turn engine: upstream's phase queue (`src/phases`, ~13.7k lines), turn order, switching, fainting, multi-target doubles. Deferred side effects get applied here.
4. Enemy AI, wave generation, trainers, rewards, biomes, mystery encounters, eggs, save format.
5. Hooking it to the renderer in `assets-pipeline/app-src`.

A real console run of this code hasn't happened. The float maths is only + - * / floor ceil, which ARM VFP rounds the same way, so I expect identical results, but that's inferred.
