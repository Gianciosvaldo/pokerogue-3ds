# Mini Black Hole transfer continuation

Later continuation: [Thief/Covet](MOVE-THEFT-VALIDATION.md) adds the move callback
for these canonical families. The observations and boundaries below describe
the original Mini Black Hole slice.

Reference: upstream `8555c08c823b856cbec4eb99ca84ea52a955836d`, locales
`270ed0a2938b2c3e2e34e7de940e84aecadf548b`, Node 22.23.3.

## Runtime boundary

`TurnEndPhase` now invokes the native one-stack Mini Black Hole transfer after
turn-status items and before turn counters. The receiver chooses an active
opponent before choosing among its transferable held stocks. Sticky Hold,
matching stacks/caps, clone defaults, donor loss, modifier refresh/stat changes,
Unburden and exact messages follow the pinned implementation. Source's zero-ID
truthiness quirk is preserved in the battle-core API: its zero-ID donor keeps a
zero stack and skips source callbacks/refresh.

Supported identities: Leftovers, Shell Bell, Focus Band, Quick Claw, King's Rock,
White Herb, Mini Black Hole, all eleven berries, Wide Lens, Multi Lens, Scope
Lens, Toxic/Flame Orb, Soul Dew, Golden Punch, all eighteen attack-type boosters
and Reviver Seed. Both Orbs intentionally share source matchType/cap behavior.
Unknown classes/caps/names remain explicit unsupported contexts. The existing
held-item ancestry classification and broad unsupported accounting are retained.

The game-core adapter projects surviving, removed and newly received stock in
native inventory order, including the remove/reappend of a receiving stack.
The explicit non-transferable flag survives named-field saves; old saves
without the field default to transferable and report a missing field using the
existing migration accounting. Invalid serialized boolean values are rejected.
Soul Dew now maps to the existing native nature-weight implementation.

## Source observations

`tests/item-transfer.producer.test.ts` captures the real modifier apply and
scene transfer helpers, or the owning real TurnEndPhase. It uses naturally
created double fields, varied natures, both sides, Sticky Hold/Unburden, capped
stocks, locks, fainted holders, empty opposing fields and native uint32 IDs
including zero. It captures BattleScene's native battle RNG before GameManager
replaces it, then restores the original method. No source gameplay function is
rewritten. Only presentation/phase shifting is intercepted.

`tools/build-item-transfer-proof.py` transports observed inputs/outputs without
calculating expected outcomes. `tests/item-transfer-test.c` compares inventory
identity/order/owner/stack/lock metadata, HP/stats/tags, all queued levels and
exact localized text, both RNG streams and lazy battle-RNG initialization.
Owning TurnEnd comparisons additionally check counters. Its stock pool excludes
healing/status items to isolate transfer sequencing; direct helper comparisons
cover those item identities. This is not complete turn-effect coverage.

- Direct helper: 8,192 cases at each seed 777/2034, 2,514/2,598 successful
  transfers, zero mismatches.
- Owning TurnEnd: 8,192 cases at each seed 777/2034, 3,643/3,713 successful
  transfers, zero mismatches.
- Native game-core command/inventory/save tests: 395 assertions pass, including
  both transfer directions, locked donor protection, repeated synchronization,
  all seventeen keys, native list ordering, the next legal save point,
  restored command replay, older schema and malformed lock bytes.
- Nine compiled runtime mutations caught; fourteen malformed transports rejected.
- Full game-core suite passes; paired 200-run saved/unsaved digest `d42eb77a`.
  App regressions, retained source corpora and exact-head host/3DS CI are required
  merge gates; their final results are recorded in the pull request.

## Remaining boundaries

This does not implement Thief/Covet, Bug Bite/Pluck, Pickup, broader transfer
classes (including subclass-specific clone semantics), reward acquisition or
manual inventory-transfer UI. A live zero-ID owner remains explicitly refused
by the legacy game-core sentinel adapter; battle-core's raw API handles it.
Noncanonical multiple Mini Black Holes per holder are not a claimed context.

This completes neither final-boss encounter orchestration nor playable doubles,
Endless wide-level representation, phase-two save/recovery or legal Classic
clear acceptance. Hardware and emulator acceptance have not run here.
