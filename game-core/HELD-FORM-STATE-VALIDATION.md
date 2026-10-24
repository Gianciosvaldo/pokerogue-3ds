# Form-item inventory and save reconstruction

Target: upstream `8555c08c823b856cbec4eb99ca84ea52a955836d`, locales
`270ed0a2938b2c3e2e34e7de940e84aecadf548b`, assets gitlink
`87426a79611f9d212c4dc8af58e2834a05b93725`.

PR #399 supplied the [native callback and scene selector](../battle-core/HELD-FORM-VALIDATION.md).
This follow-up projects canonical normal/rare form items from game inventory into
that class and preserves the native active state through saves. It does not yet
add the party-menu toggle or general reward/phase execution owner. Those core
owners are supplied by the [subsequent native owner proof](FORM-ITEM-OWNER-VALIDATION.md);
the app menu wiring remains separate.

The native constructor's active flag is true by default. Game inventory uses
`form_inactive`, whose zero default keeps existing items active. It occupies
existing structure padding. New entries explicitly reset the flag, including when
an inactive item previously occupied the reused inventory slot. Both item factories bridge as nontransferable
`PokemonFormChangeItemModifier`, with exact item identity and active state.
Only known identities with one real stack, no virtual stock and an attached
holder are admitted. Invalid, wildcard, detached and orphan entries stay opaque.
Malformed party/inventory counts refuse before projecting a partial inventory.

The reward context now receives the item's actual active state. Rebuilding the
battle inventory refreshes the explicit six caught-species facts used by native
form predicates. A missing system profile leaves those facts unknown.

## Native evidence and compatibility

`battle-core/tests/held-form-items.producer.test.ts` invokes the real normal and
rare factories, `clone`, `getArgs` and JSON `ModifierData.toModifier` restoration.
The native battle RNG method is captured before constructing GameManager and
restored after natural startup. The original full callback proof remains intact.

The 2,544 constructor observations cover all 159 sparse identities, both factories,
four player/enemy holders and active/inactive state. The C reader compares each
native clone and restored DTO against actual inventory/save projections, including
ownership, class, stock, item, active state and constructor transfer lock. It also
checks reward projection and inventory synchronization. There is no invented C
clone API standing in for the native implementation.

The existing named-field save schema gains an optional `form_inactive` field.
An absent field means active. Its presence and the earlier `eviolite_base` field
are independent, so the reader builds its expected schema by field name rather
than assuming optional fields form a trailing prefix. All 2,544 cases exercise
the new schema plus all three missing-marker combinations in both inventory
sections, with zero dropped state. Legacy saves cannot express inactive state;
they retain their previous active behavior.

A real save from PR #399's prior writer additionally proves compatibility:

- Writer revision: `6d366f9863b81c8ce16225bc84434286388d2502`.
- Writer `game-core/src/gc_save.c` SHA-256:
  `ac162dd67aa6eb036a1cd9a1baaad77cf11de6da1d1dfc3c579318224f3cf3a7`.
- Fixture `tests/fixtures/held-form.pr399.save`: 11,572 bytes, SHA-256
  `61d987b4874e58ac40b22cd3b41c4ccef14128c3137d1dfb5494d167d476a092`.
- Generator retained as `tests/held_form_legacy_writer.source.txt`: copy it to
  `work/held-form-legacy-writer.c` in that revision's checkout and compile against
  that checkout's game/reward/AI/run/battle libraries. It uses the actual old
  structs and writer together; no mixed-header ABI is used.

The fixture includes both form factories, an enemy form item and the transferred
Eviolite class marker. All load without dropped state. Additional checks cover
64 caught-profile masks, missing profile, malformed marker bytes/widths/duplicates,
wrong item families, invalid bridge identities and invalid party counts.
The shared football roster codec receives an inactive Mega item round trip and
an absent-marker compatibility check as well.

## Reproduction and limits

```sh
game-core/tools/held-form-state-diff.sh /path/to/pinned/upstream
make -C game-core held-form-items-host-test
make -j4 -C game-core test
```

The retained constructor transport is losslessly compressed. Its strict reader
requires every case and complete identity/factory/holder/active geometry.
Compiled controls alter bridge activation, orphan admission, dex bits, reward
activation, writer/reader activation, legacy defaults, optional schema selection,
boolean validation, duplicate markers and wrong-family writes. Each must fail;
malformed transports must also be rejected.

This evidence does not close general acquisition/removal, duplicate reward
fallback, native party-menu eligibility/activation, form-phase state execution,
fusion/Transform behavior or arbitrary mixed histories. The existing Griseous
Core acquisition owner remains separate. Normal app/custom/save, core/football
and actual ARM/stack CI remain required; only dedicated custom Task63 is skipped.

The final native reproduction passes the entire 124,416-callback/1,664-route
matrix and the new 508,345-assertion inventory/save replay. Frozen and fresh
constructor transports agree after normalizing only actor IDs; no other inputs
or observations are normalized. The final local full game-core suite passes, including all twelve compiled
faults and five malformed transports, with matching saved/unsaved digest
`5073f991`. Exact-head normal CI acceptance remains required.

The normal system/save app corpus is extended from 100 to 300 seeded runs,
retaining the original deterministic prefix and all coverage floors. The smaller
corpus can choose Release at every full-party prompt and miss cancellation.
The extension also invokes the existing directed trainer-music fixture. Its local
run covers cancellation and passes all model checks.
