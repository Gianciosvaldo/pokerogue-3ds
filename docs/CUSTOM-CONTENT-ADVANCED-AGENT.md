# Custom content handoff for an advanced outside agent

Prepared 2026-10-11 from port `main`
`290243097c6324e7950b03c67be694c56f7d6034` (through PR #206).
This is an implementation plan, not a record of completed custom features.
Refresh main and check existing work before each task.

The user's `ideas for pokerogue.txt` supplies the concepts: a local Nemesis/
Graveyard, a post-mortem and scouting hub, and a tactile capsule machine.
Its execution steps are brainstorming proposals. This handoff turns them into
bounded engineering tasks and corrects several unsafe assumptions. Creating
this document does not execute its tasks; pasting a task prompt assigns that
task to the outside agent.

## Intended first release

Provide an optional custom experience (working name **Legacy**), separate from ordinary
upstream Classic behavior:

1. A readable post-mortem of the last five turns, with the actual finishing
   source shown only when reliably known.
2. A small local roster of remembered enemies, a biome scouting screen, and
   optional rematches whose strength is appropriate for the new run.
3. A capsule dispenser with touch/button controls and a skippable animation.
   Its first real reward integration uses the existing voucher-to-egg API.

These are product defaults for a first implementation, not rules extracted
from upstream. Upstream remains the reference for the existing battle/item/
gacha behavior and APIs which this content calls. New custom policies are
tested against this specification, not falsely called upstream parity.

**Explicit first-release choices:**

- Retain at most **16 Nemeses**, overwriting the oldest eligible record. One
  remembered enemy per naturally completed defeat, only with reliable identity.
  Abandon, reset, clear and an unattributed defeat do not create one.
- History retains **five turns**, at most **64 observed events per turn**.
  Preserve original event order. On overflow drop oldest entries and mark the
  turn incomplete; never present a partial timeline as a complete damage audit.
- Record identity, original battle traits and loadout for history. Do not use
  the old raw stats/level/items as an unrestricted combat template.
- Scouting is **free in version 1**. Do not invent a new meta-currency earning
  economy or charge existing vouchers for it.
- A Nemesis rematch can award **one regular egg voucher**, at most once per
  record. It does not recover dropped player items or grant the old loadout.
  This reward is unavailable until durable profile/reward transactions exist.
- Offer a rematch only in a **nonfinal ordinary wild encounter window**,
  initially waves **27, 77, 127, 177**, at most once per run. If a window is a
  trainer, fixed/scripted, mystery, boss or unsupported encounter, skip it.
  Declining continues the ordinary encounter. Never replace the final boss,
  rivals, Gym/E4 fixed teams or required progression. No hidden invasion in v1.
- The first safe combat slice is a recognized, non-fused, non-Tera, non-boss
  single enemy using already proved ordinary mechanics. Refuse a snapshot
  whose required species/form/ability/moves/items cannot be represented safely.
  Surface the limitation without silently replacing its moves or abilities.
- Capsule animation settles within **1.5 seconds**, can be skipped immediately
  after commitment, and has equivalent hardware-button controls. Physics never
  determines rewards. Start with simple sprite velocities/bounces, no Box2D.
- Time Capsules, player-item recovery, custom starter/form unlocks, paid
  scouting, new currencies, enhanced stats/boss shields, doubles, fusion and
  difficulty variants are deferred. They require separate balance/contracts.

## Repository facts and integration gaps

| Existing interface | Actual role | Do not assume |
|---|---|---|
| `battle-core/include/pr/events.h` | Presentation events and after-event field snapshots | Every damage event identifies its attacker, move, stable Pokemon ID or cause |
| `GC_EV_TURN` / `GC_EV_BATTLE` in `game-core/include/gc/run.h` | Turn boundaries and ranges of raw battle events | Replayed animations are new combat events |
| `game/source/run_screen.c` | Consumes raw game/battle events and drives screens | The final `gc_run` state is the state at every earlier event |
| `rs_run()` / `rs_config.on_end` in `game/include/run_screen.h` | Run remains readable during the end callback | Its pointers remain valid after a new run starts |
| `game/source/app_flow.c:on_run_end` | Summarizes the result and clears the normal session slot | It already commits custom history/Nemesis transactions |
| `game/source/result_screen.c` | Existing result UI | It already has a post-mortem or radar |
| `game/include/ui.h`, `game/menus/mui.h` | Screen stack, input, timing, drawing | Touch input is already an abstracted gesture API |
| `assets-pipeline/app-src/include/save.h` | CRC-protected blob writes with temporary file/backup handling | Multiple files form one atomic transaction |
| `gc_system_save/load` in `game-core/include/gc/system.h` | Profile encoding/decoding | The app already owns a durable shared profile across all screens/runs |
| `gc_system_gacha_pull` in `game-core/include/gc/gacha.h` | Bounded voucher debit and creation of eggs | It hatches eggs, instantly unlocks species, or writes the SD card |

At the audited head, app profile ownership/persistence and exact causal combat
telemetry need explicit contracts. Enemy substitution has no established
custom encounter API. These gaps gate live features; UI and isolated model
work can proceed independently. No public function named in a proposed task
below exists merely because this document names it.

For reference behavior use `pagefaultgames/pokerogue` at
**`8555c08c823b856cbec4eb99ca84ea52a955836d`**, **Node 22**, and the owning
validation records. `game-core/EGG-GACHA-VALIDATION.md` excludes rehydration/
hatching/UI/account persistence. The capsule machine must preserve that scope.

## Agent rules and scheduling

Read root README, `docs/HANDOFF.md`, `docs/PARITY-TASKS.md`,
`docs/PARITY-LEDGER.md`, applicable AGENTS files and each owning README.
Read current code and merged/open PRs. Identify already completed tasks before
coding. If a task already exists, cite the implementation/proof and stop.
Record main/upstream SHAs and dirty status. Preserve other people's changes.
Use one task/branch/PR; never push main. No self-merge unless this agent receives
its own explicit authorization. Codex's personal authorization does not transfer.

This pack is specifically for an **advanced agent assigned custom UI/content**;
the older HANDOFF statement that `game/` was not ready for outside agents is
historical. The scope of each pasted prompt controls which files may change.
It does not authorize unrelated frontend rewrites, other modes or core repairs.

There are three work areas: **X** for a proposed new `custom-core/` module;
**E** for existing `game/`; **C** for game-core adapters. X is a proposed folder,
not an existing API. Keep model/policy/codec logic there and import existing
tables/APIs read-only. Only one active task per area. An integration task which
names multiple areas occupies all of them. Reserve lane C with its current
owner before C10; do not race ongoing item/parity work. `run-core/` remains
reserved to its owner and is read-only throughout this pack. If its changes
are necessary, deliver a concrete interface proposal to that owner and stop
the dependent integration. Finish all independent work first.

| ID | Task | Areas written | Dependencies | Can ship independently? |
|---|---|---|---|---|
| C01 | Audited contracts and custom data model | X; its validation doc/workflow | None | Yes, headless module |
| C02 | Bounded telemetry reducer | X | C01 merged | Yes, tested reducer |
| C03 | Raw-event observation and post-mortem UI | E | C02 merged | Yes, honest observed-history UI |
| C04 | Nemesis store, codecs and local persistence | X + E | C01/C03 merged | Yes, history/roster without rematches |
| C05 | Deterministic rematch scheduling policy | X | C04 merged | Yes, headless policy |
| C06 | Nemesis Radar/scouting interface | E | C04/C05 merged | Yes, with live spawning clearly unavailable |
| C07 | Capsule presentation prototype | E | C01 merged | Yes, test/debug preview only |
| C08 | Durable profile and reward transaction owner | E | C04 merged | Yes, durable adapter |
| C09 | Actual voucher/egg vending UI | E | C07/C08 merged | Yes, bounded egg purchase/reveal |
| C10 | Live optional rematch adapter | C + E | C03–C06/C08 merged; lane C available | Gated by safe encounter/causal APIs |

Recommended start: **C01**, then C02. After C02, an X worker can develop policy
as prerequisites land while an E worker develops UI; never run two E tasks at
once. A single agent follows dependencies serially. Scope each new assignment
with one prompt below. C10 can remain blocked while all UI/model tasks complete.
Do not claim a mock, roster screen or policy test is a live Nemesis system.

### Storage, randomness and transaction requirements

- Serialize named/versioned fields with explicit widths/encoding or a strictly
  validated textual schema. Never serialize raw C memory, padding, pointers,
  callbacks, opaque engine structures or native enums without range checks.
  Use stable IDs; never persist a party-array address or field-position pointer.
- Keep custom metadata in separate files/namespaces. Define a magic/schema
  version, size/count limits, missing-file defaults and rejection of corrupt,
  truncated, duplicate-key and future-version data. A valid future-version or
  corrupt file must not be silently replaced with defaults. Use existing save
  transport read-only; test backup and write-failure semantics.
- Fixed-capacity history/roster, bounded parsing, checked arithmetic, no per-frame
  allocation. Target custom metadata at most **64 KiB encoded** and incremental
  history/roster runtime storage at most **128 KiB**, excluding the existing
  profile object. Measure actual sizes; do not guess the large profile's size.
  Never place a profile, save buffer or snapshot history on the 3DS's small stack.
- Custom policy RNG is separate from gameplay and gacha RNGs. Record its seed/
  algorithm/state and frozen roster/schedule per run. Drawing, scouting, touch,
  animation, skipping and retrying an I/O error must not consume gameplay RNG.
- Use a durable run identifier and monotonic transaction identifiers. Store a
  run's terminal history and Nemesis insertion as one custom-state transaction.
  Reopening results, retrying writes or restarting must not insert twice.
- Do not rely on two independent file renames for atomicity. Where one action
  changes custom state plus a profile, use one authoritative envelope or a
  write-ahead journal with explicit recovery. C10 needs the equivalent contract
  for custom run bytes plus its overlay. Preserve normal save formats/slots.
- Failed writes keep the old durable state and prevent a new success/reveal.
  Recovery must not grant twice, debit twice, reroll or lose a committed reward.
  Model crash points explicitly; a successful serialization is not a disk commit.

### Validation and completion requirements

Preserve existing producers/comparators, counts, seeds, tolerances, exclusions,
chooser/digest logic and failed checks. No broad unsupported waiver. Do not
hand-edit generated outputs or copy upstream source/assets/dumps/binaries into
the repo. Keep upstream outside the checkout. Use the original C renderer,
packs and localization primitives; no web UI or general new graphics engine.

New custom policy/reducers/codecs need meaningful deterministic boundary tests,
literal independent expected cases, overflow/invalid inputs and failure/recovery
tests. A test must detect a realistic runtime mutation. Run new owned tests on
every changed head; their command names are to be created and documented, not
pre-existing successes. For C01 define a portable `make -C custom-core test`
target and an additive owned CI workflow; do not rewrite existing CI jobs.

For E tasks affecting the app's run/save/flow, capture baseline and final:

```sh
make -j2 -C game/tests runtest saves_test flowtest
game/tests/build/saves_test
game/tests/build/flowtest
make -C game/tests runtest-save
```

Also run relevant menu tests from `game/menus/README.md` and new UI host tests.
The established app rare-evolution coverage failure remains a failure; don't
replace its counts/chooser to make a task pass. C08–C10 retain game-core H:

```sh
make -j2 -C game-core test
game-core/build/gcplay --auto 1000 --jobs 8
game-core/build/gcplay --auto 1000 --jobs 8 --save-test
```

Normal/save consistency is compared on the same build; don't hard-code an old
digest. Ordinary mode with custom options disabled must retain baseline behavior,
RNG calls and unsupported accounting. C10 adds separate custom-mode cases;
its outcomes are not upstream full-run parity. Existing parity source suites
relevant to changed caller paths must retain their owning counts and coverage.

Require exact-head host/custom/3DS CI. Screens need emulator input/render checks
with existing locally built packs; host tests don't prove visual quality. Record
Old 3DS memory/frame measurements when hardware is available. Unrun emulator or
hardware checks remain explicit gaps; never fabricate screenshots or timings.
Read the cloud Docker/runtime instructions before local container builds.

Each PR needs source/API citations, file scope, proposed-versus-existing APIs,
schema/policy version, commands and actual counts/exit statuses, before/after
results, malformed-input and runtime-mutation evidence, save crash-point results,
RNG/memory bounds, screenshots when actually captured, and exclusions.
Finish with COMPLETE for the bounded task, PARTIAL or BLOCKED with an exact
missing contract/owner. No overall live-feature claim from a prototype.

## C01 — Audit interfaces and establish the new custom model

```text
Do only C01 from docs/CUSTOM-CONTENT-ADVANCED-AGENT.md. Read the entire shared
specification/rules first. Refresh main and deduplicate. One PR; create only
custom-core/, docs/CUSTOM-CONTENT-CONTRACTS.md and an additive owned
.github/workflows/custom-content.yml. Existing logic/app folders are read-only.

Audit the actual headers/callers listed in the handoff's repository-facts table.
Use pinned upstream 8555c08c823b856cbec4eb99ca84ea52a955836d/Node22 for existing
behavior only. Cite what raw events contain, when buffers reset, run end/slot
clear order, profile ownership, gacha mutation and save transport semantics.
List missing stable actor/cause/encounter/profile APIs explicitly. Do not assert
that a suggested interface already exists.

Create a small portable C module and public primitive-data contracts for:
turn/event history with explicit known/unknown attribution; terminal summary;
Nemesis snapshot/roster; run-bound frozen schedule; transaction IDs. Follow the
16-record,5-turn,64-event and versioned-storage defaults. Keep stable species/
form/move/ability IDs, source run UID/wave/biome and historical traits; record
which optional values are known. Distinguish original identity from a future
scaled combat template. No raw gc_run/pr_pokemon copy as the file schema.

Specify strict limits and validation, record lifecycle (stored,scheduled,
challenged,defeated,reward-claimed as applicable), exact ownership/lifetimes and
rejected unsupported snapshots. Define wire/schema version1 and which fields
future adapters need. Keep runtime events and persistent fields distinct.
Design the API in the validation doc before implementing it. The new custom
module has no live hooks, wallet mutation or spawn code in this PR.

Add a real make -C custom-core test target and workflow invoking it. Boundary
tests cover zero/max/over-limit counts, invalid IDs/enums/indices, unknown
metadata and overflow-safe size calculations. Report sizeof/runtime/wire-size
bounds; don't claim Old3DS performance without measurement. Later tasks import
this merged contract rather than independently inventing it. Stop after PR.
```

## C02 — Reduce combat observations into a bounded history

```text
Implement only C02 from docs/CUSTOM-CONTENT-ADVANCED-AGENT.md. Shared rules
mandatory; scope custom-core/ only, one PR; require C01 merged. Deduplicate.

Implement the pure history reducer on the merged primitive contracts. It takes
explicit run/battle/turn IDs plus observed event sequence, does not read globals,
and makes no gameplay or rendering calls. Retain last5 turns,64 events each;
handle overflow with a visible incomplete flag and stable oldest-first eviction.
Count an input batch once; retries, animation playback and screen redraw must
not duplicate it. Reset on new run; don't merge identically numbered turns from
different waves/battles. Define duplicate/conflicting sequence rejection.

Read PR_EV_DAMAGE/HP/FAINT/MOVE_ANIM/SWITCH field semantics in the real header.
Damage and healing are distinct. Treat displayed values and measured HP deltas
as separately named observations if both are available; don't double-count
damage plus its HP presentation. No damage amount -> attacker/type/kill-cause
guess. Move animation is not proof that its move caused a later indirect faint.
Represent missing actor/move/cause with explicit unknown bits. Full turn damage
totals need known completeness, not a graph silently summing a truncated list.

Use literal fixtures for empty history, five/six turns,64/65 events, multi-hit,
healing, poison/unattributed HP loss, switch/reused field position, repeated
batch, wave boundary, malformed order and save-replay input. Test repeated
reduction/idempotency and no mutation on rejected inputs. A realistic mutation
(wrong eviction or duplicate batch acceptance) must fail. No rendering, SD
calls, raw event observer or invented battle callback in this PR. Commit the
actual commands/counts in the owning validation record. Stop after PR.
```

## C03 — Observe actual app events and show the post-mortem

```text
Implement only C03 from docs/CUSTOM-CONTENT-ADVANCED-AGENT.md. Scope game/ only,
lane E, one PR; C02 merged, cores/custom-core read-only. Verify no other E task
owns the app. Read game and menus READMEs, event contracts and C01 audit first.

Use raw GC_EV_TURN/GC_EV_BATTLE data in run_screen.c, before be_convert_events
or playback; validate batch range and observe each batch exactly once. Feed
the merged reducer through a narrow adapter. Don't reconstruct past state from
the final rs_run() or use the screen animation's timing as combat telemetry.
If stable identities are not derivable independently from the actual snapshot/
party mapping, preserve unknown. Never translate a field index directly into a
persistent killer ID. Do not add battle-core events or alter combat decisions.

Capture a value-owned terminal report while rs_run() is valid and BEFORE normal
slot clear/new run/reset. Inspect on_run_end and result_screen_open. Extend the
result flow with top-screen summary/finishing source when known and bottom-screen
five-turn timeline, damage/healing/status entries and incomplete-history notice.
Provide A/back navigation and readable spacing at native resolutions. Empty/
unknown causes must still be useful; say unavailable, not a fabricated autopsy.
Do not label a final animation as the killer or claim a causal diagnosis from
damage totals. No Nemesis insertion or gameplay mutation yet.

Host fixtures must drive real event observation and result callbacks: multiple
batches, switch, indirect faint, unknown source, history overflow, replay/skip,
run end twice and starting a new run with no stale data. An observation-removal
mutation must fail. Retain app/save/flow/menu checks and exact-head CI. Emulator
check top/bottom layout and navigation with actual packs; report unrun hardware
checks. New screen/test filenames are proposals, not pre-existing APIs. Stop.
```

## C04 — Persist bounded history and the Nemesis roster

```text
Implement only C04 from docs/CUSTOM-CONTENT-ADVANCED-AGENT.md. One PR occupying
areas X and E; write custom-core/ and narrow game/ save/end adapters only. C01
and C03 merged; no simultaneous X/E task. Existing core/save library read-only.

Implement strict version1 codecs and bounded roster insertion on the merged
model. Use stable IDs/explicit fields, not struct fwrite/memcpy or pointers.
Own the most recent post-mortem plus16 Nemeses in a separate custom metadata
blob/name. Missing file starts empty; corrupted/future-version data must be
reported and preserved. Validate magic/version/length/counts/required fields,
enum/ID ranges, duplicate identities and arithmetic. Use save_read/write and
their verified backup behavior; no existing normal slot/profile schema edits.

On a naturally completed defeat, insert at most one independently identified
ordinary enemy and its observed snapshot. If the presentation data cannot
identify the actual source reliably, persist the report with unknown cause and
do NOT invent a Nemesis. Reset/abandon/clear are not defeats. Freeze the snapshot
before slot clear or run buffer reset. Use durable run/terminal transaction IDs
so result reopening, callback retries and restart cannot append twice. Persist
history and insertion together; commit success only after disk success. Full
roster overwrites its oldest eligible entry without growing storage.

Fault tests:0/16/17 insertions; ordering; repeated UID; run end twice; missing,
truncated, corrupt and future files; backup read; failure at each write/rename
boundary; reopen after commit. Preserve old durable state on failure. Measure
metadata <=64KiB and runtime <=128KiB excluding existing profile. No new spawn,
stats boost, wallet, Time Capsule or player-loadout banking. Keep existing saves
and event observation tests unchanged. Record real failure/mutation counts and
scope. An unavailable causal snapshot is an explicit live-insertion boundary;
the tested store may complete independently. Stop after this PR.
```

## C05 — Deterministic, bounded rematch policy

```text
Implement only C05 from docs/CUSTOM-CONTENT-ADVANCED-AGENT.md. Scope custom-core/
only, one PR; C04 merged; all gameplay cores read-only. No live encounter hook.

Implement a pure version1 scheduler using frozen eligible roster + recorded
custom seed/state. At run start choose at most one record using one documented
custom-only draw; freeze its ordered opportunities27,77,127,177. Offer it at the
first window the adapter reports as an ordinary nonfinal nonboss supported wild
encounter. If ineligible, try the next window; after all four, none this run.
Decline marks the schedule consumed; scouting/redraw/load never rerolls it.
Use integer selection with a documented reproducible algorithm, not ambient
Math.random/rand/global gameplay RNG or wall-clock changes after run start.

Return a data proposal: record ID, opportunity, eligibility/refusal reason and
fixed reward allowance. Scaling supplies no stat percentage buff: use the
ordinary enemy level already generated for this wave and existing standard
species-stat calculation in the future adapter. Keep original identity/traits
for history; reject unrepresentable form/ability/move/item data. Do not duplicate
run-core's level/stat formulas in custom-core. Exclude boss/fusion/Tera/doubles,
scripted/fixed/trainer/mystery/final battles and unsupported mechanics.

Define record transitions and once-per-record reward entitlement (one regular
voucher), independent of write success. Store IDs/state needed to resume the
same decision. Do not mutate a real wallet or claim an enemy was spawned.
Golden fixtures cover no/one/16 records, different seeds, each skipped window,
decline, all windows unavailable, duplicates, reload, completed/rewarded record,
unknown metadata, incompatible snapshots and invalid inputs. Repeated scouting
must leave policy and gameplay RNG unchanged. Mutate the selection or record
count rule and prove a failure. Record actual commands/outputs. Stop after PR.
```

## C06 — Nemesis Radar and scouting hub

```text
Implement only C06 from docs/CUSTOM-CONTENT-ADVANCED-AGENT.md. game/ only, lane E,
one PR; C04/C05 merged. Core/model code is read-only; deduplicate menu work.

Build a custom-history/Radar screen using ui.h/mui.h and merged value contracts.
Top: selected remembered enemy's identity and historical defeat summary. Bottom:
bounded roster list, biome/opportunity details when actually assigned, and
free Scout/back buttons. Reuse existing biome/name/sprite/localization helpers
after checking their contracts; missing assets get an honest fallback. No
generated-file hand edits or upstream artwork/source in the PR.

No fictitious live location: an unassigned roster record says no rematch is
scheduled; a frozen schedule shows the policy's actual opportunities. Until C10
is live, don't advertise active invasion or a working fight button. Scouting
doesn't consume vouchers/currency, modify gameplay RNG or create a new schedule.
Use modest dominant-type tint only when type is known; keep text contrast and
default background for empty/unknown roster. Tint must not affect combat rules.

Handle empty/full roster, long/localized names, unknown causes/traits, missing
sprite/map data, scrolling, button/touch parity where available, new run/load,
write error/future-version state and return to title/results. Model pointers
must not outlive their owner; callbacks cannot reopen a freed screen. Host
tests exercise navigation and zero mutation on scouting; use runtime-mutation
proof. Retain app/save/flow/menu checks, CI, and emulator visual/input checks.
No paid scouting/new currency/slot overwrite/starter system work. Stop after PR.
```

## C07 — Capsule-machine presentation prototype

```text
Implement only C07 from docs/CUSTOM-CONTENT-ADVANCED-AGENT.md. game/ only, lane E,
one PR; C01 merged. No real wallet, RNG, profile or unlock mutations.

Create the dual-screen capsule presentation with an explicit immutable fixture
result contract. Bottom: machine/pull controls and button equivalents. Top:
bounded capsule sprites dropping/bouncing/revealing fixture results. Use the
existing C renderer/timers and owned assets from locally built packs. No Box2D,
new physics dependency, browser UI, copyrighted assets or unlock database logic.
This is a debug/test preview; don't expose a pretend purchase in normal play.

Maximum25 capsules (existing largest gacha pull); fixed allocation. Simple
velocity/gravity/bounce with dt units matching ui.h (milliseconds), bounded
integration steps and elapsed-time clamps. Target <=1.5s to settled results.
Touch gesture is optional enhancement: detect/reject stale/out-of-bounds input,
reset gesture on release/screen leave, and provide the same pull with a button.
Inspect the actual hidTouchRead/input routing; no invented existing gesture API.
No pressure-sensitive-only control or required crank precision.

Skip immediately fast-forwards the already supplied reveal; completion callback
fires once regardless of skip, collision, repeated input, leaving/reentering or
timer cancellation. Animation, collision and random-looking motion never choose
or grant rewards. Use deterministic visual motion with no gameplay/gacha draws.
No pretending an egg is an instantly unlocked Pokemon.

Host tests:1/10/25 result items, empty/refused preview, dt=0/normal/large pause,
skip at each stage, held buttons, repeated submit, callback once and screen
destruction. A duplicate-callback or dt-unit mutation must fail. Emulator check
both screens and input; measure allocations/frame cost when possible. Preserve
app/menu checks and CI. Record prototype scope and unrun checks. Stop after PR.
```

## C08 — Durable app profile and idempotent reward transactions

```text
Implement only C08 from docs/CUSTOM-CONTENT-ADVANCED-AGENT.md. game/ only, lane E,
one PR; C04 merged. Existing game-core gacha/profile APIs and save library are
read-only. This prerequisite may be substantial: do not claim it exists.

Audit whether current main already owns/persists one shared gc_system across
app title/run/gacha/end screens. If absent, introduce a narrow app owner/adapter
which passes the correct profile to existing core calls without reimplementing
them. Preserve ordinary session-slot semantics and older valid profile data;
check ownership, initialization and loads through actual API contracts.
Do not use a fresh default profile or debug vouchers as a real purchase wallet.

Design ONE authoritative transaction envelope or explicit write-ahead journal
for profile bytes, gacha-context RNG, custom reward ledger and transaction result.
Use gc_system_save/load for profile contents and C04 codec for custom contents;
do not memcpy the large gc_system. Measure bounds and allocate buffers safely.
Failures/future versions are reported and preserved. Normal session save format
and existing CRC/backup implementation are unchanged.

For a purchase, stage a candidate profile/context, call actual
gc_system_gacha_pull exactly once, persist debit + appended eggs + resulting RNG
+ immutable result/transaction ID atomically, then publish memory/result. On
retry/restart the same transaction returns the committed result without a new
pull. I/O failure cannot reveal/grant success or consume the live wallet/RNG.
Clock/entropy inputs are supplied once and saved, not re-sampled on retry.
Keep gacha scene RNG separate from a live run's battle/generation RNG.

Also support a staged, once-per-record one-regular-voucher Nemesis reward for
future C10, atomically with its claimed flag. No invented custom currency or
new starter/form unlocks. Fault-injection tests cover each prepare/write/commit/
publish/restart boundary, duplicate requests, corrupt/backup/future data and
failed profile serialization. Every recovery ends with zero grants/debits or
exactly one coherent commit. Detect a grant-before-commit mutation.
Retain raw egggachadiff.sh at its owning counts/seeds unchanged, app/profile/save
tests, H and exact-head CI. Record transaction state diagram and real results.
No network/account persistence or hatching. Stop after PR or precise blocker.
```

## C09 — Connect the capsule UI to actual voucher-to-egg pulls

```text
Implement only C09 from docs/CUSTOM-CONTENT-ADVANCED-AGENT.md. game/ only, lane E,
one PR; C07/C08 merged, cores/custom models read-only. Deduplicate gacha UI.

Read game-core/include/gc/gacha.h, gc_gacha.c and EGG-GACHA-VALIDATION.md; pinned
upstream8555c08c823b856cbec4eb99ca84ea52a955836d/Node22 governs the existing
machine/cursor/voucher/capacity behavior. Don't infer costs from cursor labels.
Expose the three machines/five options supported by the actual API, real
voucher balances, egg capacity and refusal messages. Use C08's shared profile
and transaction owner; forbid a second wallet or independent reward generator.

One accepted pull -> one committed transaction -> immutable egg result/order
-> C07 presentation. Buttons/touch cannot enqueue duplicate purchase IDs.
Disable further purchase until commitment/result handling finishes. Insufficient
vouchers/capacity or write refusal changes no live profile/RNG and shows no
reward. Closing/skipping/restarting reveals the already committed eggs once,
without pulling again. Results are eggs: don't mark species caught/unlocked,
call hatching, grant custom forms/items, or rehydrate via an unproved path.

Call source-owned API for all RNG, pity, guarantee, featured species, debit and
egg fields. Render the saved immutable result in the API's actual order, not
physics landing order. Keep animation timing out of random/date decisions.
Persistence stores named transaction/profile/egg fields and required RNG state.

Test every supported machine/cursor, affordable/refused, capacity edges, one/
multi pull, repeated/held input, skip/reentry, exact saved result and restart
after each C08 crash point. Real callback-removal/double-pull mutations must
fail. Retain the owning gacha source suite, app/save/menu tests, H and CI;
emulator-check input/layout and bounded animation. Record what is compared as
upstream core behavior versus original custom presentation. Stop after PR.
```

## C10 — Integrate optional Nemesis rematches without invading owner lanes

```text
Implement only C10 from docs/CUSTOM-CONTENT-ADVANCED-AGENT.md. One PR occupies
lanes C and E: game-core/ plus narrow game/ adapters, custom-core/ read-only.
Require merged C03-C06/C08 and explicit lane reservation with the current core
owner. run-core/battle-core/reward-core/generated tables remain read-only.
If C is busy, finish independent work and wait; don't race parity changes.

Audit current encounter initialization, gc_new_battle/battle start/events,
terminal-cause state and legal save boundaries. Propose the smallest typed
optional custom hook/observer contract, disabled by default, before editing.
No existing custom spawn/causal API is assumed. Add only the needed caller hook
in game-core; if battle/run-core ownership needs changes, stop with the exact
interface proposal and blocker. Do not rewrite enemy generation in game/ or
hand-assign a partially initialized pr_pokemon into a live field.

Use C05's frozen schedule and canonical encounter classification. Offer the
rematch at an eligible window with accept/decline UI before combat starts.
Retain ordinary encounter on decline. For accepted supported snapshots, use
existing enemy construction/stat/item paths with the ordinary generated level;
no hard-coded scaling formula, extra boosts, boss segments, dropped player
inventory or silent ability/move substitution. Validate full initialization,
ownership, callbacks and RNG boundaries. Unsupported snapshots are explicit
refusals outside successful custom-combat counts, not hidden failures.

Reliable direct defeat attribution may use a narrowly observed actual causal
state; never guess from last animation or last standing enemy. Unrepresented
indirect/compound sources remain unknown and cannot create a Nemesis. No
battle-core event-schema repair within this task. Exclude fixed/scripted/final/
boss/trainer/mystery/doubles/fusion/Tera encounters and unproved mechanics.

Custom sessions carry a sticky flag, frozen roster/schedule, observer history,
run UID and reward IDs. Persist core run bytes plus overlay using an atomic
envelope/journal; keep separate custom slot namespace and explicit resume route.
Never load custom bytes as an ordinary upstream session or silently resume an
overlay mismatch. Verify Save/Load before/after offer, decline, accept, victory,
defeat and reward commit. One victory gives at most one regular voucher through
C08; repeats/reloads cannot regrant. Clear original roster entries only per the
documented policy/transaction, not merely when a screen closes.

Controls: custom off retains identical seeded ordinary results/RNG/unsupported
counts; custom on demonstrates actual scheduler -> offer -> constructed enemy
-> combat -> durable result/reward, both uninterrupted and resumed. Include
all four windows/skips, refusal, no roster and exact initialization fixtures.
Mutation removing the real encounter hook or duplicate reward grant must fail.
Run relevant unchanged encounter/battle source suites, H, app/save/flow/custom
tests and exact-head CI. Live custom combat, core parity and hardware acceptance
are different claims. No final-boss/Classic-clear completion claim. Stop after
this PR or the precise missing prerequisite; don't replace it with guessed code.
```

## Paste-first coordinator prompt

Use this if the outside agent should manage the sequence rather than receive
individual cards. It still starts with one task, not ten simultaneous branches.

```text
You are assigned the custom content plan in
docs/CUSTOM-CONTENT-ADVANCED-AGENT.md for Gianciosvaldo/pokerogue-3ds. Read the
whole file, repo instructions, current code and PR history. The attached ideas
are brainstorming; the handoff's explicit version1 defaults/contracts govern
this assignment. Keep upstream behavior source-backed at the pinned SHA/Node22;
custom policies use explicit specification tests and are never called upstream
parity. No invented APIs, causes, spawn behavior, unlocks or test results.

Start with C01 only. One task per branch/PR and one active task per named area;
reserve every area a multi-area integration touches. Preserve existing tests,
normal saves/RNG behavior and owner boundaries. run-core is read-only. UI/model
prototypes may complete before live gameplay prerequisites, with honest labels.
Proceed through this batch after each card's actual prerequisites merge; never
build on an unmerged side branch or self-merge without my separate explicit
authorization. Other independent assigned cards may proceed while a dependency
awaits review. Prepare concrete reviewable PRs. Report
actual evidence, COMPLETE/PARTIAL/BLOCKED per bounded task and exact remaining
dependencies. Do not claim the full custom mode is shipped until C10's live
integration and required checks are complete. Do not implement deferred Time
Capsules, currencies, extra modes or new battle rules as scope expansion.
```
