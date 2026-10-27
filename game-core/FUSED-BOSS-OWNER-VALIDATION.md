# Fused boss moveset owner

Pinned source: `8555c08c823b856cbec4eb99ca84ea52a955836d`.

After the Raging Bull fusion-type and resolved-type ability-power fixes, the
weighted generator matches all retained fused-boss observations. The public
`gc_enemy_moveset` constructor owner now admits bosses with a fusion and their
natural species passive. The existing weighted algorithm supplies boss filters,
weights and active/passive effects. Explicit passive overrides remain rejected.

Native `EnemyPokemon.isBoss()` reads `bossSegments`. Both the public owner and
generator reject inconsistent raw boss flags and segment counts. This retains
the old malformed fused-boss rejection case unchanged, alongside trainer,
challenge, daily, mystery, metadata and learned-move form callback guards.

The source captures are shared with [the power proof](FUSION-POWER-VALIDATION.md):
804 actual Spliced boss constructors and eight prepared compatible fusion
snapshots. The new strict transport preserves all selected moves in order,
`PokemonMove` PP metadata and generator finish RNG. Whole-constructor finish RNG
remains separate because the later boss IV roll occurs after move generation.
Both native captures and this additional C fixture reproduce byte-for-byte.

The host proof checks all 812 inputs through the public constructor owner,
private weighted generator and `gc_refresh_enemy`. It checks zero fallback
stubs, exact move order, PP metadata and RNG, and poisons previous battle
modifiers to catch accidental reuse. It also checks fourteen unsupported raw
inputs and late-unported rejection with RNG restoration and successful recovery.
The previous power-query test now checks challenge rejection for these inputs;
their formerly guarded successful generation is covered by this owner proof.
No old native case is removed.

```sh
make -C game-core fused-boss-owner-host-test
python3 game-core/tools/check-fused-boss-owner-proof.py
FUSED_BOSS_OWNER_SANITIZE=1 python3 game-core/tools/check-fused-boss-owner-proof.py
```

Thirteen compiled faults, five malformed captures and four forged observations
must fail. Scoped ASan/UBSan/LSan covers the adapter, generator and encounter
owner; supporting archives are unsanitized. The complete ordinary-wild,
unfused-boss and ability-power failure controls remain enabled. The unfused-boss
checker's restore-rejection mutation now targets the broader admission condition
and retains the same assertion and original native cases.

These are bounded constructor-boundary proofs, not complete trainer/TM/egg
generation, post-moveset form callbacks, arbitrary passive overrides, full
Endless pipeline acceptance, or battle-core completion.
