# Thief and Covet

Pinned upstream `8555c08c823b856cbec4eb99ca84ea52a955836d`, locales
`270ed0a2938b2c3e2e34e7de940e84aecadf548b`, Node22.23.3.

## Source behavior

`StealHeldItemChanceAttr.apply` overrides the parent apply without parent HP,
condition or move-chance gates. It always draws the global battle float, checks
its own0.3 chance, filters the target's transferable held stock, infers/caches
each candidate's tier, and picks from the highest tier using the user's battle
RNG. The scene transfers one stack. Sticky Hold is checked across sides; donor
and receiver HP do not gate this override. Source's zero-owner truthiness and
remove/reappend inventory order are retained. The exact stole-item message is
queued after a successful transfer. Both registrations remain source edge cases;
the port preserves that pin rather than correcting the cartridge discrepancies.

Tier inference differs by pool: e.g. Leftovers is tier3 in PLAYER and tier4 in
TRAINER; absent WILD entries fall back to PLAYER, then TRAINER. Cached positive
tiers persist across transfers. Tier0 is falsy and re-infers. The canonical
identities below have at most one registered tier in a given pool. Their tier0
identities are pool-invariant, so cloned type aliases do not need independent
mutable cache identities in this bounded representation.

`ModifierData` saves no type tier and reconstructs a fresh type on load. The
runtime cache therefore travels through game inventory and battle projections,
while load resets it. No save schema field is added. New reward stock retains
its option tier; stacking retains the existing type. Generated canonical enemy
stock retains its unique pool tier. Wave1000 Mini Black Holes are a source
exception: appended by `getModifierType`, they initially have no tier.

## Observed evidence

`tools/move-theft-diff.sh UPSTREAM [16384] [777]` generates and replays actual
native calls. Native BattleScene battle RNG is captured before GameManager and
restored after startup. Source gameplay functions are not replaced; only UI
info updates and phase advancement are intercepted. Naturally initialized double
fields supply all four roles; actor IDs (including zero) remain actual inputs.
The producer exercises raw overrides and actual `applyMoveAttrs` dispatch.
Level/ability phases are not drained here. Exact checks cover inventory
identity/order/owner/stack/lock/tier, HP/stats, resulting tags, every queued level
and exact localized text, and both RNG states.

- Seeds777/2034:16,384 complete cases each, zero mismatches. Direct-call successes
  1,067/1,025, with additional successful dispatch cases. Each seed includes
  positive transfers for all17 supported identities and all eight combinations
  of API/dispatch and same/opposite side direction. Coverage floors and complete
  footers are checked; source exceptions are not skipped.
- Supported stock: Leftovers, Shell Bell, Focus Band, Quick Claw, King's Rock,
  White Herb, Mini Black Hole, all eleven berries, Wide Lens, Multi Lens, Scope
  Lens, Toxic/Flame Orb, Soul Dew, Golden Punch, all eighteen attack boosters,
  and Reviver Seed. Locks, capped receiving stock, custom tier values0–5,
  wild/trainer ownership, fainted actors and Sticky Hold/Unburden are included.
- Independent native tier probe:17 identities×three inference pools and204
  actual ModifierData reconstruction cases. Game replay compares those answers
  through real save/load:2,640 assertions exact.
- Actual game commands/inventory/projection/save tests:3,911 checks pass, with
  successful theft in both directions. They include blocked/full/locked stock,
  loaded command replay, new/stacked reward tiers, generated stock and source
  tier reset on load.
- 17 compiled runtime faults caught;19 malformed captures rejected. Existing
  Mini Black Hole guards remain scoped to their original implementation;
  their assertions/counts and source producers are retained.
- Retained Mini Black Hole:8,192 each at seeds777/2034 for API and owning
  Turn End (32,768 cases), all exact; successes2514/2598 and3643/3713. All nine
  previous runtime mutants and fourteen malformed controls still reject.
- Normal game-core tests pass; paired200 saved/unsaved runs both give digest
  `2d205791`. The changed digest reflects newly enabled theft in ordinary runs.
- Retained broad doubles exact:1962/1949/1965; mixed exact:1976/1970/1991/1995.
  Every previous covered ID is retained. Normal app/custom/save and3DS CI remain
  required before merging.

## Open boundaries

This does not close the held-item inventory or all doubles interactions. Unknown
classes/caps/names, unrepresented stock and runtime capacity overflows still
report unsupported paths. Raw callbacks targeting the user itself are explicit
unsupported contexts; Thief/Covet's enabled NEAR_OTHER target excludes the user.
The existing game adapter's live-zero-ID sentinel limit remains visible, while
the battle-core API observes source zero-ID behavior. Full natural move/faint/
switch/fusion/suppression interactions, other item families, Pickup and berry
stealing/eating remain separate work. No full battle-core completion claim.
