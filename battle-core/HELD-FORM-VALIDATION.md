# Held form-item callback and scene selection

Pinned upstream: `8555c08c823b856cbec4eb99ca84ea52a955836d`; locales
`270ed0a2938b2c3e2e34e7de940e84aecadf548b`; assets gitlink
`87426a79611f9d212c4dc8af58e2834a05b93725`.

`PokemonFormChangeItemModifier.apply` temporarily deactivates an active item
when its argument is false, calls the scene's item-form trigger, and restores
the previous active state. Its override omits the parent's holder/condition
gates. A direct call on a foreign-holder modifier can therefore succeed through
another matching item in the scene. The scene predicate searches the player
inventory even for an opposing Pokémon. These source behaviors are intentional
in this port.

`src/modifier/modifier.ts`, `src/battle-scene.ts`, the form-change triggers and
the initialized species registry supply 159 sparse item identities and 416
ordered registrations across 137 species. Four registrations have an additional
default-form predicate (Meowstic or Toxtricity). Conditions comprise 34 ability
predicates (Multitype/RKS System) and six caught-species predicates. The latter
require explicit profile facts; missing facts report unsupported behavior rather
than assuming the species has been caught. No registered condition has an
enforcement callback at this pin.

The new class is appended to preserve existing IDs. Canonical items have one
real stack, no virtual stacks, a known item identity and a nonnegative holder,
and are nontransferable. Invalid identities stay outside the turn whitelist.
The active field occupies existing modifier padding. Other form triggers keep
their existing implementation; item triggers now enter this selector.

## Native observations

The producer runs the real pinned GameManager and restores the native battle RNG
method after natural battle startup, having captured it before construction.
It does not replace the native form predicates or scene selection. Observer
wrappers call each original predicate unchanged. Each observation checks that
the global and battle RNG states are unchanged.

The retained 124,416 callback cases include all prior 70,912 observations:

| Cases | Inputs |
| --- | --- |
| 53,248 | All 416 registrations × 32 active/argument/holder/actor-side/inventory-side combinations × four ability/caught gates |
| 1,536 | Six dex predicates × 32 combinations × eight none/one-hot/all caught masks |
| 12,800 | Four default-form predicates × 32 combinations × four gender inputs × all 25 natures; valid genderless/male/female plus the earlier raw gender input |
| 3,328 | All registrations × eight direct calls on foreign holders with a matching player-inventory item |
| 53,248 | Original registration/gate matrix with the selected actor actually removed from the native field container |
| 256 | Both Ultra Necrozma reverse registrations × 32 combinations × neither/lunar/solar/both companion items |

All 416 registrations have positive native eligibility. Exactly 405 can be
selected. Eleven reverse registrations share their entire eligibility predicate
with an earlier simple registration, so native first-match ordering shadows
them: Pikachu, Eevee, Zygarde and eight Alcremie flavors. A structural certificate
requires precisely those eleven registrations; they remain in the tables and
tests. No registration is skipped to achieve the selected count.

For every registration, a positive and a negative callback case also run both
native immediate and delayed scene dispatch with a preexisting message sentinel:
1,664 routing observations. C compares the complete queue levels, order, selected
form, phase kind and Pokémon reference through its public form dispatch entry
points. The main matrix independently compares predicate results, selected rule,
temporary-active restoration and both RNG states.

The producer additionally audits 2,544 native constructor/clone/ModifierData
round trips: every identity × normal/rare factory × four holders × active/inactive.
These are native serialization observations, **not** proof of the game-core save
bridge, which this change does not implement.

## Reproduction and controls

With Node 22 and the prepared pinned upstream checkout:

```sh
battle-core/tools/held-form-items-diff.sh /path/to/upstream
make -C battle-core held-form-items-host-test
```

The transport builder copies native inputs and observations; it does not compute
the expected selection. The reproduction script regenerates the registration
facts into a temporary directory and requires them to match the production
tables. The compressed retained transport is lossless and replayed in normal CI.
The replay passes 9,327,622 assertions. Strict readers reject
missing/duplicate/invalid records and trailers. Compiled
controls alter inventory side, ownership, activation, ability/dex/default-form
gates, selection order, Necrozma priority, restoration, phase identity, Pokémon
reference, RNG consumption and immediate/delayed routing; all 17 faults and eight malformed transports are rejected.
The retained Macho Brace proof also now checks the distinct battle RNG and
rejects an injected battle RNG call.

## Remaining boundaries

This proves callback selection and phase queuing within the listed matrix. It
does not prove execution of the queued form phases, profile updates, generic
reward acquisition/removal, party-menu activation, active-state persistence,
fusion/Transform interactions, arbitrary property-grafted modifier subclasses,
challenge overrides, or unbounded inventories/levels. General form-item lifecycle
work remains open. In particular, ordinary game inventory items are not yet
projected into this class; no player-facing menu/save completion is claimed.
Normal app/custom/save, core/football and actual ARM/stack CI remain required for
each PR. Only the dedicated custom Task63 job remains excluded.

Local full game-core tests pass with identical saved/unsaved digest `5073f991`.
The complete native regeneration script passes, including immutable generated
registration-table comparison. Exact-head normal CI acceptance is still required.
