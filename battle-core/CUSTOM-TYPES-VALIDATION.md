# Custom Pokémon types (`customPokemonData.types`)

Source: `8555c08c823b856cbec4eb99ca84ea52a955836d`; Node 22.

## What was missing

`Pokemon.getBaseTypes()` reads `customPokemonData.types` before the form's own types, and
`fusionCustomPokemonData.types` before the fusion half's:

```
firstType  = customTypes[0] ?? form.type1
secondType = customTypes[1] ?? form.type2
fusionType1 = fusionCustomTypes[0] ?? fusionForm.type1   (same for type2)
```

Weird Dream (`[null, randomRegularType]`) and Clowning Around (two random types, on the party member and, for
a fusion, on its fusion half) write those arrays. `pr_pokemon` had `custom_ability` but no custom types and
`pr_get_types` ignored them, so neither encounter could be ported 1:1.

## Change

- `pr_pokemon.custom_types[2]` and `fusion_custom_types[2]`. A slot is `0` for null/absent (the form's type
  shows through, as `??` does) and otherwise `PokemonType + 1`. Only the 18 regular types can be stored
  (`RegularPokemonType | null` upstream). Zero-initialised Pokémon, which the engine tests build with `memset`,
  and saves from before this change therefore read as "no custom types".
- `pr_get_types` (`src/pokemon.c`) applies them to both halves, ahead of the existing fusion rule
  (`fusionType2 !== null && fusionType2 !== firstType`, else `fusionType1 !== firstType`). `summonData.types`
  (Transform and similar) still wins over them, as upstream's early return does, and Tera, the added type and
  the UNKNOWN-removal behave as before.
- game-core: both arrays are saved in a `custom_types` section of their own (`gc_save.c`), like the fusion halves,
  so an older save loads whole, with none, and counts nothing as dropped (the first attempt put them in the
  Pokémon field list; `wide_level_host_test` caught the dropped-field count on the v2 fixture). A damaged
  schema or a slot above `FAIRY + 1` is refused on load; DNA Splicers copies the donor's `custom_types` into the primary's `fusion_custom_types`
  (`fusionCustomPokemonData = pokemon.customPokemonData`) and Unsplice clears them
  (`fusionCustomPokemonData = null`).

## Evidence

`tests/custom-types.producer.test.ts` runs the actual `Pokemon.getTypes()` on a real `PlayerPokemon` whose
species, form, fusion species/form, `customPokemonData`, `fusionCustomPokemonData` (absent, or an object),
`summonData.types`, `summonData.addedType` and Tera state are randomised; it records the answer for 16 flag
combinations (`includeTeraType`, `returnOriginalTypesIfStellar`, `bypassSummonData`, `ignoreThirdType`) per
case. Custom arrays are empty, one slot, or two slots, each slot null or one of the 18 regular types.
`tests/custom-types-test.c` rebuilds the same Pokémon and compares `pr_get_types`; it exits 2 if the corpus
does not actually exercise the field (fewer than 10% of cases with custom types, 5% with fusion custom types,
5% where they change the answer).

| Seed  | Cases | Rows  | With custom | With fusion custom | Custom changes result | Failures |
|-------|-------|-------|-------------|--------------------|-----------------------|----------|
| 777   | 4000  | 64000 | 2051        | 984                | 1314                  | 0        |
| 9001  | 4000  | 64000 | 1943        | 984                | 1275                  | 0        |
| 4242  | 4000  | 64000 | 1955        | 984                | 1311                  | 0        |
| 31337 | 4000  | 64000 | 2012        | 932                | 1297                  | 0        |

### Planted mutations

`tools/custom-types-mutations.py <trace>` rebuilds `custom-types-test` with one change to the real
`pr_get_types` and replays the seed-777 trace. The intact build exits 0; every mutant exits 1:
ignore the first custom slot, ignore the second, off-by-one on either, ignore either fusion slot,
fusion half reading the primary's custom data, custom types applied only without a fusion, custom types
overriding `summonData.types`, and a null slot read as Normal.

In game-core, `tests/fusion_host_test.c` checks the donor-to-fusion copy, the Unsplice clear, a save/load
round trip of both arrays, and refusal of an out-of-range slot in either array. Removing the copy, the clear,
the saved fields or the load validation each makes it fail (run by hand, then restored).

## Not covered

- Illusion with custom types: `getBaseTypes` reads the Pokémon's own `customPokemonData` even while an
  Illusion shows another species; the C code does the same, but the corpus keeps Illusion inactive.
- `customPokemonData.types` holding a non-regular type (UNKNOWN, STELLAR) is not representable. Upstream
  cannot produce one either (`RegularPokemonType | null`), and its save migration turns UNKNOWN into null.
- No encounter writes these fields yet; that is Clowning Around and Weird Dream, as separate PRs.

## Commands

```
battle-core/tools/custom-types-diff.sh PINNED_UPSTREAM [N=4000] ["777 9001 4242 31337"]
python3 battle-core/tools/custom-types-mutations.py battle-core/build/custom-types/custom-types-4000-777.txt
make -C game-core fusion-host-test    # or: make -C game-core build/fusion_host_test && game-core/build/fusion_host_test
```
