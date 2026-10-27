# Controller and app storage for long turns

The native turn retained by [the trace-capacity proof](../battle-core/TRACE-CAPACITY-VALIDATION.md)
produces 555 C presentation events. The default controller buffer holds 384.
Core chunk delivery alone therefore did not let the app show this whole turn.

`gc_run` now accepts a caller-owned storage reserve callback. It retains its
384-entry inline buffer for existing clients; opted-in readers use
`gc_run_battle_event_data`. Appending reserves space when necessary, preserves
the existing prefix, and publishes the same ordered `GC_EV_BATTLE` ranges.
Neither the core's 192-entry chunks nor the default controller capacity changes.
Allocation failure, an invalid callback result and integer overflow remain
explicit failures. After a lost suffix, the controller does not resume appending
through a later reserve attempt as if the sequence had been complete.

The app owns one reusable heap arena for its single active run. It grows
geometrically, copies the inline prefix on first use, and retains the prefix
when `realloc` moves it. New and restored runs bind this owner before their
events are read. The arena and callback are transient and are not serialized;
the arena is freed after the app stops driving its run. All four app readers
use the accessor: combat observation, duplicate send-out detection, capture
detection and event conversion. Battle-end charge visibility snapshots use
the same append path, including beyond the inline limit.

## Retained checks

`game/tests/event_store_test.c` first runs the existing native trace-capacity
comparison, including its 2,034 state/text/RNG/phase checks. Only after that
comparison passes does it reuse the collected C events:

- Three actual controller appends (192, 192, 171) preserve every event byte and
  publish contiguous ranges. Default clients still report overflow at 384.
- The actual app converter, with the captured English locale inputs, matches
  all 144 native texts and 40 hit HP snapshots (435 screen events in total).
- Forced moving allocation, reuse after new/load, failure before and after
  growth, invalid callback outputs, integer overflow and a Dig battle-end
  snapshot after event 555 exercise raw storage contracts. These guards are
  not additional native gameplay evidence.
- An actual older PR399 save loads without transient storage pointers; after
  rebinding, the complete retained turn is stored again.

The storage runner performs 7,944 checks. The actual run driver additionally
plays 10,000 runs with valid external storage forced even for short turns and
the inactive inline array cleared before app readers run. Save/resume runs
(300 with system data) and Custom Run save/resume runs (100) use the same mode.
The existing normal app/custom/save tests and coverage floors are retained.

`game/tests/check_event_store.py` compiles nine faults in the real controller
and storage owners, and three in the real app driver. A successful build is
required; compilation failure does not count as detection. The unmodified
runner must pass from the same working directory before faults are tested.
`EVENT_STORE_SANITIZE=1` instead instruments the changed controller run/battle
owners and app storage/converter with ASan/UBSan/LSan. Supporting archive units
remain unsanitized; this is a scoped sanitizer check.

```sh
make -C game/tests event_store_test runtest
game/tests/build/event_store_test
python3 game/tests/check_event_store.py
EVENT_STORE_SANITIZE=1 python3 game/tests/check_event_store.py
game/tests/run_host_tests.sh
```

This closes the demonstrated 384-event app handoff failure. It does not remove
other bounds: controller notification queues, app conversion/text/animation
batches, other engine stores and available heap still have limits. The bounded
native turn and app runs are not a claim of complete battle-core parity. The
source capture, producer, expected observations and fault controls from the
trace-capacity proof are unchanged. No Windows builder is released by this work.
