# Shedinja extra-party evolution and live TM history

Pinned source: `8555c08c823b856cbec4eb99ca84ea52a955836d`, locales
`270ed0a2938b2c3e2e34e7de940e84aecadf548b`, Node 22.

`PlayerPokemon.evolve` calls `handleSpecialEvolutions` before changing the
selected species. A primary or fused Nincada evolving to Ninjask creates the
extra Shedinja when there is party room and at least one ordinary Poké Ball.
The ball is **not consumed**. The child uses a fresh constructor, rather than
a copy of the parent: new ID, full HP, base friendship, level-total EXP, no
status/Pokérus/mint, and new move objects with zero spent PP but retained PP Ups
and max-PP overrides. The source explicitly copies passive, luck, encounter
and fusion metadata, forces genderless, and clones the parent's held items.
Spliced Endless consumes temporary fusion rolls before replacing that fusion;
Daily also generates a temporary moveset. Both affect the global RNG.

The evolution callback now invokes that owner. It reuses run-core's existing
fusion generator and the normal moveset generator. The child's evolution
updates the same primary-species profile fields as native fusion evolution.
Native Daily rentals (`metBiome == -1`) skip IV/seen/caught updates: the replay
exposed the previous C write (`dex[290].ivs[0] == 1`, native `0`) and the guard
now matches. Non-rentals and other modes retain profile updates.

`usedTMs` is the **same array object** in live siblings. Actual
`LearnMovePhase.learnMove` callbacks now update all live materializations with
the same runtime array token, including a Training Session retained member.
Tokens are independent of Pokémon IDs, survive party compaction, and are not
serialized. Native `PokemonData` JSON reconstruction separates the arrays;
C save/load likewise keeps their values but drops the token. Identity-counter
exhaustion and real stock/history limits remain explicit errors.

## Native observations and checks

The retained producer runs the actual native selection/evolution/special owner,
constructors, item clones, TM callbacks and `PokemonData` JSON reconstruction.
It captures native `BattleScene.prototype.randBattleSeedInt` **before creating
GameManager**, restores it after battle startup, and asserts that the evolution
and learning implementations are native. Visual init/assets/info/modifier bars,
learning animations, audio and UI acknowledgement are bypassed; this is not a
presentation or whole-encounter proof. Untouched party members' constructor
choices and offsets are explicit input controls so fresh captures reproduce.

The 2,400 rows cover five modes, primary/secondary Nincada and primary-first
negative controls, every party size 1–6, ordinary-ball counts 0/1, cycle offsets
20/25, abilities 0/2, all shiny tiers, raw/minted natures, burn/healthy state,
and Leftovers, Eviolite, Macho Brace and Soul Dew. Profile inputs cover caught
and uncaught roots. Every row compares party state, six IVs/stats, move PP,
ordered TM history, item stock/locks/Eviolite clone class, ball counts, both RNG
streams, profile mutations, legal C command saves with zero dropped fields,
and independent teaching after reload. No unsupported/stub flags are cleared.

Two additional sequences create successive fused siblings, teach them, remove
one through the party owner, and add an unrelated Pokémon with a colliding ID.
Separate C invariants check the retained Training Session member and identity
counter exhaustion. These invariants do not prove the whole Training Session
encounter. The compact replay performs 5,508,266 checks. The checker requires
14 compiled implementation faults, 10 malformed captures and four forged
observations to fail; a compilation failure is not counted as a caught fault.

Retained TM history (517,788 checks), 1,440 fusion level-evolution cases and
2,016 fusion item-evolution cases pass. The edited evolution/items/TM/constructor
and save translation units also pass the full new replay with AddressSanitizer
and UBSan; linked unchanged libraries are not instrumented in that scoped run.
Normal app/custom/save tests and the ARM build remain required in exact-head CI.

## Reproduce

```sh
make -C game-core shedinja-host-test
python3 game-core/tools/check-shedinja-proof.py
# With Node 22 and a clean pinned upstream plus installed dependencies:
game-core/tools/reproduce-shedinja.sh /path/to/upstream /tmp/shedinja-proof
```

The compressed JSON retains native observations. The builder only validates and
transports them into interned field vectors; it does not calculate expected
species, stats, stock, profile mutations or RNG. Fresh JSON and generated C
must match the retained fixture exactly.

## Remaining boundaries

This closes the recorded creation/TM-history cases, not all copying or inventory
semantics. Native also shares the IV array; the [Training Session follow-up](IV-ARRAY-VALIDATION.md)
now covers its light-reward mutation owner, separate identities and JSON reset.
Other constructor/IV mutation owners still need review. Full held
class/parameter cloning and arbitrary lock/virtual/type-reference histories are
not established by these four factories. Native `fusionTeraType` is retained in
the capture, but the pin only declares, serializes and copies it: it has no battle
read consumer and is not compared to a fabricated C field. Canonical holder ID
zero still collides with the existing wildcard representation; random fresh IDs
do not prove that boundary closed. Unknown Challenge rules, constructor inputs
outside this basis, full encounter generation and hardware acceptance remain
separate obligations. The custom football adapter retains its distinct direct
evolution rules. No builder is produced by this work.
