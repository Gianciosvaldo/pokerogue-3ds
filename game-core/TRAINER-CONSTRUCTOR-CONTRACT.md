# Constructor integration contract

Source pin: `8555c08c823b856cbec4eb99ca84ea52a955836d`.
This records shared constraints for the trainer, ability/form/shield, fusion and
Tera work; it does not certify completion of those lanes.

- `Trainer.genPartyMember` keeps base construction, moveset generation,
  constructor finish and `postProcess` inside one member seed offset. Restore
  ambient RNG after the whole member. Regenerating moves is not a second
  constructor finish or a second trainer IV reroll.
- `BattleScene.addEnemyPokemon` creates the enemy, optionally boosts IVs when its
  **boss argument** is true, then runs `postProcess`. Callbacks can change
  ability/gender/form/ball/segments, regenerate moves and force a third-slot move.
- Trainer IVs reroll after the first moveset without recalculating stats. Plain
  ability/form/gender assignments and `EnemyPokemon.setBoss` also do not
  recalculate stats. A later generation can therefore use original constructor
  stats with newer IVs or forms. Preserve actual generation-time stats; the fresh
  factory's automatic recalculation is not a general regeneration contract.
  Source methods that explicitly recalculate stats require their own ordering.
- Trainer `config.isBoss`, Pokémon `isBoss()` and stored passive access are
  separate. The reviewed Gym config permits eggs while its fresh Pokémon has
  no shields/passive. Do not infer one flag from another.
- `willTerastallize` uses the initial team index and the trainer's dynamic
  `instantTeras` policy. Planned Tera affects generation before actual Tera state
  is applied. Current field position and the already-terastallized flag are not
  substitutes. Runtime queued Tera, moveset prediction and save/resume must agree.
- `gc_generate_moveset` remains a guarded fresh-constructor API. Its rejection
  must leave the Pokémon and RNG untouched. Unsupported postprocessing, forms,
  fusion/passive or Tera contexts remain visible until their validated owner is
  integrated. Preserve the existing prefix-only API and callback failure atomicity.

The retained ordinary and plain-Gym proofs are in
[TRAINER-CONSTRUCTOR-VALIDATION.md](TRAINER-CONSTRUCTOR-VALIDATION.md) and
[GYM-EGG-CONSTRUCTOR-VALIDATION.md](GYM-EGG-CONSTRUCTOR-VALIDATION.md).
