# Quash and the native move timing queue

Pinned upstream `8555c08c823b856cbec4eb99ca84ea52a955836d`, locales
`270ed0a2938b2c3e2e34e7de940e84aecadf548b`, Node 22.23.3.

`ForceLastAttr.apply` always queues `moveTriggers:forceLast`, names the target
with its battle affix, and calls `phaseManager.forceMoveLast` using cached
Pokemon identity. This override omits the parent's HP gate and does not run
move conditions or chance. Quash's move-level double-battle and target-not-acted
conditions are separate and retain their existing caller. The queue helper
changes only the first matching queued move, in its current queue order.
It does not rewrite all moves or resolve the target again from a field slot.

The port now queues the native message and marks that first move LAST. Existing
priority/speed sorting then places those moves after the other timing groups.
No state/save representation changes are required.

`tools/quash-diff.sh UPSTREAM` reproduces the actual native API, owning turns
and compiled fault checks. Native BattleScene RNG is captured before GameManager
construction and restored after battle startup. UI/automatic advancement are
intercepted at the API boundary; gameplay formulas and queue methods are native.

- All 8,192 API cases and 15,936 operations match. The matrix preserves the
  existing After You test arrangements while calling the registered Quash
  attribute: four users/targets, living/fainted and acted/unacted actors, empty
  queues, duplicate owners, every initial timing, consecutive calls and field
  swaps after move construction. It compares exact messages, all queue levels,
  markers, dynamic queue order/owner/timing, actor HP/acted state and both RNG.
- All 8,192 owning doubles turns match, with zero unsupported cases or source
  exceptions. A passthrough spy observes 946 actual successful native callbacks.
  Quash competes with normal and positive-priority damage at wide levels,
  including Substitute, switches, knockouts and the retained ally/spread/faint
  controls. The strict wrapper requires every source turn while retaining all
  established state/text/phase/RNG comparisons and coverage floors.
- Eleven compiled faults are caught, including invented HP/acted gates, missing
  or incorrect text/affix, wrong target, slot-based identity, FIRST instead of
  LAST and modifying every matching move. Missing forcing also causes actual
  owning-turn mismatches. Twenty-one malformed captures are rejected.

These bounded captures do not close all doubles, arbitrary queue-capacity,
form/identity interactions or remaining move callbacks. Normal game-core,
app/custom/save sanitizer tests, retained covered IDs and exact-head 3DS CI
remain merge gates. Only dedicated custom-only Task63 is skipped.
