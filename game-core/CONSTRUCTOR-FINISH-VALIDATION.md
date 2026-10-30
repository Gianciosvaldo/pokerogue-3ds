# Joined boss constructor and finish APIs

Pinned source `8555c08c823b856cbec4eb99ca84ea52a955836d`, locales
`270ed0a2938b2c3e2e34e7de940e84aecadf548b`, Node22.

The earlier fused-boss proof ended at moveset-generation RNG. This follow-up
joins run-core base generation, the public game-core moveset adapter, run-core
finish (including the later boss-IV roll), refresh, and the explicit empty-item
party/stat/threshold owner. It validates an existing chain; no gameplay guard is
removed and no new moveset fallback is admitted.

The native producer invokes actual `addEnemyPokemon`, observing its real moveset
method with a passthrough hook, followed by actual `updateModifiers` and
`regenerateModifierPoolThresholds`. Battle RNG is captured before GameManager
construction and restored after startup. Presentation updates are replaced;
source/framework/helpers stay clean. Stats are captured separately after the
post-constructor modifier update: the boss IV roll initially leaves constructor
stats stale. The C proof also observes party stats at the first external
threshold-RNG call using a passthrough wrapper. It catches a lost early update
even if the later duplicate stat calculation repairs the final state.

## Retained inventory and results

1,744 exact cases, 205,324 comparisons, zero mismatches:

- All 804 original Spliced boss constructor observations remain byte-identical
  before/through moveset generation and constructor finish RNG. The transport
  rejects any change to those retained records.
- Another 804 cases apply the same inventory in ordinary Endless: 17 species,
  levels 100/65,536/70,000, singles/doubles context, repeated seeds and zero/four
  Ability Charms. The additional 136 cases cover level 65,535 in both modes.
- Compare base identity, IVs, fusion metadata, nature, ability, Tera type and boss
  segments; exact moves/PP; RNG before/after generation, constructor finish and
  threshold generation; final IVs, EXP, HP, stats, passive/type getters and fresh
  PP through refresh. No enemy-moveset fallback or source exception occurs.
- Ten compiled faults are caught: missing/wrong boss-IV rolls, ignored Ability
  Charms, restored fused-boss fallback, move order, IV projection, PP metadata,
  threshold RNG draws and the early deferred stat update. Twelve malformed
  captures and five structurally legal forged observations are rejected.
- Fresh native JSON and generated C records reproduce byte for byte. Scoped
  ASan/UBSan/LSan covers base/finish, moveset adapter/generator, mon projection,
  encounter owner and the passthrough observer; supporting archives are not
  sanitized by that focused run.

```sh
python3 game-core/tools/check-constructor-finish-proof.py
bash game-core/tools/reproduce-constructor-finish.sh PINNED_UPSTREAM OUTPUT_DIR
CONSTRUCTOR_FINISH_SANITIZE=1 python3 game-core/tools/check-constructor-finish-proof.py
```

## Scope still open

These are controlled constructor/API inputs. Species, level, boss and mode are
supplied directly; wave1 forced bosses and wide levels are not claimed as natural
encounter selection. The doubles contexts contain one constructed enemy, not a
complete generated doubles party. Empty configured items omit random held-item
selection. Threshold RNG is compared, not the complete threshold table.

No battle, EXP award, reward selection, executed learning/evolution, save/resume,
terminal transition or display playback is chained here. Names/power-query
metadata in the retained JSON do not add display/power claims to this follow-up.
That full integration and source branch inventory remain necessary for Endless
acceptance. Trainer/challenge generation, post-moveset form callbacks and explicit
passive overrides still need review. These bounded records do not establish
battle-core completion. Normal core/app/custom/save/football and actual ARM/stack
checks remain required; only dedicated Task63 is excluded. No builder is made.
