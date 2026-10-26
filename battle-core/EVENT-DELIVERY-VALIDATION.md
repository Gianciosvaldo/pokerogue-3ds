# Complete battle-event handoff

Reference: upstream `8555c08c823b856cbec4eb99ca84ea52a955836d`, locales
`270ed0a2938b2c3e2e34e7de940e84aecadf548b`, assets
`87426a79611f9d212c4dc8af58e2834a05b93725`.

## Native counterexample

A doubles turn with Population Bomb and three Instruct commands hits a Weak Armor
Regirock forty times. The source continues to show Weak Armor and capped-stat
messages after the stats reach their limits. The port finished the turn with
matching HP, PP, histories, RNG and phase order, but its 192-event buffer dropped
the remainder of the presentation. Game-core's later copy could not recover it.

The producer uses controlled species/abilities, level1000, IV15/nature0, natural
HP/stats and initial stat stages (user ATK-6/ACC+6, speed stages0/-1/-3/-2).
Two Blissey have Natural Cure, two Regirock have Weak Armor. The source executes
all four commands in natural speed order, including the speed changes. There
are no history injections or HP/PP repairs after the turn starts. These are
fixture inputs, not a claim about ordinary generated Regirock abilities or AI
choosing the three Instruct commands.

Sandstorm is retained as an explicit initial weather input. Both native RNG
streams are fixed after startup. Native battle RNG is captured **before**
GameManager construction and restored after battle startup. The long turn
exceeds the framework's whole-`to()` 20-second limit, so the producer advances
every real phase using the framework's unchanged per-phase driver/wait; no
source/framework code or gameplay phase is replaced or skipped.

The final native capture records all104 actual message-box texts, forty damage
observations, forty received records, actor frames, weather/terrain, PP, move
histories, RNG and the complete phase log. The modeled TurnStart-through-Berry
segment has447 phases; the preceding command/target phases are caller setup.
The eight English locale resources are pinned inputs to the actual C formatter,
not computed expected messages. An independent fresh capture and generated C
fixture reproduce byte for byte. Preliminary incomplete locale/environment
captures are not validation evidence.

## Streaming contract

The optional synchronous `pr_battle.event_sink` receives a full buffer's pending
suffix before storage is reused. The caller copies or consumes those events
before returning and must not reenter or mutate the battle engine. After each
engine call, it processes the remaining unacknowledged suffix and calls
`pr_event_acknowledge`. Acknowledged diagnostic entries remain readable until
reuse; they are not sent twice. With a sink, `e->events` is the current chunk,
not a complete turn archive. A caller needing the whole sequence collects the
chunks and final suffix.

The default caller without a sink/drain keeps the original explicit overflow
behavior. Errors remain sticky for that call. Existing count resets are
compatible, and engine continuation/switch/reset owners reset acknowledgments.
Neither the192-event core buffer nor the384-event game-core handoff limit is
increased. There is no allocation. The downstream limit still reports failure;
this slice is not a claim that all presentation/capacity bounds are closed.

The later [controller storage follow-up](../game-core/CONTROLLER-EVENT-STORAGE-VALIDATION.md)
adds optional caller-owned growth and binds it in the app. Default clients keep
the explicit 384-event limit; the core chunk capacity remains 192.

`pr_event_add` returns the appended entry so callers can annotate it even when a
flush changed its index. The actual Tera animation owner and charge-cleanup
snapshot use that entry. Game-core installs the sink for each battle, including
restored battles, handles ability-reveal knowledge in each drained chunk, and
emits ordered `GC_EV_BATTLE` ranges. The app already consumes those explicit
ranges. The final handoff acknowledges delivery, preventing a later cleanup
snapshot or repeated drain from replaying old events. Save schema and enum IDs
are unchanged; callbacks and acknowledgments are transient runtime state.

## Checks

- **1140 native comparison checks**: all104 rendered texts in order, every one of
  forty damage/HP snapshots, critical flags, all447 phase names within the owning
  boundary, actual mover identities, actor HP/stages/PP, retained histories and
  received-record fields, weather and both RNG streams. The complete361-event
  sequence arrives in two chunks; no overflow/unsupported flags. C completes at
  the next TurnInit; its449-entry full trace includes later end-turn phases.
- **2743 raw transport guards**: default overflow, more than five FIFO buffers,
  post-append annotations, acknowledgment/reuse without duplicate delivery,
  legacy direct reset, sticky error state and actual Tera metadata across flush.
- **1441 controller checks**: ordinary player Population Bomb/ally Instruct with
  NPC Splash produces a twenty-hit turn exceeding192 events; two contiguous
  notifications deliver it before and after legal initial save/load. HP/PP/RNG,
  snapshots, callback context rebinding, repeated-drain behavior and an
  early-chunk-only ability reveal are checked. The384-event downstream limit
  remains an explicit tested error. This is controller consistency evidence,
  not a paired upstream full-run/AI-choice proof.
- **11 compiled production faults**, **13 malformed captures** and **three
  forged observations** (last message, damage snapshot and battle RNG) are
  rejected. Compilation failure does not count as a caught production fault.
- Scoped ASan/UBSan/LeakSanitizer runs instrument events, engine, phases and
  game-core's battle/run owners. Supporting archives use ordinary optimized
  builds; full-program instrumentation is not claimed.

```sh
# Node22, clean pinned source/locales and installed dependencies:
bash battle-core/tools/reproduce-event-delivery.sh /path/to/upstream /path/to/output
python3 battle-core/tools/check-event-delivery-proof.py
```

The retained history/Tera/cleanup, normal app/custom/save/football and actual
ARM/stack checks remain required. Only the dedicated Task63 check is skipped.
Broader source inventory, remaining identity/fusion interactions, wide-level
acceptance and other capacity paths stay in the
[completion checklist](COMPLETION-CHECKLIST.md). No builder/release is added.
