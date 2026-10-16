# Wide EXP, candy and slot-summary callers

Pinned source `8555c08c823b856cbec4eb99ca84ea52a955836d`, locales
`270ed0a2938b2c3e2e34e7de940e84aecadf548b`, Node22. This slice follows the
wide storage/stat foundation; it does not complete wide-level Endless.

## Source and implementation

`PlayerPokemon.addExp` adds EXP and advances one level at a time while below
`BattleScene.getMaxExpLevel()`. On reaching or already exceeding that cap it
keeps the greater of that level's total EXP and the original EXP. There is no
level-200 limit. The source table/formula transition at level100 must be kept:
not every growth curve is monotone there. The port retains the source loop.

`PokemonLevelIncrementModifier.apply` adds one plus Candy Jar stacks to level,
sets EXP using `getLevelTotalExp` when at/below `getMaxExpLevel(true)`, calls
`addFriendship(6,true)`, then constructs a LevelUpPhase with the old/new levels
and actual party index. The normal cap does not limit candy. The ignore-cap
source value is Number.MAX_SAFE_INTEGER. Reward actors, evolution last-level
metadata and level-request metadata now retain signed32-bit levels. The game
bridge copies those levels and EXP without its former 255 clamp or byte cast.
The reward EXP helper consumes the existing run-core API, including its pinned
floating-point behavior. No run-core source is modified.

Level/count overflow outside signed32-bit is explicitly flagged/refused; Rarer
Candy preflights the whole party before applying. This is a representation
limit, not a claim of support for every upstream Number input. App EXP helpers
avoid signed overflow at INT32_MAX. The reconstructed slot summary retains the
full stored level; there is no new on-disk summary format.

## Evidence

Run `game-core/tools/wideprogressiondiff.sh UPSTREAM [12288] [777]` with Node22.
The producer captures native battle RNG before constructing GameManager and
restores it after battle startup. It calls actual native addExp/addModifier,
actual modifier filters, constructors, friendship logic and dynamic queue
insertion. UI sprite/info/candy-bar display and phase advancement are mocked;
LevelUp phases are **not executed**. Queue inspection reads the real requests
behind dynamic markers. The replay uses real gc_port_apply_reward with rotated
party storage, rather than a narrowed legacy candy fixture.

Seed777: 12,288/12,288 exact, 4,096 per operation (EXP/Rare/Rarer), all six growth
rates, 20,069 changed actor levels, 9,873 input actors above65,535 and 18,629
queued requests. Inputs include boundaries around levels100/200/255/32768/65536,
levels through1,000,000, level-cap boundaries, waves1–100,000, Classic/Endless/
Spliced Endless, 0–99 jars, fainted actors and friendship-cap boundaries.
Exact comparisons include level, double EXP, friendship, HP/maxHP, starter
progress/candy, party identity, old/new request levels and both RNG states.
No source exceptions are skipped. A strict complete footer and coverage floors
are required; malformed or missing records fail.

The unchanged legacy candy producer/replayer retains 4,000 cases, replayed free
and bought: 8,000 comparisons, zero mismatches. Its unrelated fixture byte cast
is not reused for wide inputs. The normal host candy table retains every case;
its old port-specific 254→255 expectation is corrected to the native254→260
with five Candy Jars. New host checks cover full-width queue metadata and
atomic overflow refusal. App slot tests save/inspect/load levels256/65536/
1,000,000. `wide_progression_guards.py CAPTURE` compiles six runtime faults and
rejects fourteen malformed captures; guards never edit production files.

## Still open

This checks level-request creation, **not dynamic execution order**, queued
LevelUp effects, full learning/evolution progression, EXP-award arithmetic,
high-wave encounter generation, damage, inventory, or complete natural Endless
runs. Source inventory and interacting doubles/faint/switch/fusion paths remain
on COMPLETION-CHECKLIST.md. A bounded exact replay does not close those rows.
Normal game-core/app/custom/save checks and exact-head CI (including3DS) remain
required; only the dedicated custom-only Task63 workflow is skipped.

Local normal game-core suite passed, including the retained candy table and
99 new wide-progression host checks. Paired200 saved/unsaved runs both retain
digest `5e19bd8a`. Standalone reward candy test:9 checks, zero mismatches.
