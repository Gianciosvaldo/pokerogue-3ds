# Canonical Multi Lens acquisition caller

Source pin `8555c08c823b856cbec4eb99ca84ea52a955836d`, Node 22. This slice
connects reward-core #177's canonical cap-two operation to the game caller and
existing #172 inventory bridge. It adds acquisition/Scene proof; the earlier
#157 battle helper/full-turn and #172 inventory evidence remain separate. It
does not complete broad Task 96, item transfer, or general Scene lifecycle parity.

## Actual boundaries

The raw adapter maps stable party IDs through storage/party order, normalizes
constructor args -1/0 only in a shadow context, and preserves existing keys,
args, stock order and holders. Canonical Wide Lens, Reviver Seed and Golden Punch
normalize their shadow args too. First exact-holder match wins: a full first
Multi stack refuses even if a later duplicate has room. Wildcard stock (-1 in
source, game owner zero) never matches the target. Detached zero stays refused.

The real public party filter refuses full stock before Scene.addModifier using
`modifierType:ModifierType.PokemonHeldItemModifierType.extra.tooMany`. The
separately probed direct Scene path instead emits `battle:itemStackFull` and
applies the source tier-default ball. Source persistent add/stack returns false
after mutation: free choices finish, paid choices error without charging. A
successful Scene add/stack removes orphan wildcard Multi records, resets the
battle projection and recalculates the party. Raw calls, public full refusals,
and direct full replacement leave wildcard stock intact.

Admission refuses malformed canonical stock/args/timers, invalid counts/IDs/order,
unknown/null keys or holders, virtual/detached stock, challenges and inventory
projection beyond 96. Scene admission additionally refuses nonpositive opaque
stocks, fusions, stat-changing stock, species-bound Gimmighoul trackers, form
items and unrelated orphan held records. These guards prevent unproved broad
updateModifiers cleanup/recalculation. Raw ordinary opaque stocks remain opaque
and preserved. An unhandled public call reaches the existing counted fallback;
this is not source parity or a zero-fallback claim. Orb #182 uses a separate API
and does not change this canonical-held contract.

## Source and retained evidence

Four seeds `777 9001 4242 31337`, 2,000 cases each:

| Seed | Raw full/add/stack | Actual Scene inventory changes | Direct full ball | Public full blocked | Actual count changes | Damage position changes |
| --- | --- | --- | --- | --- | --- | --- |
| 777 | 1178/400/422 | 792 | 601 | 607 | 2000 | 12912 |
| 9001 | 1202/400/398 | 805 | 603 | 592 | 2000 | 12897 |
| 4242 | 1210/400/390 | 812 | 598 | 590 | 2000 | 12887 |
| 31337 | 1202/400/398 | 802 | 598 | 600 | 2000 | 12906 |

All 8,000 unchanged owning raw source fixtures replay through the new game
adapter; all 8,000 new actual Scene/SelectModifierPhase cases match. Source calls
real original factories, party selectFilter, addModifier, updateModifiers and
calculateStats. Canonical owned Multi records also round-trip through real
ModifierData with their class, holder, real stack and zero transient stock checked.
UI input callbacks, graphical refresh/audio and phase-end observation are bounded;
cleanup and effect methods are not mocked or replaced by formulas.

The comparison checks all 54 modeled party fields, full fixture inventory/order,
original args/holders, wallet, acceptance/refusal, full fallback ball deltas,
profile preservation and RNG. Source full-message literal presence and the game
message event key are checked; exact rendered-message text and UI timing remain
excluded. Source PokemonData, achievements/gameStats and global/battle RNG stay
unchanged. The effect probe deliberately changes transient turnData, which is
outside that PokemonData invariance claim.

Observed actual hit-count and damage-helper numerical changes are independent
of acquisition/mapping visits. Damage probes use seven raw helper positions,
covering first/repeat/later and negative hitsLeft positions where applicable;
these are not seven executed strikes or a new full-turn claim. Expected numbers
come from the actual source helper, not copied arithmetic.

Raw guards: 29. Caller host: 426 checks, including atomic malformed/raw/Scene
refusals, held stock persistence through legal encounter saves, and actual loaded
count/damage helpers. Wildcard save behavior is game consistency coverage;
canonical owned ModifierData proof does not claim wildcard loading parity.
Five actual C caller mutations fail: wrong target, omitted wildcard cleanup,
incorrect paid-success result, disconnected battle projection, and clobbered
constructor args. Eight malformed transport cases refuse (missing/extra/wrong
footer, oversized request, fractional target/cost, NaN wallet, unknown item).
Four invalid producer-script requests refuse before production.

All established 48 bridge/temp/Wide source suites pass without changing fixture,
producer, comparator, case count or tolerance. The approved stronger canonical
Temp assertion from #180 remains intact. All 16 established apply/candy/ball/
option outputs are byte-identical. Original Mint/Seed/Lens save fixtures load
through their owning host checks (113/130/199 checks).

`make -j2 -C game-core test` passes; paired 200-run digest stays `bc0fbf1c`.
Before/after 1,000 normal/saved runs both end with zero stuck/crashed/timed out,
50,394 turns and paired digest `5b6c9ee2`. Saved paths retain 14,679 round trips,
maximum 11,472 bytes. Every printed reward-choice and stand-in row is unchanged,
including reward fallback 148 runs/185 uses and post-battle 167/1380. This host
consistency result is separate from the source acquisition/effect proof.

Reproduction from repository root:

```sh
. /workspace/shared/activate-parity.sh
make -j2 -C game-core all
make -j2 -C game-core test
for seed in 777 9001 4242 31337; do
  game-core/tools/multiacquirediff.sh /workspace/shared/upstream-lane-c 2000 "$seed"
done
game-core/build/mint_host_test /workspace/validation/lane-c/mint-legacy.bin
game-core/build/seed_host_test /workspace/validation/lane-c/eight/reviver/legacy.bin
game-core/build/lens_host_test /workspace/validation/lane-c/eight/wide/legacy.bin
game-core/build/gcplay --auto 1000 --jobs 8
game-core/build/gcplay --auto 1000 --jobs 8 --save-test
```

Evidence directory: `/workspace/validation/lane-c/three/multi-acquire`. Earlier
failed new-fixture builds/disconnected public dispatch are excluded; the final
C dispatch and numeric/identity comparison pass. No other folder was edited.
