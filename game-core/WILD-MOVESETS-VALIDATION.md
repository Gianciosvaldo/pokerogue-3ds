# Ordinary wild moveset generation

`gc_enemy_moveset` previously called only the small run-core clean factory.
Most species, and every fusion, therefore took the last-four-level-moves
fallback even though the weighted generator already existed in game-core for
mystery encounter callers. The retained 800 ordinary wild constructor inputs
included 794 such fallbacks.

The adapter keeps the existing clean factory first and connects the weighted
level-pool generator for ordinary non-boss wild Pokémon in Classic, Endless
and Spliced Endless. The generator now accepts non-boss fusions without a
passive. Trainer streams, challenges, broader bosses/passives and post-moveset
form callbacks remain separate owners. Malformed fusion snapshots are rejected.
An unported operation rejects the attempt without changing the Pokémon or RNG.

## Source boundary

Source `8555c08c823b856cbec4eb99ca84ea52a955836d`, locales
`270ed0a2938b2c3e2e34e7de940e84aecadf548b`, assets
`87426a79611f9d212c4dc8af58e2834a05b93725`, Node 22.
Both producers capture native `BattleScene.prototype.randBattleSeedInt` before
constructing GameManager and restore it after battle startup. They capture the
actual native EnemyPokemon generator before GameManager and verify that its
base Pokemon method remains intact. Enemy developer overrides are disabled.
There are no skipped cases or C-derived expected outputs.

The constructor producer calls actual `scene.addEnemyPokemon` and native full
moveset generation. Its first 572 inputs retain the previous fusion projection
matrix (four primaries, levels 1/100/70000, both Endless modes, then 500 Spliced
inputs with four Ability Charms). Sixteen level-65536 inputs append a meaningful
16-bit boundary. Signature moves follow the primary species, as upstream does.
Inputs are shiny-locked, wild, non-boss, without a passive; weather/terrain are
absent and format is single. This is a constructor/generator boundary, not a
complete encounter, modifier or summon-sequence proof.

The retained producer uses the original C adapter/wild-constructor snapshots
as **inputs** and runs the actual native generator for the expected outputs.
Native stats and ability must independently equal those input snapshots.
It covers all 225 formerly rejected non-boss adapter cases and all 800 wild
constructor snapshots. This is a raw generator oracle, not a native full-wild
encounter capture. All 2,100 adapter seeds were recorded before adding assertions:
the old test used its evolving assertion count as seed. The original 2,900
inputs are retained and bound by hash, so added checks cannot silently replace
the old corpus.

## Checks and limits

The native owner test compares every ordered level-pool entry and its source,
constructor stats/ability, selected moves, PP-used/PP-up/override metadata and
all four published Phaser RNG-state components. It exercises the public wild
adapter, the underlying generator and refresh. Previous-battle stat modifiers
are poisoned to verify clean constructor context. The retained adapter still
compares every old positive case with the unchanged clean factory, every old
unsupported boss with an unchanged-object/RNG rejection, and all 800 actual
wild constructors through their existing constructor-finish draws. All 794
formerly rejected wild inputs now have independent native expectations.
Fifteen unsupported boundary cases and an injected late unported operation
check atomic rejection and recovery.

Phaser's `state()` publishes c/s0/s1/s2. C's additional `n` member is seed-mash
scratch, not a fifth native observation. Retained comparisons preserve that
scratch while comparing actual published native state exactly.

Run `python3 game-core/tools/check-wild-movesets-proof.py` for fixture transport,
actual owners and deliberate compiled/malformed/forged failure controls.
`WILD_MOVESETS_SANITIZE=1` instruments the adapter, generator and encounter unit
with ASan/UBSan/LSan; supporting archives are unsanitized. Normal game-core,
app, custom, save, football and ARM build/stack checks remain required.

The original level-70000 matrix did **not** kill a 16-bit level-narrowing fault:
4464 still exceeds every learn level. The appended native level-65536 cases
exist to test that fault meaningfully. The earlier source observations remain
unchanged. Passing this bounded proof does not close trainer/fused-boss/passive
movesets, post-moveset form changes, all weather/ability interactions, complete
Endless encounter generation, or hardware acceptance.

The retained comparison passes 1,613 native cases and 168,244 checks. The
original adapter/constructor suite passes 51,166 checks: 1,350 supported
adapter cases, 750 unchanged unsupported boss cases, and all 800 wild
constructors (including all 794 formerly rejected inputs). Eleven compiled
faults, thirteen malformed capture/corpus controls and four forged-output
controls are detected. Both source captures and generated fixtures reproduce
byte for byte, including the appended boundary cases.
