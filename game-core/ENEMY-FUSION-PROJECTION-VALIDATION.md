# Generated enemy fusion projection

Run-core already generates an enemy's fusion species, form, ability index and
gender. `gc_mon_new_enemy` discarded those fields, including when
`gc_refresh_enemy` rebuilt the object after constructor completion. The native
generation result therefore became an unfused battle actor. The initial
72-case native capture contained 24 fusions; the old controller lost all 24.

The controller now copies the generated fusion fields and marks the supplied
secondary gender known before common initialization. Existing initialization
refreshes the effective ability cache from that identity. Refresh uses the
same constructor owner, so the identity survives both calls. Empty/zero and
canonical `-1` fusion sentinels leave the actor unfused. This adds no fields,
rolls, save format or generation algorithm.

## Native boundary and checks

Source `8555c08c823b856cbec4eb99ca84ea52a955836d`, pinned locales, Node 22.
The producer captures native battle RNG before constructing `GameManager` and
restores it after battle startup. It calls actual `scene.addEnemyPokemon`,
`EnemyPokemon` construction and `generateFusionSpecies`, with developer enemy
overrides disabled. Its explicit boundary replaces moveset generation with a
fixed Splash and no rolls. Inputs are wild, non-boss and shiny-locked; player
Ability Charms provide hidden-slot diversity. No constructor result is derived
from C or normalized.

The matrix has 572 cases: 524 fusions and 48 unfused controls, 14 nonzero fusion
forms, 39 hidden fusion slots, all three fusion genders and 274 input levels
above 65,535. It retains the first 72 observations unchanged and adds 500
Spliced Endless cases. Primary species include Bulbasaur, Ditto, Mew,
Magnemite and Eevee; levels are 1, 100 and 70,000.

The C proof consumes these native constructor snapshots as the input DTO to
the actual creation and refresh owners. It then executes the existing
`gc_enemy_finish_party_items` owner with zero enemy items and compares abilities
(including caches), identity, gender, types, IVs, stats, HP, EXP and fused names.
The projection and refresh must not add RNG draws. Post-constructor item-pool
threshold generation has its own RNG calls, outside the captured constructor
boundary; this proof does not compare that later phase's RNG with the earlier
snapshot. It does not suppress or restore those calls.

Raw guards check constructed move PP metadata through refresh. Each source
enemy frame is also placed into an actual legal command/save window and loaded
again, checking identity, secondary gender, ability, HP, EXP, stats and field
rebinding. These are serialization tests with imported constructor frames,
not native encounter/summon-order evidence. All 572 roundtrips and 46,680 host
checks pass.

Nine compiled owner faults are detected, after unmodified runners through the
same link/working-directory paths pass. They omit projection, substitute
primary form/ability/gender, lose known gender, add a transport RNG draw,
retain the primary ability cache, discard identity on refresh, or lose PP
metadata. Ten malformed captures and four forged outputs (ability, HP/stat,
name and EXP) are rejected. Compile failures do not count as detection.
Fresh native JSON and its generated C fixture must match byte for byte.

Scoped ASan/UBSan/LSan instruments the constructor, encounter and save owners;
supporting library units remain unsanitized. Normal core/app/custom/save tests
and actual ARM build/stack checks remain required.

```sh
python3 game-core/tools/check-enemy-fusion-proof.py
ENEMY_FUSION_SANITIZE=1 python3 game-core/tools/check-enemy-fusion-proof.py
bash game-core/tools/reproduce-enemy-fusion.sh /path/to/upstream /path/to/output
```

## Remaining scope

This fixes the handoff of existing generation results. It does not replay the
complete constructor-generation RNG pipeline in C, implement fused/trainer
moveset generation, prove boss or shiny-fusion arrangements, or complete
natural Spliced Endless encounters/runs. Existing moveset fallback flags remain
visible. General DNA Splicers acquisition, fusion item evolution,
Nincada/Shedinja extra-party creation and broader capture/progression
interactions remain separate owners. Registry and bounded case counts do not
establish battle-core completion. No Windows builder is released.
