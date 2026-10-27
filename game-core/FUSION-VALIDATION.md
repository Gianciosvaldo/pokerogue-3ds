# DNA Splicers fusion (reward screen)

Taking DNA Splicers from the reward screen or the shop used to fall through to the generic held-item stand-in: the item was
added to the Pokemon that was picked, and no fusion happened. It now runs upstream's whole path at
`8555c08c823b856cbec4eb99ca84ea52a955836d`:

- `FusePokemonModifierType`: the offer needs two picks (`GC_TARGET_FUSE`; `GC_CH_REWARD`/`GC_CH_SHOP` carry the member that
  stays in `b` and the member that goes in `c`, both party indices). The same member twice, a pick that is not in the party,
  a member that is already fused (either one) and the fusion challenge are refusals: nothing is taken, no money is spent.
- `FusePokemonModifier.apply` -> `PlayerPokemon.fuse()`, in `src/gc_fusion.c`: the fusion half (species, form, gender, ability
  index, shiny, variant, luck, the second member's custom ability, evolution pause), the new hit points from both members'
  maxima before the fusion (`Math.round`), the stats of the fusion with the first member's held items, the status (the
  second member's, or a coin when both have one; a fainted result with a living second member gets one hit point and the second
  member's status), every held item of the second member moving over (`ignoreUpdate`, so vitamins and the Mystery Encounter stat
  items move although Manage Items would not move them; what doesn't fit a full stack is dropped with the second member), the
  second member leaving the party with the battle's pointers following, and one `LearnMovePhase` per move of the second member,
  queued in its order for the first member's new place.
- The save: `fusion_shiny`, `fusion_variant` and `fusion_luck` (what the second member's shine contributes to the party's
  luck) are an optional `fusion` section. A save without it loads whole and counts nothing as lost; a damaged one is refused.
- Held items in general: Toxic Orb and Flame Orb are one class upstream (`TurnStatusEffectModifier`), so a holder of one
  can't be given the other in Manage Items either (it used to take both).

## What is compared, and against what

`tests/fusion.producer.test.ts` runs upstream's real `FusePokemonModifierType.selectFilter`, `FusePokemonModifier` and
`PlayerPokemon.fuse()` in its test scene on a random party of 2-6 Pokemon of any kind (fused ones, fainted ones, statuses with
counters, a shuffled pile of held items of every kind, vitamins included). `tests/fusion_diff_test.c` replays each case
through `gc_port_apply_reward` and compares, member by member: the fusion half, the ability, hp, status and its counters, the
pause, the six stats, the luck, the moveset; the whole item list entry by entry; the learn phases queued, in order and with
the member's place; whether the status coin was drawn; that the party storage has no hole and the battle's pointers follow it.

| Seed | Cases | Fused | Refused | Coin drawn | Members | Items | Learn phases | Mismatches |
|---|---|---|---|---|---|---|---|---|
| 777 | 600 | 463 | 137 | 64 | 1,356 | 3,166 | 1,171 | 0 |
| 9001 | 600 | 459 | 141 | 77 | 1,392 | 3,258 | 1,166 | 0 |
| 4242 | 600 | 452 | 148 | 76 | 1,384 | 3,272 | 1,133 | 0 |
| 31337 | 600 | 467 | 133 | 64 | 1,384 | 3,314 | 1,174 | 0 |

`tools/fusionplant.sh` replays a trace through 23 copies of game-core, each with one line of the port changed (the old
maximum, the average, `Math.round`, the one-hit-point floor, the fainted status, the coin and its direction, items not
moved or kept, locked items, the Orb class, stats not recalculated, the tracker, the pointers of the battle, the learn
order and place, every part of the fusion half, the evolution pause): **23 of 23 exit 1.** Two things are not planted: the
fusion ability set by hand next to `pr_refresh_ability` (the refresh derives the same value) and the Gimmighoul tracker that
a Gimmighoul second member brings (it needs a member holding its tracker).

`tests/fusion_host_test.c` (325 checks, in `make test`) covers what doesn't need upstream: the shop and the free pick, the
refusals leaving everything as it was, the two orders of the picks, a full moveset asking what to forget, the coin and the
status rules, held items (a full stack, vitamins, the Orbs), the luck and the save, including the save from before the
section and damaged ones. `make test` also replays `tests/fixtures/fusion-splicers.native.txt.gz` (the seed 777 trace,
produced by upstream) so CI checks the comparator without an upstream checkout.

## Reproduce

Node 22, upstream at the commit above with its dependencies installed, nothing of it tracked.

```sh
make -C game-core test
game-core/tools/fusiondiff.sh /external/pinned-upstream 600 777
game-core/tools/fusiondiff.sh /external/pinned-upstream 600 9001
game-core/tools/fusiondiff.sh /external/pinned-upstream 600 4242
game-core/tools/fusiondiff.sh /external/pinned-upstream 600 31337
game-core/tools/fusionplant.sh game-core/build/fusion-777.txt
```

## Not done here

- **The app.** Opening the party screen for the second pick and sending the two indices is `game/`'s (a separate change); until
  then the offer is refused with "pick two Pokemon" when only one index comes.
- Unsplice (upstream's party option that splits a fusion back into two Pokemon), the Splice achievement and the
  `pokemonFused` counter, and the parts of `fusionCustomPokemonData` other than the ability.
- reward-core's own DNA Splicers case rounds the average hp with `floor(x + 0.5)`, which differs from `Math.round` at
  0.49999999999999994. game-core does not use its hp (it works it out from both members' real maxima with `js_round`); the
  reward-core case is left as it is (its own lane).
- The status coin is unseeded upstream (`Math.random`), so it uses the stand-ins' generator (`gc_stub_rand`) and can't move the
  seeded rolls; the tests set it to compare both outcomes.
