# Shared held-item modifier types

Native held-item clone() preserves the same mutable ModifierType object. If a
partial transfer leaves two holders, a later Thief/Covet tier inference through
one clone changes the cached tier seen by the other holder. This matters for
Scope Lens, Leftovers, Shell Bell and King's Rock: their player and trainer pool
tiers differ. Equal-key independently constructed items remain independent.

The pinned native producer exercises a partial scene transfer followed by two
actual StealHeldItemChanceAttr(1) applications, with a separate ally thief so
merging cannot hide the original holder's cache. Eight observations cover four
families in both directions. They retain shared type identity and reuse tier 3
or 4 when the later theft uses a different pool. The producer captures native
BattleScene.randBattleSeedInt before constructing GameManager and restores it
after natural battle start. This is a raw callback sequence, not a full-turn
owning-callback completion claim. Pin: 8555c08c823b856cbec4eb99ca84ea52a955836d.

Battle-core gives split stacks a shared transient type reference. Tier inference
updates all surviving entries with that reference, across both inventories.
Independent factory entries receive distinct references when first cloned.
Game-core preserves references through both inventory projections, including
newly inserted stock. The allocator reserves references held outside the mapped
prefix. Encounter saves exclude references, and load rebuilds fresh type objects
as native ModifierData does; no save format change.

The normal game-core test executes the eight native-derived sequences with a
projection between each operation, checks independent equal-key items, and
verifies reference reset through an actual legal encounter save/reload. Retained
scene transfer, move theft, Mini Black Hole, Grip Claw and Baton source traces
remain required, along with the normal app/custom/save and actual ARM checks.

This repairs shared type/cache identity. Ordered loot-object identity, Pickup's
post-battle selection/removal/interleaving, remaining held families and Eviolite's
class-changing clone remain separate battle-core work.
