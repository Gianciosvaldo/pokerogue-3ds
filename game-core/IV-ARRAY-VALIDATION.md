# Shared Shedinja IVs through the Training Session reward owner

Source: `8555c08c823b856cbec4eb99ca84ea52a955836d`, locales
`270ed0a2938b2c3e2e34e7de940e84aecadf548b`, Node 22.

The explicit IV argument in `Pokemon`'s constructor is assigned by reference.
Consequently Shedinja and its parent initially share **both** `ivs` and
`usedTMs`, as independent arrays. Training Session's light-training reward
writes two IV entries in place. Those changes reach a live sibling, even while
the selected Pokémon is temporarily outside the party. `PokemonData` JSON
reconstruction separates the arrays, so later training affects only the
selected Pokémon after reload.

Before this change, C materialized only the initial IV values. In the first
native case, training changes `[0,1,5,10,20,30]` to
`[10,1,15,10,20,30]` in Ninjask **and** its Shedinja sibling; C left the
sibling's HP IV at `0` instead of `10`. A runtime IV-array token now preserves
that reference through the party and retained Training Session object. It is
independent of Pokémon IDs and the TM-array token, and is omitted from saves.
The light reward publishes the writes before recalculating the selected mon.
Sibling stats follow the ordinary later `updateModifiers` recalculation;
writing an array does not itself change those separate stat arrays.

## Evidence and scope

Sixteen native cases use four IV profiles (mixed increment thresholds, all 30,
and all 31), training either the parent or child, with and without an actual
`PokemonData` JSON roundtrip. The producer invokes actual Shedinja evolution,
`TrainingSessionEncounter.options[0].onOptionPhase`, `setEncounterRewards`, and
the installed reward callback under `executeWithSeedOffset(wave * 1000)`.
Native battle RNG is captured before GameManager construction and restored
following battle startup. Presentation is bypassed, as is
`initBattleWithEnemyConfig`: this is a **reward-owner** proof, not a complete
Training Session encounter, enemy-generation, battle or reward-shop proof.

The C replay creates the siblings through the actual evolution owner and uses
real command saves with zero dropped fields. It prepares the retained member
and party arrangement from the native callback input, including that input's
captured RNG, then invokes `gc_training_return`. It compares IVs, stats, HP,
PP, identity, profile changes, stock and both RNG streams. The earlier option
entry/battle-installation RNG is not calculated by this proof or claimed equal.
It is not silently substituted as an expected callback result. Native and C
callback outputs remain exact, with every stub/unsupported counter checked.

The 26,888 checks also include explicit C invariants for party removal,
retained-object lifetime, a colliding unrelated Pokémon ID, independent TM/IV
identities and stat-array update order. These invariants are distinguished from
the sixteen native owner observations. The checker rejects eight compiled
implementation faults, ten malformed captures, and four forged observations.
A fresh pinned capture and generated fixture reproduce byte-for-byte.

The existing 2,400-case Shedinja/TM replay remains unchanged and passes. Scoped
ASan/UBSan covers `gc_mon`, `gc_training`, `gc_save` and the new replay; supporting
archives are not instrumented. Normal app/custom/save tests and the actual ARM
build remain required on the PR head. No Windows builder is produced.

```sh
make -C game-core iv-alias-host-test shedinja-host-test
python3 game-core/tools/check-iv-alias-proof.py
game-core/tools/reproduce-iv-alias.sh /path/to/pinned/upstream /tmp/iv-proof
```

Only the native light-training IV mutation owner is covered here. Constructor
copies outside the Shedinja path, arbitrary Challenge mutations, legacy mystery
handlers, held-item/IV-cap interactions and full encounter transitions remain
separate inventory obligations. New constructors and JSON loads start with no
runtime alias. Genuine array-identity counter exhaustion remains visible as an
engine limit, rather than wrapping identities.
