# Interrupted two-turn moves at encounter end

A player can remain underground, airborne or hidden when its ally defeats the
last opponent before the second turn. The stale queued move can target a missing
slot in the next encounter and leave the player hidden.

The encounter owner now cancels unfinished charging moves, including stale bench
state, after post-battle callbacks. It removes only charge-owned queues and tags.
Spent PP, stat boosts, move history, unrelated queues and RNG stay unchanged.
A state-only field event reveals surviving field occupants before rewards.
No save schema or low-level charging rules change.

## Explicit correction to the pin

Source is `8555c08c823b856cbec4eb99ca84ea52a955836d`, locales
`270ed0a2938b2c3e2e34e7de940e84aecadf548b`. This is the requested bug correction,
not a claim that upstream already cancels charges: native BattleEndPhase and
ordinary resetWaveData retain the queue and hiding tags. Native removal APIs
provide the expected corrected frame in the producer, explicitly marked as a
policy override. Native battle RNG is captured before GameManager construction
and restored after natural startup. Only animation callbacks and phase shifting
are controlled for observation; no condition or charge callback is replaced.

## Retained checks

- 28 actual native charge/end/reset/explicit-cleanup observations: Dig, Fly,
  Phantom Force, Dive, Bounce, Shadow Force and Sky Drop; both user slots and
  both targets. Compare queue, tags/ownership, visibility, PP, stages and history.
- 44 real game-controller doubles victories, naturally followed by both single
  and double encounters, with command-point save/load; 7,235 checks including
  all 921 move IDs against the 17 native charging IDs, partial/orphan and bench
  state. Save remains restricted to existing legal command checkpoints.
- 24 real app event-bridge cases covering both player slots and four hiding tags.
- Seven compiled faults: omitted cleanup, clearing all queues, first-slot-only,
  omitted Dive tag, omitted reveal event, refunded PP and reset summon/history.
  Every control exits through a failed assertion.
- Retained chain-wave test: 256 native histories plus wave/save/leave-field
  ownership, 569 checks, passes.
- ASan/UBSan/LeakSanitizer passes for the controller test and instrumented
  gc_battle.c, and separately the actual app bridge and its test. Other linked
  archived modules are ordinary builds; this is not a fully sanitized core.

Run `make -C game-core two-turn-end-host-test chain-waves-test` and
`python3 game-core/tools/check-two-turn-end-faults.py` after building libraries.
The host test and app bridge test run in normal CI. With Node 22 and pinned
upstream dependencies installed, reproduce the native fixture using
`game-core/tools/reproduce-two-turn-end.sh /path/to/upstream /tmp/charge-proof`.
These checks cover this encounter bug, not complete battle-core equivalence or
hardware rendering timing. No Windows builder or release is produced here.
