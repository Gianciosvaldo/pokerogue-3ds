# Intrinsic held-item constructor locks

Pinned source: `8555c08c823b856cbec4eb99ca84ea52a955836d`; locales
`270ed0a2938b2c3e2e34e7de940e84aecadf548b`.

The game item store defaulted constructor-locked items to transferable and
trusted that obsolete value on load. Battle projection already repaired some
classes, but left form items and Macho Brace transferable. Knock Off could
therefore select an item the source excludes, draw RNG and report an unknown
item. Menu transfer/discard could also offer intrinsically locked items.

`gc_item_key_locked` transports the pinned constructor property for the two
form-item generators, vitamins, Gimmighoul coins, Shuckle Juice, Old Gateau
and Macho Brace. New/matched acquisitions and native-style save reconstruction
set that property; battle projection and menu callbacks also guard legacy
false flags. Invalid boolean bytes still reject before reconstruction. Explicit
saved locks remain preserved for other identities, including encounter-granted
Mini Black Holes. Ordinary Eviolite/species items remain transferable.

This adds no modifier class or form-effect admission. In particular, full
PokemonFormChangeItemModifier activation/deactivation, generic form phases and
Macho Brace's incrementing-stat owner remain separate prerequisites. The
missing concrete-class inventory is not closed by a lock-property fix.

## Native proof

The producer starts natural doubles, captures native randBattleSeedInt before
GameManager construction and restores it after startup. It uses actual item
factories, ModifierData reconstruction, the actual PartyUiHandler.getItemModifiers owner
and RemoveHeldItemAttr.apply. No source effect oracle is duplicated in the
transport builder. The native leaf and C bridge both preserve RNG with a
locked-only inventory; this does not establish complete-turn queue parity.

All 2,632 required observations are retained: both form generators over all
159 numeric enum members (including factory-accepted NONE), six vitamin stats,
two Juice variants and the other three locked identities, four field owners
and both active settings. Fresh/restored lock, UI exclusion and Knock Off
refusal match. C adds 39,495 bridge/menu/save checks, with zero dropped entries,
legacy false metadata and transferable/explicit-lock controls. The source's
raw Scene.tryTransferHeldItemModifier/tryDiscardHeldItemModifier helpers do
not themselves gate the lock; the party UI filters first. The guarded C
functions are those UI callbacks, not claims about unrestricted raw helpers.

Six actual compiled faults (new slot, load, projection, transfer, discard and
Macho Brace omission) and eight malformed transports must fail. The ordinary
battle/game/app/custom/save and actual ARM stack/build gates remain required;
only custom-only Task63 stays skipped. No downloadable builder is published.

```sh
. /workspace/shared/activate-parity.sh
game-core/tools/held-lock-diff.sh /external/pinned-upstream
python3 game-core/tools/check-held-lock-proof.py
make -j2 -C game-core test
```

The retained host checks exposed old expectations for unlocked vitamin saves
and menu transfers. Both vitamin quantities and the recipient IV boundary are
kept, now requiring the source UI refusal, unchanged ownership/HP and canonical
lock after restore. No fixture case or expected source field is removed.

Full local game-core tests pass with saved/unsaved digest `5073f991`.
The four retained money seeds still pass all 16,000 direct/public comparisons.
