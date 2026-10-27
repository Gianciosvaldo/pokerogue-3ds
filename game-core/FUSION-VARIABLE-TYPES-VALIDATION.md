# Fusion Raging Bull type owner

A Pokémon with Paldean Tauros as its fusion partner treated Raging Bull as
Normal in battle-core. The pinned attribute recognizes either species and
returns Fighting for that fusion. This also affected weighted move generation:
a fresh fused-boss probe selected Thunder instead of Raging Bull as the forced
STAB move. The core attribute now recognizes the partner while preserving the
primary species' form when both halves match.

The pinned implementation reads `user.fusionSpecies.formIndex`, the registry
species object's own index **0**, rather than `user.fusionFormIndex`. Therefore
its fusion branch returns Fighting even when the stored fusion form is Blaze
or Aqua. This change preserves that observed source behavior. A primary Tauros
still uses its actual form: Fighting / Fire / Water.

## Native query boundary

Upstream `8555c08c823b856cbec4eb99ca84ea52a955836d`, locales
`270ed0a2938b2c3e2e34e7de940e84aecadf548b`, assets
`87426a79611f9d212c4dc8af58e2834a05b93725`, Node 22. The producer captures
native battle RNG before GameManager and restores it after startup. It also
verifies the native Pokemon type method identity. Queries consume no source
RNG. This is a prepared species/form query boundary, not a DNA acquisition or
complete encounter proof.

The 2,976 native rows cover Judgment, Multi-Attack, Techno Blast, Aura Wheel,
Raging Bull and Ivy Cudgel. They enumerate every valid form of their relevant
pinned species, each as primary, as fusion partner, with an unrelated partner,
and the full relevant primary/fusion form combinations. Controls have neither
matching species. The actual attribute holder, Pokemon.getMoveType and
item-spawn type arrays are observed directly. The producer also records the
native attribute return flag and AI type endpoint for context; those latter
two fields are not separately claimed as C runtime comparisons here.

The C test invokes actual `pr_apply_variable_move_type`, `pr_get_move_type` and
`gc_types_for_item_spawn`, checks every returned type and item-array entry,
checks unchanged actor bytes and all C RNG streams, and rejects unported
results. The old core has three raw/actual mismatches (the three fusion Tauros
forms), while item projection already agrees because it has an existing
partner-as-primary compatibility owner. The fix gives zero mismatches across
all 2,976 rows and 26,795 checks. Other variable type families retain their
existing query results. Item behavior is unchanged.

Both source JSON and generated C fixture reproduce byte for byte. The strict
transport validates the entire input matrix and keeps actual source outputs.
Six compiled faults, ten malformed captures and three forged output controls
are detected. Initially, a getter-RNG fault survived an all-zero C RNG state;
seeding the test with the captured native state makes that fault meaningful
and detectable. No input, output or coverage assertion was dropped.

Scoped ASan/UBSan/LSan instruments the core move attribute unit and the
controller's item-type owner; supporting archives remain unsanitized. Run
`python3 game-core/tools/check-fusion-variable-types-proof.py`, or set
`FUSION_VARIABLE_SANITIZE=1` for that scoped sanitizer pass. Normal
core/app/custom/save/football tests and ARM build/stack checks remain required.

## Related diagnostic and limits

The discovery came from an additional 804 actual native fused-boss generator
frames, with 26,861 auxiliary native power queries performed in native
moveset-generation mode. The initial C diagnostic matched every power and RNG
state but differed on one selected moveset: Dedenne + Paldean Tauros. Its
Raging Bull type prediction reached the same core attribute fixed here.
That diagnostic explicitly bypasses the existing fused-boss admission guard
in a scratch copy. It is not a claim that those bosses are enabled in game.

Fused boss/passive acceptance, conditional power interactions, trainer
movesets, broader caller prerequisites, complete Endless and hardware
acceptance remain separate. This bounded source proof does not establish full
battle-core completion. No Windows builder is produced.
