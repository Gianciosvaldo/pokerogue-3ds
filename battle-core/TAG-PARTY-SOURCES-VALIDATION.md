# Battler-tag full-party sources

Pinned source: `8555c08c823b856cbec4eb99ca84ea52a955836d`; locales:
`270ed0a2938b2c3e2e34e7de940e84aecadf548b`.

`BattleScene.getPokemonById` searches the player party, then the enemy party;
undefined/null IDs return no Pokemon, while zero is a valid ID. Battler tags use
that method through `getSourcePokemon`. Trapped/Octolock admission and
Seed/Substitute creation in the port previously searched only the field.
They now retain benched and fainted party casters. Missing Trapped/Seed sources
in complete parties follow native decline/no-op behavior. Missing party context
still reports `PR_FEAT_PARTY`.

Substitute is different: native `onAdd` returns before initializing `hp` and
`sourceInFocus` when its caster is absent. Those values are captured explicitly
as `u`, not normalized to zero. Such a doll remains unsupported in the C model,
including when source ID is undefined. This guard is intentional and is not a
claim that absent-source Substitute is fully represented.

## Evidence

The additive producer invokes native `Pokemon.addTag`, with the native battle
RNG captured before GameManager construction and restored after battle start.
No source/framework/helper changes are needed. The natural initial battle is
doubles; the test then sets controlled API contexts rather than executing whole
turns. Six actual Pokemon objects occupy three-member parties on both sides.

The fixed Cartesian matrix retains 768 cases: four tags, two caster sides,
four caster shapes (active, benched, fainted bench, absent lookup), three ID
shapes (ordinary, zero, undefined), four target contexts (clear, Ghost, Grass,
existing Substitute), and two caster levels (30/50). The last distinction checks
that doll HP comes from the caster rather than its recipient.

- **696 fully compared API cases**, plus **72 explicitly guarded native
  undefined-HP Substitute cases**; 1,173,888 comparisons, zero mismatches.
- Input/output Pokemon frames, party order and source identity, tag order and
  nullable fields, boolean result, both RNG states, exact localized messages,
  phase levels/dynamic queues, actual animation recipient and key are retained.
- Twelve additional C controls distinguish incomplete party context from valid
  field fallback, including ID zero. No unsupported flag is cleared to pass.
- Ten compiled faults are caught: each field-only caller, rejecting ID zero,
  falsely flagging known absent sources, losing unknown-party guards, admitting
  undefined-source dolls, using recipient HP, caching the wrong Seed index,
  and rejecting a fainted caster. Eleven malformed captures and four forged
  observations are rejected.
- A fresh Node22 producer run reproduces native records and all nine pinned
  locale files byte for byte. Focused tag/runner address, undefined-behavior
  and leak sanitizer replay passes. Retained Tailwind, Substitute maximum-HP
  and residual/Wish callbacks are checked separately.

Reproduce:

```sh
python3 battle-core/tools/check-tag-party-sources-proof.py
bash battle-core/tools/reproduce-tag-party-sources.sh PINNED_UPSTREAM OUTPUT_DIR
TAG_PARTY_SOURCES_SANITIZE=1 python3 battle-core/tools/check-tag-party-sources-proof.py --replay-only
```

The proof covers these API owners, not arbitrary whole-turn reachability,
every party/copy interaction, or full battle-core/Endless completion. Normal
core/app/custom/save/football checks and the actual ARM build remain mandatory;
only the dedicated Task63 check is excluded. No Windows builder is generated.
