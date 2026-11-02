# Plain Gym constructor egg pools

Pinned upstream `8555c08c823b856cbec4eb99ca84ea52a955836d`; locales
`270ed0a2938b2c3e2e34e7de940e84aecadf548b`.

Brock, Roxanne, Tate and Liza now use the native egg pool alongside their TM and
level pools. These reviewed default profiles have no `postProcess` constructor
callback or instant Tera policy. Tate/Liza's named doubles are included; Brock/
Roxanne double contexts stay explicitly unowned. Other special trainers retain
their existing counted fallback until their own complete constructor owner lands.

The source trainer config has `isBoss=true` and permits eggs. Each captured
Pokémon has `isBoss=false` and no passive. Those are distinct source properties;
this change does not add shields, passives or form transitions.

## Implementation and source order

`src/ai/ai-moveset-gen.ts` first supersedes level moves, then builds the egg and
TM pools. Egg moves come from the root species' full native egg table, independent
of player unlocks. Their weight is the rounded highest level-pool weight times
0.85, capped at 60. Rare eggs use 5/6 of that weight, require level 170 and are
excluded for shielded boss Pokémon. Ordinary eggs require level 80 and the
trainer's `allowEggMoves` config. TMs exclude egg/level duplicates and consider
both other pools when removing superseded moves.

The master map inserts TMs, eggs, then level moves. Filling applies both caps to
the shared `tmCount + eggCount`; forced signature/STAB picks bypass the fill caps
but increment the appropriate count. Removing a useless selected move does not
refund it. The egg cap rises at 80/121/161/201. All balance constants, config
facts and cap thresholds are dumped from native APIs; no player egg-unlock state
is substituted for the enemy pool.

The existing per-member constructor continuation still generates moves, finishes
the constructor once, and restores ambient RNG. No changes are made to trainer
selection, special boss callbacks, abilities, forms/shields, fusion or trainer Tera.

## Actual-source comparisons

`tests/gym-egg.producer.test.ts` observes real `Trainer.genPartyMember`,
`addEnemyPokemon` and every `EnemyPokemon.generateAndPopulateMoveset` call. Native
battle RNG is captured before `GameManager`, restored after battle startup, and
real moveset/effective-power methods are checked. The producer asserts each
reviewed constructor has exactly one generation, no `postProcess` argument and
no planned Tera. It records source config/role distinctions and initial team
indices instead of inferring them from trainer names.

The frozen observations contain 288 first-member cases plus 288 complete parties
with 1,536 members. Waves are 31/91/131; levels are 79/80/120/121/169/170/200/201;
Tate/Liza include both single and named-double contexts. Real prior source party
members are installed before each subsequent generation. Both original capture
partitions are immutable hashes in the transport.

Production comparisons cover all 1,824 base/moves/finish sequences, including
species, forms, abilities, gender, nature, initial/final IVs, shiny metadata,
friendship, moves/PP, all four RNG fields and no-item stat/HP refresh. All 288
parties pass, including 96 doubles. Twenty-three rejection cases preserve
unsupported roles, modes, forms, callback profiles, active field/movesets,
stat-operation failure and unreviewed double contexts; partial-party failure
remains atomic.

A failing new mutation control showed that those natural party samples never
exercised a level/egg duplicate. Native catalogue discovery identified overlaps
in fixed Revavroom forms 2/3/5. Eighteen additional **direct generator API**
controls prepare those forms, select ability slot 0, call the real native stat
calculation, clear moves, then observe real generation at levels 80/170/201.
They are synthetic input contexts, not claims that these forms occur on Brock's
natural team, and implement no form-transition or team-boss behavior. They catch
the missing duplicate exclusion without dropping any party observation.

The combined runner passes 128,928 assertions. The earlier ordinary-trainer
proof retains all 247,371 assertions. The compiled-fault suite rejects all 21
faults, including missing/early/rare eggs, weights, duplicates, insertion order,
shared caps/counters, cross-pool superseding, source stream restoration and
unreviewed profile admission. Transport checks reject 19 malformed/forged observations. A fresh native capture
and C projection are byte-identical. Scoped ASan/UBSan/LeakSanitizer passes the
same 128,928 assertions with the generator, constructor continuation and runner
instrumented; other linked archives remain unsanitized.

Reproduction:

```sh
make -C game-core gym-egg-constructor-test gym-egg-capture gym-egg-faults
bash game-core/tools/reproduce-gym-eggs.sh /path/to/pinned-upstream /tmp/gym-egg
```

Normal battle/app/custom/save and actual ARM stack/build checks remain required
before merging. Only dedicated custom Task63 is skipped. No Windows builder.

This is bounded source evidence. It does not close postprocessing/regeneration,
admin/team-boss movesets, abilities/forms/shields, fusion, trainer Tera, the source
closure ledger or the integrated high-wave Endless acceptance chain. Changes in
those lanes must preserve these constructor observations during integration.
