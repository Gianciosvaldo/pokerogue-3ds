# Custom content contracts, version 1 (tasks C01, C02, C03, C04, C05, C06, C07, C08, C09)

Written for task C01 of [CUSTOM-CONTENT-ADVANCED-AGENT.md](CUSTOM-CONTENT-ADVANCED-AGENT.md). It does three things:
audits the interfaces the custom content will touch, fixes the version-1 data contracts, and records what the new
`custom-core/` module proves. It is not a record of a working custom mode: the module has no live hooks, no file
access, no wallet access and no spawn code. Since C03 the app feeds the reducer and shows a read-only post-mortem screen
(sections 2.9 and 5.2). Since C04 it keeps the last run's report and the id counters in one file on the SD card (sections 2.10 and
5.3); no Nemesis is ever inserted by the app yet, and no custom mode exists. Since C05 `custom-core` also holds the rematch
policy as pure data (section 2.11, 5.4): nothing in the app calls it yet. Since C06 the title screen has a read-only
Nemesis Radar over the stored roster (section 2.12, 5.5); it starts nothing. Since C07 the app has a capsule-machine presentation
prototype that only a debug file opens (section 2.13, 5.6); it shows fixed results and is connected to nothing. Since C08 the app
owns the profile transactions (voucher-to-egg pull, the once-per-record Nemesis reward) behind one write-ahead envelope (section 2.14, 5.7).
Since C09 the title has a **Capsule Machine** entry: the three voucher-to-egg machines, one committed transaction per accepted pull,
shown through the C07 capsule screen (section 2.15, 5.8). That is the first purchase a player can make; it hatches nothing.

| Item | Value |
|---|---|
| Port `main` audited | `ff728e20e7443b4c41116c0c5e02dd10f4bcc5aa` (through PR #208; the handoff was written at `2902430`, through #206) |
| Branch base | the same commit; working tree clean at the start of the task |
| Upstream pin | `8555c08c823b856cbec4eb99ca84ea52a955836d`, Node 22 is the reference runtime |
| Upstream checkout used | **None.** Every statement in the audit is about this repository's code. C01 makes no claim about upstream behavior and adds no upstream comparison. |
| Open PRs at the start | none |
| Merged/open work on this subject | none: `custom-core/` did not exist, and no PR mentions Nemesis, post-mortem or capsule work |

Labels used below: **[read]** read in the code at the cited place, **[ran]** observed by running something,
**[inferred]** follows from code read but was not executed, **[choice]** a version-1 policy this PR picks that the
handoff did not fix (listed again in "Choices to confirm").

## 1. Audit

### 1.1 What the raw battle events contain

`pr_event` ([`battle-core/include/pr/events.h`](../battle-core/include/pr/events.h)) holds `type`, `battler`,
`target`, `id`, `value`, a message, weather/terrain, and `field[4]`, one `pr_event_field` per field position
(species, form, `party_slot`, `hp`, `max_hp`, `status`, `fainted`, `substitute`, `visible`).

- **[read]** No event carries a Pokemon id, an attacker, a cause or a hit index. `battler` is a *field position* (0 and
  1 player, 2 and 3 enemy). `field[].party_slot` is an index into `bt->party[side]`. `Pokemon.id` exists
  (`pr_pokemon.id`, `battle-core/include/pr/battle.h:206`) but is not in the event.
- **[read]** The field snapshot is taken when the event is *added* (`battle-core/src/events.c:84-115`), which for the damage
  event is when the damage-animation phase *runs*, not when the damage was dealt. `PR_EV_DAMAGE` is added at
  `phases.c:3662`; its `value` was fixed earlier, at `phases.c:1905` (`anim_amount = (int32_t)damage`, the damage after
  `pokemon_damage` clamped it to the remaining HP and applied Endure-style survival). So `value` is the amount applied
  and `field[].hp` may already include later changes. The two are different observations.
- **[read]** `PR_EV_HP` has two meanings (`events.h`): a heal, where `value` is the amount *shown*
  (`phases.c:3490-3503`: it is capped at max HP, not at the room left, so it can exceed the HP actually gained), and
  status damage, where `value = hp_after - hp_before` and is negative (`phases.c:3032`). Indirect damage through
  `damage_and_update` with a negative heal arrives as `PR_EV_DAMAGE` with result `HIT_RESULT_INDIRECT`.
- **[read]** `PR_EV_MOVE_ANIM` gives the user (`battler`) and one target per event. Nothing links it to a later damage
  event by id. A move animation is not evidence that the move caused a later faint.
- **[read]** `PR_EV_FAINT` carries only the fainting position (`phases.c:2745`). The faint path ends in
  `pr_leave_field(..., true)` (`phases.c:2779`), which calls `reset_turn_data` (`phases.c:2457-2472`) and so clears the
  fainted Pokemon's `attacks_received`.
- **[read]** There is one place that does record an attacker: `pr_pokemon.attacks_received[]`
  (`pr_attack_record`: move, result, critical, `source_battler_index`, `source_id`, damage), filled for direct damaging
  hits at `phases.c:1990`. It keeps the newest 8, is turn data (cleared by `reset_turn_data`, which also runs when a
  Pokemon leaves the field or switches, `switching.c:528,547,703`), and does not cover poison, weather or other
  indirect loss. **[inferred]** It is a candidate source for a *narrowly observed* attribution, but whether it can be
  read at the moment the app needs it is not established, and no code reads it for this purpose.
- **[read]** One battle-core call returns at most `PR_MAX_EVENTS` (192) events; the rest are dropped and
  `events_overflow` is set (`events.c:84`, `engine.h:164-166`).

### 1.2 Turn boundaries and buffer lifetimes in game-core

- **[read]** `GC_EV_TURN` (`a` = `bt->turn`) is queued only by `gc_battle_run` for a normal turn (`pick < -1`,
  `gc_battle.c:524-531`), immediately before `pr_run_turn`. It is **not** queued for `pr_resume_switch`,
  `pr_continue` or the battle start. So the battle events of a turn can arrive as several `GC_EV_BATTLE` ranges after
  one `GC_EV_TURN` (the turn, the rest of it after a switch prompt, the rest after experience), and battle-start events
  arrive with no turn marker at all.
- **[read]** `GC_EV_BATTLE` is `{a, b}` = a range of `run->battle_events[]` (`gc_run.c:113-127`). The array holds
  `GC_MAX_BATTLE_EVENTS` (384) entries and is **reset to empty at the start of every `gc_run_choose`**
  (`gc_run.c:301-303`), so a range is valid only until the next answer. Over 384 sets `battle_events_overflow`; an
  overflowing battle-core call (`events_overflow`) sets it too. The event queue itself holds `GC_MAX_EVENTS` (256) and
  sets `ev_overflow`.
- **[read]** In the app, `drain()` (`game/source/run_screen.c:236-239`) copies the queued events into `G.q[]` after each
  answer. `handle_event` sends `GC_EV_BATTLE` to `be_convert_events` (`run_screen.c:426-454`) and `GC_EV_TURN` to its
  `default:` branch, which shows nothing (`run_screen.c:567`). `G.run.battle_events[]` is the array the ranges index.
- **[read]** `settled_after()` (`run_screen.c:292-299`) already records that the battle state in `G.run.bt` matches a
  given event range only when the run stopped right after that range. In any other case `G.run.bt` is the state at
  the end of the answer, not at the event. So a stable id cannot be read from `bt` for an arbitrary event.

### 1.3 Run end, slot clear and `rs_run()`

- **[read]** `end_run` runs when the driver reaches `GC_SCREEN_GAME_OVER` or `GC_SCREEN_CLEAR`
  (`run_screen.c:1170-1172`), cancels its timer, counts the result and calls `cfg.on_end(kind, arg)`
  (`run_screen.c:1075-1087`). It returns without driving anything further.
- **[read]** The app's callback `on_run_end` (`game/source/app_flow.c:88-97`) does, in this order: build a `result_args`
  (`victory`, `rsv_summarize(rs_run(), ...)`), **`rsv_clear(A.slot)`**, then `result_screen_open`. The return value of
  `rsv_clear` is ignored. The result screen reads only that summary (`result_screen.c`); it has no history.
- **[read]** `rs_run()` returns `&G.run`, a static (`run_screen.c:1260`). The pointer never dangles, but its *content* is
  replaced by the next `rs_start` (`gc_run_new`, which memsets the run) or `rs_resume`. `rs_resume` calls `rsv_load(slot,
  &G.run, ...)` directly (`run_screen.c:1241-1257`), and `gc_run_load` documents that a refused load leaves the run
  empty. So any consumer must copy what it needs *by value* during `on_end`. The handoff table's "its pointers remain
  valid after a new run starts" is true only of the address.
- **[read]** The app has no abandon, retire, reset or save-and-quit flow (grep of `game/` finds none; `slot_screen.h`
  lists Rename Run as missing). The only run endings are `GAME_OVER` and `CLEAR`. `CC_OUT_ABANDON` and `CC_OUT_RESET`
  therefore exist in the contract but cannot occur in the current app.
- **[read]** `gc_run.terminal_result` (0, 1 defeat, 2 awarded clear) is game-core state, not passed to `on_end`.
- **[read]** Crash consequence of the order above **[inferred]**: the slot holds the save written at the *start of the
  encounter in which the player lost* (`write_save` at the first COMMAND screen). If the app stops after the defeat but
  before `rsv_clear`, Continue reloads that save and the player replays the wave. If a custom commit ran before the clear
  and the app then stops, a Nemesis exists for a defeat that was undone by the reload. If the clear ran first and the app
  stops before a custom commit, the defeat leaves no record. C04 must choose the order knowingly; neither is free.
- **[choice, C04]** The app commits the custom report **before** `rsv_clear` (section 2.10). A stale report (of a defeat the
  player undid by reloading) is the worse outcome of that order, a missing report of a defeat that stood is the worse outcome of the
  other; a report is only a post-mortem, and the app never inserts a Nemesis from it, so the stale one cannot invent an enemy.

### 1.4 Run identity

- **[read]** Nothing in game-core or the app gives a run a durable identifier. `gc_run.run.seed` is the upstream run seed;
  the first run of a session uses the configured seed (`run_screen.c:1212`, `G.st.runs == 1 ? G.cfg.seed : NULL`) and
  `rundriver.txt` can repeat it, so a seed is not unique. `rs_config.entropy` is `(uint32_t)osGetTime()` at boot
  (`game/source/main.c:80`) mixed with a run counter and `ui_frame()` (`app_flow.c`, `run_config`); 32 bits and
  not persisted.
- **[read]** `pr_pokemon.id` (uint32) is drawn once from the run RNG when a Pokemon is generated (`gc_mon.c:60,99`,
  `gc_mystery.c:334`, `gc_encounter.c:390`) and is saved with the run. It is stable inside one run and is what held
  items point at. It is not unique across runs.
- **[read]** Normal run saves (`gc_run_save`, `GC_SAVE_MAX` 128 KiB) have their own format and cannot carry a custom UID
  without changing it, which the handoff forbids. **[inferred]** So for an ordinary run the UID belongs to an
  *observation* (`rs_start` or `rs_resume`), not to the saved run: loading the same save twice and losing twice gives two
  UIDs and may create two records. Whether that is acceptable is a [choice] listed below.

### 1.5 Profile ownership and the gacha API

- **[read]** `gc_run.sys` is the pointer to the profile (`gc/run.h`, "NULL = no system updates"). `gc_run_new` zeroes the
  run, so it starts NULL. **No file under `game/` mentions `gc_system`, `gc_system_save`, `gc_system_load`,
  `gc_system_init_defaults` or `gc_system_gacha_pull`.** Only `game-core/tools/gcstress.c` and tests set `run->sys` or
  touch the profile. The app has no shared profile, no profile file and no profile load; vouchers and eggs live nowhere
  the app can reach. C08's premise ("introduce an app owner") is confirmed, not already satisfied.
- **[ran]** `sizeof(gc_system)` = 724,176 bytes and `sizeof(gc_run)` = 314,488 bytes on x86-64 Linux/gcc 13.3 (a probe
  compiled against the headers; ARM not measured). The profile alone is about 707 KiB, so it must live in static
  storage and cannot be copied into a 128 KiB custom budget or onto the 3DS stack.
- **[read]** `gc_system_gacha_pull(sys, ctx, out, why)` (`gc/gacha.h`): mutates the profile (debits `voucher_counts`,
  appends eggs, statistics, pity) and both RNG states in `ctx` only on success; capacity and cost refusals leave profile,
  RNG and result unchanged (`game-core/EGG-GACHA-VALIDATION.md`). It returns up to 25 eggs (`order[25]`), does not hatch,
  does not write the SD card, and has **no caller outside `game-core/tests` and `game-core/tools`** (grep). The validation
  record excludes `EggData.toEgg` rehydration, hatching, UI and account persistence.

### 1.6 Save transport (`save_write` / `save_read`)

[read] `assets-pipeline/app-src/source/save.c`, header `{magic "PRS1", len, crc32}`, 64 MiB read limit:

1. `save_write` writes `<name>.sav.tmp` completely (fflush, fclose; removes the tmp file on failure).
2. It reads the current `<name>.sav`. If it is CRC-valid: `remove(<name>.sav.bak)`, then `rename(main, bak)`; a failed
   rename returns `SAVE_IO_ERROR` and leaves the tmp file behind. If the current file fails its CRC it is deleted and the
   good `.bak` is kept.
3. `rename(tmp, main)`; a failure returns `SAVE_IO_ERROR`.

Crash or failure windows (what `save_read` then returns):

| Stopped after | main | bak | tmp | `save_read` |
|---|---|---|---|---|
| step 1 | old | old bak | new | old |
| `remove(bak)` | old | none | new | old |
| `rename(main, bak)` | **none** | old | new | **old, from the backup** (`used_backup`) |
| `rename(tmp, main)` | new | old | none | new |

Consequences for the custom work:

- The new bytes are never recoverable from a stranded `.tmp`; the old state wins. A failed write always leaves the old
  durable state readable, which is what the handoff requires, but a reader cannot tell "never written" from "write
  stranded".
- `save_read` falls back to `.bak` when the main file fails its CRC and still returns `SAVE_OK`. **A damaged main file
  therefore rolls the state back silently by one write.** Within one blob that is internally consistent (the ledger rolls
  back with the roster). Across two files it is not: a profile file and a custom file can roll back independently, which
  is why C08/C10 need one envelope (the handoff's rule) and why a ledger kept in a *different* file from the thing it
  protects would not make a retry idempotent.
- `save_write` rotates a CRC-valid main file into `.bak` and deletes the previous `.bak`. It knows nothing about schema
  versions. A file written by a *newer* version is CRC-valid, so the first write by this build would move it to `.bak`
  and the second would destroy it. **Any custom adapter must read, check the version, and refuse to write when the
  existing file is unsupported.** C04 owns that check; `CC_E_VERSION` is the status for it.
  (Done in C04: `custom_store.c` decodes before it writes, and a file it cannot use is never written, section 2.10.)
- There is no atomicity across names. `save_init` creates `sdmc:/3ds/pokerogue/save`; names are plain strings with no
  sanitising (`save.h`).

### 1.7 Input and the UI layer

- **[read]** `ui_tick` reads `hidKeysDown()` and `hidKeysHeld()` only (`game/source/ui.c:228-229`). There is no
  `hidTouchRead` anywhere under `game/` or `assets-pipeline/app-src/`. `shop_ui_touch(float, float)` exists
  (`game/menus/shop_ui.c:279`) and is called only by `game/tests/shop_ui_test.c`; nothing feeds it. **There is no touch or
  gesture API in the app.** `ui_inject(ui_button)` feeds buttons for tests.
- **[read]** Time is milliseconds of game time (`ui.h`: `float dt` in ms, `ui_now()`, `ui_after`). Screens are
  `ui_screen` structs on a stack (`ui_set`, `ui_push`, `ui_pop`).

### 1.8 Interfaces that do not exist (explicit list)

None of these is assumed by this PR. A later task that needs one must add it in its own owner lane or stop.

1. **Stable actor identity in a battle event.** Events carry positions and a `party_slot` snapshot, never `Pokemon.id`.
   Deriving an id needs `bt->party[side][party_slot]->id` *as of the event*, which the app cannot read except when
   `settled_after()` holds. Change would be in battle-core (forbidden here) or an adapter that proves its mapping.
2. **A causal source for HP loss or a faint** (attacker, move, indirect cause). Only `attacks_received` (direct hits,
   cleared at faint and switch) exists, unread.
3. **An encounter classification API** ("ordinary nonfinal nonboss supported wild encounter"). game-core has
   `battle_type`, `me_battle`, `trainer_type` and `pr_pokemon.is_boss`; there is no predicate and no list of fixed/
   scripted waves for this purpose.
4. **A hook to substitute or add an enemy at encounter generation.** `gc_new_battle` and `gc_mon.c` generate the enemy;
   there is no optional hook and the handoff forbids assigning a hand-built `pr_pokemon` into a live field.
5. **A durable run identifier** (section 1.4).
6. **An app-owned profile**, profile file, and any reward ledger (section 1.5).
7. **A touch/gesture layer** (section 1.7).
8. **A terminal-cause output of `gc_run`** (`on_end` receives only the screen kind).
9. **An entropy source stronger than 32 bits of `osGetTime()`** (section 1.4).

### 1.9 Where the handoff's table needs a correction

- "`rs_run()` ... Its pointers remain valid after a new run starts": the address is constant, the content is replaced
  (section 1.3).
- "`gc_system_save/load` ... The app already owns a durable shared profile": it does not (section 1.5).
- "Touch input is already an abstracted gesture API": it is not (section 1.7).
- "`app_flow.c:on_run_end` ... already commits custom history/Nemesis transactions": it does not, and it clears the slot
  *before* opening the result screen (section 1.3).
- "Multiple files form one atomic transaction": confirmed false; additionally a CRC-damaged main file silently falls back
  to `.bak` (section 1.6).

## 2. Version-1 contract

The source of truth for field names is [`custom-core/include/cc/types.h`](../custom-core/include/cc/types.h); limits are
in `limits.h`, encoded widths in `wire.h`, functions in `model.h`. Prefix `cc_` (custom content).

### 2.1 Three kinds of data

| Kind | Types | Rule |
|---|---|---|
| Runtime observation | `cc_obs` | What the screen layer saw. Never stored. Positions (`battler`) identify nobody; ids are 0 unless derived independently. |
| Persistent history | `cc_event`, `cc_turn`, `cc_history`, `cc_terminal`, `cc_nemesis`, `cc_roster`, `cc_counters`, `cc_ledger`, `cc_meta` | Fixed-width, no pointers. Optional values have a `known` bit; **a clear bit requires a zero value**. |
| Run-bound policy | `cc_schedule` | Belongs to one run UID. Frozen when the run starts. Lives in the custom run overlay (C10), not in `cc_meta`. |

None of the structs is a file format. The file format is the layout in section 2.6.

### 2.2 Limits and defaults

From the handoff: 16 Nemeses, 5 turns, 64 events per turn, free scouting, one voucher per record, windows 27/77/127/177,
metadata at most 64 KiB encoded and 128 KiB of runtime storage. Added by this PR: 8 remembered ledger entries, 8
remembered held items per record, 4 moves, item keys of at most 32 characters (the longest pinned key,
`MYSTERY_ENCOUNTER_GOLDEN_BUG_NET`, is 32 [ran: scan of `reward-core/gen/out/reward_data.c`]).

ID ceilings are the largest values in the pinned enums: species 8901, move 920, ability 319, biome 50, nature 24, status
7, hit result 15. They are *range* checks. Whether a species/form/move is a real, constructible one is membership in the
game's tables, which `custom-core` does not link; that check belongs to the adapter (C03/C05/C10) and is not claimed
here. `tests/layout_test.c` includes the generated `pr_enums.h` read-only and fails if any pinned enum exceeds a ceiling.

### 2.3 History

`cc_event` records one observed fact about a *target* position's Pokemon: a shown hit (`DAMAGE`), a shown heal (`HEAL`),
HP lost with no shown hit (`HP_LOSS`, never attributed), a faint, a switch in or out, or a status change. The target's
species and form are always known (they come from the same snapshot as the event); everything else is optional:

- `shown` is the number the game displayed (a magnitude). `hp_delta` is the HP change measured from consecutive
  snapshots (damage negative, heal positive). They are separate fields and are never added.
- `source_*` and `move_id` are present only when an adapter established them. `HP_LOSS` may carry none of them; `FAINT`
  may carry a source and move but no amounts; switches carry only the target and `hp_after`.
- `seq` strictly increases across the whole history; `cc_history_validate` rejects reordering and duplicates.

`cc_turn` holds up to 64 events, oldest first. When a 65th arrives the oldest is dropped, `dropped` counts it and
`CC_TF_EVICTED` is set; the validator requires exactly this combination (`dropped > 0` iff `EVICTED`, and a full turn
when evicted). `CC_TF_SOURCE_LOSS` (a game buffer overflowed upstream of the observer) and `CC_TF_BEGAN_MID`
(observation started late) are the other two ways a turn is incomplete. **A turn with `flags != 0` must not be
presented as a complete damage audit.** The turn key is `(battle_seq, turn)`: `battle_seq` counts battles inside the run
(assigned by the adapter) so equal turn numbers from different battles never merge; `turn 0` is "before the first turn".

`cc_history` keeps the last five turns, oldest first, keys strictly increasing. `CC_HF_BEGAN_MID` records that observation
began after the run's first battle (a resumed save: older turns could not be seen).

`cc_terminal` is the value-owned run-end report: outcome, wave, biome, and, when reliably known, the finishing move and
the finishing and fallen actors (stored whole: id, species, form, side, level, so they survive without the history).
`txn_id == 0` is a candidate; a stored report has it set.

### 2.4 Nemesis record, lifecycle and the insertion gate

`cc_nemesis` is **historical**: `orig_level`, `ivs`, `items`, `nature`, `ability`, `moves` are what the enemy *was*.

Identity versus combat template. The record is not a combat template and carries no "scaled" copy. A future rematch
builds the enemy through the game's own construction path at the ordinary level generated for that wave (handoff
C05/C10). Which identity fields the construction applies is C05/C10's decision; version 1 only fixes that (a) `orig_level`,
`ivs` and `items` are never copied into combat, (b) a record whose required ability or moves are unknown is refused
(`CC_REFUSE_ABILITY_UNKNOWN` / `MOVES_UNKNOWN`), and (c) nothing is silently substituted.

Lifecycle:

```
STORED --> SCHEDULED --> CHALLENGED --> DEFEATED --> REWARD_CLAIMED (terminal)
   ^           |              |
   +-----------+--------------+     (window passed or declined, run ended, rematch lost)
```

`cc_nemesis_transition` is the only mutator and carries the bookkeeping with the state: `STORED->SCHEDULED` sets
`sched_run_uid`; `SCHEDULED->CHALLENGED` sets `challenge_wave` (one of 27/77/127/177); returning to `STORED` clears both;
`DEFEATED->REWARD_CLAIMED` sets `reward_txn` once. A second claim, a skipped state, or a missing argument returns an error
and leaves the record unchanged. `cc_nemesis_validate` requires the bookkeeping to agree with `life`, so a record cannot be
hand-edited into an inconsistent state without being rejected on decode.

Insertion gate. A defeat creates a record only if `cc_insertion_check` returns `CC_SUPPORT_OK`: the terminal outcome is
`DEFEAT`; the terminal's finishing actor is an enemy whose id equals `source_pokemon_id`; the record carries
`FINISHER_VERIFIED`, a known ability and moves; and no trait is set (boss, fused, Tera, doubles, trainer-owned, mystery,
fixed/scripted, or itself a rematch enemy). Otherwise the post-mortem is stored and no record is created, and the
refusal says why. A stored record always has `traits == 0`.

Roster invariants (`cc_roster_validate`): at most 16; `record_id` strictly increasing along the slots (so slot order is age
order); `(source_run_uid, source_pokemon_id)` unique; at most one record SCHEDULED or CHALLENGED. A full roster overwrites
the oldest record with `cc_life_overwritable` (STORED or REWARD_CLAIMED). Insertion itself is C04.

### 2.5 Schedule, identifiers and the ledger

`cc_schedule` (one per custom run): the frozen candidate ids (ascending), the one chosen id, the four windows with their
state and skip reason, and the custom stream's algorithm id, seed and post-draw state. States: `NO_CANDIDATE` (frozen
roster empty), `ACTIVE`, `OFFERED`, `DECLINED` (consumed), `ACCEPTED`, `EXHAUSTED` (all four skipped). Window rules:
skipped windows form a prefix, then at most one of offered/declined/accepted, then pending windows; `DECLINED` and
`ACCEPTED` are never followed by another offer. The algorithm field is `CC_RNG_ALGO_V1` iff a record was chosen; the
algorithm itself is C05's. **Nothing here draws a random number.**

Identifiers. `cc_run_uid = (install_nonce << 32) | run_seq`, never 0, allocated from `cc_counters` by
`cc_alloc_run_uid`; `cc_record_id` and `cc_txn_id` come from the same struct. The allocator hands out the current value and
advances the counter; **the caller must persist the counter before using the id**, otherwise a crash could reissue it.
Exhaustion is `CC_E_OVERFLOW` and changes nothing.

Transactions. A *terminal* transaction stores the report, the history and, when the gate passes, the Nemesis in one write
of one blob; a *reward claim* sets a record's `reward_txn`. `cc_ledger` remembers the last 8 committed transactions
(`txn_id` strictly increasing). Idempotency key: `TERMINAL` by `run_uid`, `REWARD_CLAIM` by `record_id`.
`cc_ledger_append` refuses a repeated key and a non-increasing `txn_id`, so a retried end-of-run write either finds the
earlier result or is rejected; it cannot insert twice. Because the ledger is inside the same blob as the roster, a
backup rollback (section 1.6) rolls both back together.

### 2.6 Wire layout, version 1

Little-endian, packed, no padding. One blob (`custom` metadata) inside the save transport's own CRC envelope. Decoding is
C04's; these widths fix its output. A decoder rejects: wrong magic, schema other than 1 (`CC_E_VERSION`, never
defaults), `reserved != 0`, `body_len` different from the bytes present, any count over its limit, any enum or `known`
bit not defined, and every `validate` failure of the decoded structures.

```
envelope  magic u32 = 0x4D435250 ("PRCM")   schema u16 = 1   reserved u16 = 0   body_len u32          12
body      counters                                                                                    20
          n_ledger u8, then n x ledger entry                                              1 + 22 x n
          n_records u8, then n x nemesis                                                 1 + 369 x n
          has_report u8 (0|1); if 1: terminal, then history
counters  next_record_id u32, next_run_seq u32, install_nonce u32, next_txn_id u64                      20
ledger entry  txn_id u64, run_uid u64, record_id u32, kind u8, outcome u8                               22
terminal  run_uid u64, txn_id u64, outcome u8, known u8, biome u8, wave u16, finish_move u16,
          finish_actor (9), fallen_actor (9)                                                            41
actor     id u32, species u16, form u8, side u8, level u8                                                9
history   n_turns u8, flags u8, then n_turns x turn                                         2 + sum(turn)
turn      battle_seq u32, wave u16, turn u16, dropped u16, flags u8, n_events u8, then n_events x event
                                                                                      12 + 38 x n_events
event     seq u32, kind u8, target_side u8, source_side u8, target_form u8, source_form u8, result u8,
          known u16, target_species u16, source_species u16, move_id u16, target_id u32, source_id u32,
          shown i32, hp_delta i32, hp_after i32                                                          38
nemesis   record_id u32, life u8, traits u8, known u16 | source_run_uid u64, source_pokemon_id u32,
          source_wave u16, source_biome u8 | species u16, form u8, gender i8, shiny u8, orig_level u8,
          nature u8 | ability u16, passive u16 | ivs 6 x u8 | n_moves u8, moves 4 x u16 |
          n_items u8, items 8 x (key 33 bytes NUL padded, arg i16, stack u16) |
          finish_move u16, fallen_species u16, fallen_form u8 |
          sched_run_uid u64, challenge_wave u16, reward_txn u64                                         369
```

Maximum body: 12 + 20 + 177 + 5,905 + 1 + 41 + 12,222 = **18,378 bytes** (17.95 KiB) against the 64 KiB budget. Event
and item arrays are written with their *used* counts, not their capacity, so a typical state is far smaller.
`cc_wire_meta_size` and `cc_wire_meta_size_for` compute the exact encoded length for a given content with checked
arithmetic.

### 2.7 API

All functions are pure. Everything up to `cc_status_str` is in `cc/model.h`; the reducer (section 2.8) is in `cc/reducer.h`.

| Function | Purpose |
|---|---|
| `cc_obs_validate` | range checks on one observation, including the optional attribution (section 2.8) |
| `cc_actor_validate`, `cc_event_validate`, `cc_turn_validate`, `cc_history_validate`, `cc_terminal_validate` | history structures, canonical-unknown rule, ordering, eviction invariant |
| `cc_nemesis_validate`, `cc_nemesis_support`, `cc_insertion_check`, `cc_nemesis_validate_stored`, `cc_roster_validate` | records and the roster |
| `cc_life_can_transition`, `cc_life_overwritable`, `cc_nemesis_transition` | lifecycle |
| `cc_schedule_validate` | the frozen schedule |
| `cc_counters_validate`, `cc_alloc_record_id`, `cc_alloc_txn_id`, `cc_alloc_run_uid` | identifiers |
| `cc_ledger_validate`, `cc_ledger_find`, `cc_ledger_append` | idempotency |
| `cc_meta_init`, `cc_meta_validate` | the whole persisted state, including cross-structure checks (ids below the counters, ledger below `next_txn_id`, an empty report is empty) |
| `cc_size_add`, `cc_size_mul`, `cc_wire_meta_size_for`, `cc_wire_meta_size` | checked size arithmetic and encoded length |
| `cc_status_str`, `cc_refusal_str` | text for statuses and refusals |
| `cc_reducer_init`, `cc_reducer_begin_run`, `cc_reducer_end_run`, `cc_reduce`, `cc_reducer_validate`, `cc_turn_summarize` | C02: the bounded history reducer (section 2.8) |

Errors are `cc_status`: `CC_E_NULL, RANGE, COUNT, UNKNOWN_BITS, NONCANONICAL, ORDER, DUPLICATE, CONSISTENCY, LIFECYCLE,
OVERFLOW, VERSION`.

### 2.8 Reducer (C02)

`cc_reduce` folds batches of observations (`cc_obs`, section 2.1) into the last five turns of at most 64 stored events each
(`cc_history`, section 2.3). It is a pure function of the reducer state and the batch: no globals, no clock, no random
numbers, no file, no allocation, no game call. It does not observe anything itself. Who calls it, and from where in
`drain()`/`handle_event`, is C03.

**What a raw event gives it** [read, `battle-core/include/pr/events.h`, `phases.c`, `switching.c`]:

| Raw event | `cc_obs` kind | Stored `cc_event` |
|---|---|---|
| `PR_EV_DAMAGE` (`phases.c:3662`: `battler` = the position that was hit, `target` = -1, `id` = HitResult, `value` = the amount applied after clamping) | `DAMAGE` | `DAMAGE`; `shown` = `value`; `result` = `id` |
| `PR_EV_HP`, `value > 0` (`phases.c:3503`: the heal amount, capped at max HP and not at the room left) | `HP` | `HEAL`; `shown` = `value` |
| `PR_EV_HP`, `value < 0` (`phases.c:3032`: `p->hp - before`, computed by the engine) | `HP` | `HP_LOSS`; **`hp_delta` = `value`**, not `shown`: it is a measurement, not a displayed number |
| `PR_EV_HP`, `value == 0` | `HP` | **none**: the header cannot say whether it is a heal of 0 or status damage of 0 (`phases.c:3503` can emit a 0 heal, `:3032` a 0 loss), so it is counted in `ignored` and nothing is guessed |
| `PR_EV_FAINT` (`phases.c:2745`, before `status = FAINT`) | `FAINT` | `FAINT` |
| `PR_EV_SWITCH_OUT` (`switching.c:477`, snapshot still shows the leaving Pokemon) | `SWITCH_OUT` | `SWITCH_OUT` |
| `PR_EV_SWITCH_IN` (`switching.c:523`, `:699`, snapshot shows the entering Pokemon) | `SWITCH_IN` | `SWITCH_IN`; `value` is not stored (the header calls it the party slot, the code passes the field slot `field`; the reducer does not rely on either) |
| `PR_EV_MOVE_ANIM` (`battler` = user, `target` = its target) | `MOVE_ANIM` | **none**: context only, counted in `ignored`; it is never linked to a later event |

`CC_EV_STATUS` is not produced: no raw event in this card's list reports a status change, and deriving one from differing
`status_after` snapshots would be a guess about when and why. Every other `PR_EV_*` is outside the contract.

**Batch identity.** An input is a `cc_batch`: `run_uid`, `batch_id`, `battle_seq`, `wave`, `turn`, `flags` and up to
`CC_MAX_BATCH_OBS` (384, `GC_MAX_BATTLE_EVENTS`) observations, oldest first. All of one batch belongs to one turn; a range
that spans turns is split by the adapter into batches with their own ids. `batch_id` is chosen by the adapter, is `>= 1`,
and strictly increases within a run. The reducer remembers the id and a 64-bit FNV-1a digest of the **last accepted** batch
only. The digest is taken field by field in a fixed byte order, never over struct bytes, and covers the turn key, the flags and
every field of every observation; `run_uid` and `batch_id` are left out because they are equal wherever it is compared. Rejection is checked in this order and the first failure wins:

| Condition | Result |
|---|---|
| a NULL reducer or batch; `n_obs > 0` with no array | `CC_E_NULL` |
| unknown `flags` bit; `n_obs > CC_MAX_BATCH_OBS` | `CC_E_UNKNOWN_BITS`; `CC_E_COUNT` |
| `run_uid`, `batch_id`, `battle_seq` or `wave` is 0 | `CC_E_RANGE` |
| no run begun, or the run was ended | `CC_E_LIFECYCLE` |
| `run_uid` is not the active run | `CC_E_CONSISTENCY` |
| the stored state is structurally impossible (`n_turns` or a turn's `n_events` over its limit, or an accepted batch with no turn) | `CC_E_CONSISTENCY`, before any index is computed |
| `batch_id` below the last accepted | `CC_E_ORDER` (an older replay cannot be proven to be one, so it is never applied) |
| `batch_id` equal to the last, digest equal | **`CC_OK`, `duplicate = 1`, nothing changed**: a retry, an animation replay or a redraw is counted once |
| `batch_id` equal to the last, digest different | `CC_E_DUPLICATE` (the same identity with other content) |
| turn key older than the last turn (see below) | `CC_E_ORDER`; same battle, other wave: `CC_E_CONSISTENCY`; `BEGAN_MID` on a turn that already exists: `CC_E_CONSISTENCY` |
| an observation fails `cc_obs_validate` | its status, with `bad_obs` = its index |
| `seq` would pass `UINT32_MAX` | `CC_E_OVERFLOW` |

**Atomicity.** The batch is validated and fully simulated first (every stored event is built and passed through
`cc_event_validate`); only then is the state written. A rejected batch changes **nothing**, including the remembered id and
digest, so a corrected batch may be resubmitted with the same id. No copy of the 12 KB history is made (the 3DS stack is
small); `cc_reducer` is meant to be static or part of the run state, never a local variable.

**Turn identity.** The key is `(battle_seq, turn)`. Against the newest stored turn: a larger `battle_seq` opens a new turn
(its `wave` may not decrease); the same `battle_seq` needs the same `wave`, and a larger `turn` opens a new turn while an equal
one appends to it (this is how the several ranges of one turn merge); anything older is `CC_E_ORDER`. So identically
numbered turns of different battles or waves never merge. An accepted batch always materializes its turn, even with no
stored event; the adapter should not submit batches for things it did not observe. A sixth turn drops the oldest.

**Event order.** `seq` is assigned by the reducer from a per-run counter starting at 1, in batch order. An event that is
evicted keeps its number, so numbering does not depend on what was retained; the retained events of one turn are
consecutive, and `dropped` says how many came before them. The counter never wraps: a batch that would pass `UINT32_MAX`
is `CC_E_OVERFLOW`.

**Eviction.** Appending to a turn keeps the newest 64 events. The number to drop is computed once per batch: first from the
turn's existing events (oldest first), then from the front of the batch's own new events when the batch alone exceeds 64.
`dropped` counts every event lost (saturating at 65535) and `CC_TF_EVICTED` is set, exactly the combination
`cc_turn_validate` requires. The tail of a turn beyond `n_events` is kept zero, so two states built from the same input
are byte-equal (tests compare with `memcmp`).

**Shown versus measured.** The two are separate fields and are never added (section 2.3):

- `shown` is the number the event carries for a hit or a heal. It is as faithful as the event.
- `hp_delta` is **measured by the reducer** from `hp_after` snapshots: `hp_after` of this observation minus the previous
  snapshot of the same position, taken only when that previous snapshot is known and shows the same species and form. It
  is the movement of the snapshot *between two observations*. The snapshot is taken when the event is added, not when the
  damage was dealt (section 1.1), so HP moved by a neighbouring hit can fall on either event; **over a complete turn the
  per-position sum is the net movement, per event it is not an attribution.** A sign that contradicts the kind (HP rose on a
  `DAMAGE`, fell on a `HEAL`) leaves `hp_delta` unknown and is counted in `sign_conflicts`; the reducer does not invent a
  value. `HP_LOSS` keeps the engine's own `value` as `hp_delta` and is not cross-checked.
- The tracker forgets a position on `SWITCH_OUT` and `FAINT`, on an observation with no snapshot, when the snapshot shows
  another species or form, at a new `battle_seq`, and after a batch flagged `SOURCE_LOSS` or preceded by a gap. It is set by
  `SWITCH_IN`. Two different Pokemon of the same species and form in one position with no switch event between them cannot be
  told apart; the game always emits the switch.

**Attribution is pass-through.** Nothing in a raw event names an attacker, a move that caused a hit, or a cause of loss
(section 1.1), and the reducer never derives them: not from the size of an amount, not from a preceding `MOVE_ANIM`, not
from which side was hit. `cc_obs` carries optional `src_*` fields (the adapter's assertion, established independently of the
event, with `src_known` bits equal to `CC_EK_SOURCE_SIDE/ID/SPECIES/MOVE`); the reducer copies them to the event when set and
leaves the bits clear otherwise. They are accepted on `DAMAGE`, on a heal and on `FAINT`, and refused on `HP_LOSS`, on
switches and on `MOVE_ANIM`. An adapter that sets them owns the claim. **This amends the C01 `cc_obs`** (new fields,
`sizeof` 40 to 52); nothing else used it yet.

**Completeness.** A turn is a complete record of what the observer was given iff `flags == 0`. It is incomplete when
events were evicted (`EVICTED`), when the batch said the game's own buffer overflowed (`CC_BF_SOURCE_LOSS`, from
`battle_events_overflow`/`events_overflow`), when observation started late (`CC_BF_BEGAN_MID`, only on the batch that opens the
turn; `cc_reducer_begin_run(.., resumed)` sets `CC_HF_BEGAN_MID` for the history), or when a batch id was skipped. A skipped
id means the reducer never saw a batch it was owed: both neighbours, the incoming turn and the previous newest turn, get
`SOURCE_LOSS`, and the tracker is cleared. The first batch of a run may have any id.

`cc_turn_summarize` gives per target side the count of events by kind and, for `DAMAGE`, `HEAL` and `HP_LOSS`, the number of
events with a shown amount, the number with a measured delta, and the two sums **separately**; `exact` is `flags == 0`. When
`exact` is 0 the sums are lower bounds over the retained events and must be presented as such, never as a turn total.

**Runs.** `cc_reducer_begin_run(uid, resumed)` resets everything for a new run. The same `uid` again is `CC_E_DUPLICATE` (UIDs
are never reused, section 2.5). `cc_reducer_end_run` closes the run: later batches get `CC_E_LIFECYCLE`, so animation or
screen replay after the end cannot change what was captured. It is idempotent. The history is read from `r->hist`; C03 copies
it by value after `end_run`.

**Cost bounds.** Per batch: three linear passes over the observations (digest, simulation, write), one `memmove` of at most 63
events when a turn overflows, one of at most four turns when the window rotates. These are operation counts. **No Old 3DS
timing was measured**; memory is `sizeof(cc_reducer)` (section 5) plus the caller's batch array.

**Not in this contract.** No codec (C04), no live observer, no stable-identity derivation (section 1.8), no rendering, no
SD access.

### 2.9 The observer in the app (C03)

`game/source/combat_observer.c` is the only code that feeds the reducer. Decisions, each checked by `game/tests/observer_test.c`:

- **Where.** `run_screen.c drain()` calls `co_observe()` right after the queue is pulled from game-core, before
  `be_convert_events` and before anything is played. Observation therefore happens once per pull of the game's queue and does not
  depend on animation, skipping or the player's speed setting. `end_run()` calls `co_finish()` first; `new_run()` and a
  successful `rs_resume()` call `co_begin_run(false/true)`. A continued run has `began_mid` set and its history says so.
- **Input.** `GC_EV_WAVE` (battle number), `GC_EV_TURN` (turn, starting at 1) and `GC_EV_BATTLE` (a range into
  `run->battle_events`, read only during that drain). One non-empty range becomes one batch with a fresh id; a range with no
  observed event makes no batch, so no empty turn. Events before the first TURN of a battle are turn 0. The turn number is kept
  across drains so the chunks after a switch prompt join the same turn.
- **Mapping.** `PR_EV_DAMAGE`, `PR_EV_HP`, `PR_EV_FAINT`, `PR_EV_SWITCH_OUT`, `PR_EV_SWITCH_IN`, with the field snapshot
  the event carries (HP -1/-1 when the snapshot is unusable). `PR_EV_MOVE_ANIM` and all text/animation events are not given to the
  reducer and are not counted as lost.
- **Loss.** `battle_events_overflow`, a full game-core queue (`GC_MAX_EVENTS`), `ev_overflow` (once), a refused batch, an event the
  reducer cannot represent, a bad range and a drain number that never arrived all set `SOURCE_LOSS`: on the stored turn when there is
  one, otherwise carried to the next batch or to `report.untracked_loss`. Nothing is guessed to fill a gap.
- **Not known, never inferred.** Who dealt a hit, with what move, and what caused an HP loss: no raw event carries it, so every
  stored event has its source bits clear. Pokemon identity is species and form from the snapshot; stable ids stay unknown (a
  snapshot `party_slot` can be re-ordered between the event and the drain). The run uid is a counter of this session.
- **Report.** `co_finish()` copies the history by value into a `co_report` that owns its data. A defeat names the player Pokemon of
  the last `FAINT` in the retained turns as "last to fall", or "not recorded". It is idempotent.
- **Shape.** About 52.9 KB static (reducer, 384 observations, report). Built as one translation unit
  (`custom_core_build.c`) because the devkitPro Makefile names objects by basename and `custom-core/src/history.c` collides with
  `battle-core/src/history.c`; renaming it is an area X change and is left to a later task.

### 2.10 The store and the terminal transaction (C04)

Two parts. `custom-core` (pure, no I/O) has the codec (`cc/codec.h`, `codec.c`) and the transaction (`commit.c`). The app side
(`game/source/custom_store.c`) owns the file. Everything below is exercised by `custom-core/tests/codec_test.c` and `commit_test.c`
(core) and `game/tests/store_test.c` and `flowtest.c` (app, over the real `save.c` in a temporary folder).

**The file.** One save slot named `custom` (`custom.sav`, `.sav.bak` next to the run slots and `system.sav`), holding the section 2.6
blob (magic `PRCM`, schema 1) inside the transport's header and CRC. It is separate from the run slots, the system data and every
existing schema: none of them is read, written or changed by C04. The largest possible file is 18,378 bytes plus the 12-byte transport
header (budget 64 KiB); a file holding one full report and a full ledger is 12,486 bytes.

**Codec.** Every field is written and read one by one with its stated width, little-endian; no struct is copied to or from a buffer.
`cc_meta_decode` is strict: magic, schema (any other than 1, older or newer, is `CC_E_VERSION`), the reserved field, a body length that
is exactly the bytes present, every count, every enum and "known" bit, and then every `cc_meta_validate` rule (ordering, duplicates,
canonical unknowns, ids below the counters, lifecycle bookkeeping). A blob that decodes re-encodes to the same bytes. On any error the
output is all zero bytes. Truncation, trailing bytes and a count over its limit are `CC_E_FORMAT` / `CC_E_COUNT`.

**Transaction.** `cc_commit_terminal(meta, terminal, history, candidate, result)` is one run's end as one change of `meta`:
the report with its history, the counters, the ledger entry and (only when a candidate is given and `cc_insertion_check` accepts it)
one roster record. Rules:
- Idempotent by run uid through the ledger (the last 8 runs): a run already in it returns `replayed` and changes nothing, even if the
  roster has changed since. The uid must be one this install issued (`cc_run_uid_issued`).
- All checks run before the first write; any error leaves `meta` exactly as it was.
- Ids are allocated by the transaction: the transaction id always, the record id only when a record is inserted. A candidate with a
  record id of its own, a lifecycle other than STORED, or the same (run, Pokemon) source as a stored record is refused.
- Insertion is bounded: a full roster (16) gives up its oldest record that `cc_life_overwritable` allows (STORED or REWARD_CLAIMED);
  when none is, the report is stored with `CC_REFUSE_ROSTER_FULL` and no record. Records stay ordered by record id.
- With no candidate (the enemy that ended the run is not reliably known) the report is stored with an unknown cause.

**The app's store.** `cs_begin_run` (installed as the observer's uid source) reads the file on first use, hands out the next uid and
writes the advanced counter **before** returning it, so a uid is never issued twice across a crash. `cs_commit` stages the transaction
on a copy of the state, encodes it, writes it, and only after the write succeeded does memory change: a refused write leaves the card
and memory as they were. `cs_commit` is safe to call again for the same report.
- A file that is there and cannot be used (damaged, a different schema, or unreadable) is **never repaired, overwritten, rotated or
  deleted**: the game goes on, no uid is issued, nothing is stored, and the result screen says why in one line. A missing file is a
  new install. A damaged main file with a good backup is used from the backup (the transport's fallback), and the next write replaces
  the damaged file.
- A run's uid is per observation, as in section 1.4: a resumed run gets a new uid and its report is stored under it.
- If the card refuses the write at the run's end, the result screen says the report could not be saved, and leaving the screen tries
  once more (idempotent, so a write that did land is not repeated). After that the report is lost; nothing retries in the background.

**The boundary that stays open.** The app calls the transaction with **no candidate**. Building a record needs the finishing enemy's
species, ability, moves, held items and a stable Pokemon id, and the battle events carry none of them (section 1.8, items 1 and 2).
The tests exercise real insertion, overwriting and refusal with synthetic candidates; the live path stores the report with an unknown
cause and an empty roster. Closing this belongs to C10 and needs a battle-core change that is not part of this task.

**Memory.** `custom_store.c` holds 57,410 bytes of static storage (the `cc_meta` state, the staging copy and the encode and read
buffers), beside the observer's about 52.9 KB: about 110 KB of the 128 KiB runtime budget.

### 2.11 The rematch policy (C05)

`custom-core/include/cc/scheduler.h`, `src/scheduler.c`. Pure like the rest of the module: it decides, as data. It spawns no enemy,
reads no game state, touches no wallet and grants nothing; the adapter (C10) acts on the answers. Every call is atomic in memory
(all checks before the first write, an error changes nothing) and a retry or a reload that repeats a call gets the first answer back
and changes nothing.

**The stream.** The custom policy stream is SplitMix64, `CC_RNG_ALGO_V1`: `state += 0x9E3779B97F4A7C15`, then the two multiply-xorshift
rounds (30, 27) and the final xorshift 31, all modulo 2^64. `cc_rng_v1_pick(state, n)` is the next output modulo `n`. The bias of the
modulo is below `n / 2^64` (under 10^-18 for the 16 records a roster holds); it is accepted instead of a rejection loop so that the
run's draw is **exactly one step** of the stream. The seed is an argument and is recorded in the schedule; the module reads no clock,
no `rand`, no `Math.random` and no gameplay or gacha generator (the adapter keeps the seed independent of them, section 4). The tests
hold the first five outputs of two seeds, and a table of state, first output and `output % n` (n = 1..16) for six seeds, produced by an
independent Python implementation (`tools/golden_rematch.py`) that agrees with the published SplitMix64 vectors.

**At the start of a run** (`cc_rematch_start`). The candidates are the records whose life is STORED, whose `cc_nemesis_support` is
`CC_SUPPORT_OK`, and (when the adapter passes the mask) whose snapshot the adapter can rebuild; a record the adapter cannot represent
(a form, ability, move or item it does not know) is never silently reshaped, it just is not a candidate. They are frozen, in record
id order, into `frozen_ids`. No candidate: state `NO_CANDIDATE`, no draw. Otherwise one draw picks `frozen_ids[pick]`, the stream's
state after it is stored, the record goes STORED to SCHEDULED for this run, and the windows 27, 77, 127, 177 start PENDING. Nothing
ever draws again, so scouting, redrawing, reloading or declining cannot change who was chosen. Refused with no change: a run uid of
0, an invalid roster, a record still held by any run (`cc_roster_release_except` returns what a crashed run left first).

**Windows** (`cc_rematch_window`). At each window wave the adapter reports, before the wave is generated, what the encounter is
(ordinary wild, trainer, fixed or scripted, mystery, boss, final wave, unsupported mechanic) and whether the enemy is fused, Tera or
in a double battle. Only an ordinary single wild encounter without those flags is eligible; the reasons keep a fixed precedence
(final, boss, trainer, mystery, fixed, unsupported, then fused/Tera/doubles as unsupported). The first pending window that is eligible
is OFFERED. An ineligible one is SKIPPED with its reason and the next window is tried; after the fourth, the schedule is `EXHAUSTED`,
nothing happens this run and the record goes back to STORED. Windows must be reported in order (`CC_E_ORDER` otherwise). The answer
for a window already answered is the stored answer and the report is not looked at again: an offered window gives the same offer, an
accepted one gives `CC_PROP_RESUME` (the reload case), a skipped one its reason.

**The proposal** is data: kind (nothing / offer / resume), why, the skip reason, the opportunity 1..4, the wave, the record id, the
level rule `CC_LEVEL_ORDINARY_WAVE`, and the fixed reward allowance of **one regular egg voucher**. There is no stat buff and no level
formula: the enemy is built by the game's own path at the level the wave already generated, and custom-core knows no level or
stat formula. The record keeps its original identity for history; what the rematch applies of it is the adapter's decision, as
section 2.4 says.

**The answer** (`cc_rematch_decide`). Accept: window and schedule ACCEPTED, record CHALLENGED at that wave. Decline: window DECLINED,
record back to STORED, schedule DECLINED, **consumed**: no later window is offered this run. The same answer again is a replay; the
opposite answer is `CC_E_LIFECYCLE`. **The outcome** (`cc_rematch_result`): won, CHALLENGED to DEFEATED; lost, back to STORED
(the schedule stays ACCEPTED, so there is no second try this run). Replays are idempotent; the opposite outcome is refused.
`cc_rematch_view_of` is read only.

**The reward** is once per record and does not depend on a write. `cc_reward_entitlement_of` is a function of the record's life
alone: DEFEATED is owed one voucher, REWARD_CLAIMED is claimed, everything else was not won. `cc_reward_claim(meta, record)` is the
transaction (section 2.5's `REWARD_CLAIM`, keyed by record): it allocates a transaction id, appends the ledger entry and moves
DEFEATED to REWARD_CLAIMED with that id, all on `cc_meta`, staged and committed together. A claim of a REWARD_CLAIMED record is a
replay and changes nothing; a ledger and a record that disagree are `CC_E_CONSISTENCY`. If the write of the staged copy fails the
record is still DEFEATED and still owed, and the adapter claims again; **the voucher is granted only after that write succeeded**
(C08/C10 own the wallet side; custom-core never mutates a wallet).

**Resuming the same decision.** Everything needed is in `cc_schedule` (run uid, seed, state after the draw, frozen ids, chosen id, the
four window states and reasons) and in the record's `sched_run_uid` and `challenge_wave`. The schedule has **no wire codec in this
task**: it lives in C10's overlay of the run's save. Two things C10 must keep: the schedule's `run_uid` must be the same value when a
run is loaded again (C04 gives a resumed ordinary run a new uid per observation, section 2.10, so C10 must key the schedule by the uid
it stored, not by the observer's), and `cc_roster_release_except(roster, current_run_uid)` must run before `cc_rematch_start`.

### 2.12 The Nemesis Radar (C06)

`game/source/radar_screen.c` (+ `radar_assets.c`, the bridge to the menus layer for species types, icons, sprites and move and nature
names, the same split `party_icon.c` makes), opened from a fourth title entry, "Nemesis Radar". **It is a read-only look at the stored
roster and starts nothing.** It never writes the custom file, never calls the scheduler, spends no voucher or currency, draws no random
number, and has no fight, invasion or purchase button; its text says "Live rematches are not available yet." because they are not (C10).

- **What it holds.** At `radar_open` it validates and **copies** the roster (about 6.3 KB) and an optional schedule into its own
  state, and keeps nothing of the caller's pointers except the exit callback. A roster or schedule that fails custom-core's validators
  is refused (`radar_open` returns false) and the app says so instead of showing it. The copy is the model the screen runs on: the
  owner can change or free its data while the screen is up.
- **The list** (bottom screen): newest record first, five rows (icon, name, level, wave and biome, life tag Remembered / Held /
  Challenged / Beaten / Claimed). UP/DOWN move, LEFT/RIGHT move a page, A opens the Scout page, B leaves. The top screen shows the
  highlighted record: name, level, where it ended a run, the type in words (only for the eighteen known types), and a sprite read
  from the SD card **after the cursor has rested 140 ms** (a held button crosses records without reading any), with the species icon
  at double size standing in until then and when the packs have no sprite. A faint tint of the type's colour (alpha 70 of 255, under
  the text) is drawn only for a known type; it is colour only and no rule reads it.
- **The Scout page** (A): a free page of what the record remembers: species, level, shiny and gender, wave and biome (or "an unknown
  place"), the last of the player's Pokemon to fall, the final move, ability, passive (only when recorded), nature, moves, IVs and held
  items (the game's names; a key the locale does not name is shown readable; a cut-short loadout says "and more"), then the rematch
  status line, and the policy's four windows with their reasons when a schedule names this record. Anything the record does not
  hold is "not recorded". Twelve lines are visible; UP/DOWN scroll.
- **Rematch status** (`radar_rematch_text`, one line per record): "No rematch is scheduled." for a stored record; "Held by a run whose
  plan is not loaded here." for a held record when no schedule is given; the real window ("Planned for this run. The next window is
  wave N.", "Offered at wave N.", "Accepted at wave N.", declined, exhausted) only when the schedule passed in names that record and
  agrees with the record's life; when they contradict each other it says so and shows neither as scheduled. **The app has no stored
  schedule** (it lives in C10's overlay of the run's save), so the app passes none: the windows are shown only in host tests that pass a
  frozen schedule.
- **Empty and unusable.** No file or no records: "No enemy is remembered yet." and that this version does not add enemies during play.
  An unusable file (damaged, other version, unreadable): its reason from `cs_get_state`, "It is left as it is."; the file is not read
  again or written.
- **Input.** Buttons only. **There is no touch input for `ui.h` screens** (only the shop reads the touch screen), so there are no
  touch buttons to keep in parity; B leaves from either page. Leaving calls `on_exit` once; the screen takes no more input after it.
- **Boundaries.** The title is the only entry (the result screen has none: it lists the current run, not the roster). Enemies appear
  here only if a file holds records; the app cannot create one (section 2.10), so on a real install the Radar shows the empty state until
  C10. `game/tests/mkcustom.c` writes a file with records for looking at the screen on an emulator.

### 2.13 The capsule presentation prototype (C07)

`game/source/capsule_model.c` (pure motion and timeline, no ui, no clock, no allocation, no random numbers) and `capsule_screen.c` (the
two screens). **It is a presentation over a result list it is given, not a machine that decides anything**, and it is not connected to the
gacha (that is C09, over C08's profile).

- **The result contract.** `cap_fixture`: `n` in 1..25 items, each a tier (0..3: a colour and a word, nothing else) and a non-empty,
  NUL-terminated label of at most 31 bytes. `capsule_screen_open` refuses (returns false, opens nothing) NULL, an empty or oversized list,
  a tier out of range and a label with no end or no text; it **copies** the fixture, so the caller's data may change or go away. Nothing
  in the model writes to a fixture and the shown labels are the supplied labels in the supplied order. An egg or capsule is not shown as
  an unlocked Pokemon: the result list prints the label as given and the screen says "These results were fixed before the machine ran."
- **Who may open it.** Only the debug file: `rundriver.txt` line `capsule=<1..25>` opens the preview before the title with `n` results
  made from the position alone (`capsule_debug_fixture`: tiers by position, labels "Fixture result N"). Normal play has no entry, and
  the text says it is a preview ("A preview with fixed results.", "Nothing is bought."). No wallet, profile, unlock, voucher, save or
  RNG function is called by either file.
- **Motion.** Gravity 0.0016 px/ms^2, restitution 0.35 (a bounce under 0.08 px/ms stays down), the chute above the field's top, floor
  at y 164. Item i has a fixed slot (reading order, five to a row, rows centred, the last row centred on itself) and a fixed x spread at
  the chute, both functions of i and n only; the capsule arrives over its slot with its horizontal speed chosen for the fall time, so
  **where a capsule lands is not where it is "pushed": slot i always holds item i** (no collision is simulated between capsules, only the
  floor; capsules in different rows are 28 px apart and in a row 40 px apart for radius 10, so they cannot overlap at rest). The bottom
  row leaves the chute first, one every 16 ms after the 200 ms knob turn.
- **Time.** dt is in milliseconds (ui.h). `cap_step` ignores dt <= 0 and NaN, clamps one call to 100 ms (a long pause is not a jump),
  and integrates in fixed 8 ms steps, at most 16 per call, so the result does not depend on the frame rate. The model is SETTLED when
  every capsule is open (measured natural settle: 1216 ms for one result, 1280 ms for 5, 10 or 25) or at **1500 ms at the latest**, when
  whatever still moves is placed at its slot and opened. Allocation: none (the model is one fixed struct, `sizeof(cap_model)` = 1,748 bytes).
- **Skip.** B or START during the knob turn or the drop calls `cap_skip`: every capsule goes to its slot, open, the results are shown at
  once, and `on_settled` fires then. Nothing is pulled again and no result changes. A pull before the first A is not skippable (B leaves).
- **Callbacks.** `on_settled` fires **once per opening**, the first time the results are all shown (time or skip) and never if the
  screen was left before. `on_exit(how)` fires **once per opening, from the player's input only** (B before the pull, A / B / START
  after the results have been up 300 ms, so a held button does not close it at once); `how` says whether the results were shown. Neither
  is called from `leave()`, a timer or a stale screen; a callback of an earlier opening never reaches a later one (the screen keeps one
  state, cleared at each opening). The callee of `on_exit` leaves the screen (the debug entry shows the title).
- **Input.** Buttons only; A is the pull ("the same pull" as a touch gesture would be). **There is no touch**: the framework gives no
  touch input to ui screens (only the shop reads the touch screen), so a gesture would need a new API; none is invented. No pressure or
  crank input. A held button's auto-repeat is ignored through `ui_input_repeating()` (added to ui.c: true only while the framework
  delivers a repeat), so holding A pulls once and holding B does not skip and then leave.
- **Both screens.** Top (the 320 x 180 field): a backdrop, the chute, capsules drawn as seven horizontal strips of a circle (lid lifts
  and a mark appears when open), the selected result outlined while scrolling. Bottom: the title, the knob (the owned
  `images/egg/gacha_knob` from the locally built packs, turned by the model's knob angle; a plain rectangle when the packs lack it), the
  instruction lines, then the result list (five rows, tier chip and label). No art is added to Git.

### 2.14 The profile transaction owner (C08)

**Audit of "who owns the profile".** C01's section 1.5 said no file in `game/` mentions `gc_system`; that was true then and is not now. Main
already owns **one shared `gc_system`**: `system_data.c` loads `save/system.sav` once (`sysd_get`, static 724 KB), attaches it to each run
and to the starter select, and writes it at run save points (`sysd_save`, skipped while unchanged, never over an unreadable file). What
did **not** exist: any caller of `gc_system_gacha_pull`, any persistence of the gacha scene RNG, and any way to change the profile together
with the custom file. C08 adds exactly that, as an adapter over the existing calls; `gc_system_gacha_pull`, `gc_system_save/load`,
`save_read/save_write` and the system.sav and custom.sav formats are unchanged and read-only here (`git diff main -- game-core` is empty).

`game/source/profile_tx.c` (+ small hooks in `system_data.c` and `custom_store.c`). **One write-ahead envelope**, the file `ptx`, is the
only authority for a transaction in flight. It holds: the next transaction id, the gacha scene RNG (its own stream, never a run's battle or
generation RNG), the last 8 immutable results, and at most one pending transaction. The save library is atomic per file only, so
cross-file consistency is a redo log: the commit is ONE `ptx` write that carries the candidate profile bytes, the resulting scene RNG, the
transaction id and the result together; the profile file and the custom file are then rolled forward from it, each step idempotent.

```
            begin_pull / begin_claim                    execute
   IDLE ---------------------------------> PREPARED ---------------> pull ONCE on the live profile (claim: add the voucher)
    ^        1 write: request + txn id        |                          |
    |        (clock, entropy seed, record,    | refused (cost, capacity, |  serialize the candidate (gc_system_save)
    |         digest of the profile now;      | voucher cap, not owed,   v
    |         scene RNG seeded at the first)  | profile changed)    ONE write: candidate bytes + scene RNG + result + txn id
    |                                         v                     fails -> live profile put back from its pre-image
    |  <------- 1 write: result kept (REFUSED), nothing else        |        (bytes a restart would read); stays PREPARED, retry
    |                                                               v
    |                                                          COMMITTED   <== the commit point; the result is durable
    |                                                               |
    |   roll-forward, each step idempotent:                         |  1. claim only: custom.sav (cc_reward_claim: ledger + CLAIMED)
    |   boot (ptx_recover), the gate on sysd_save, or a retry       |  2. system.sav = the candidate bytes
    +<--------------------------------------------------------------+  3. ptx: pending cleared (the result stays in the ring)
```

Writes in a clean run: a pull 4 (begin, commit, system.sav, clear), a claim 5 (plus custom.sav), a refusal 2.

**What each crash leaves, and what finishes it** (all tried by the test, section 5.7):

| Stopped | Card | Next boot / next call |
|---|---|---|
| before `begin` | nothing | nothing happened |
| after `begin` | PREPARED: request saved | `ptx_pending()` names it; `ptx_execute(txn)` runs the pull from the SAVED clock and seed (the caller supplies nothing again) |
| after the commit write | COMMITTED, profile and custom still old | boot sees the card holds the profile the transaction started from, installs the candidate in memory and on the card (claim: the claimed flag first), clears. The same result, no second pull |
| after `custom.sav` (claim) | claimed, voucher not yet | boot: the claim is a replay, the profile is installed, cleared |
| after `system.sav` | profile applied | boot sees the candidate on the card, clears |
| after the clear | done | the result is kept: asking again returns it |

Rules that make this safe:

- **Nothing before the commit point.** The pull runs once, on the live profile, then the profile is serialized; if the commit write fails the
  live profile is loaded back from the bytes serialized before the pull (the same bytes a restart reads), the scene RNG is not advanced, no
  result is stored and nothing is handed to the caller (`PTX_X_IO`; a retry is safe). A profile that cannot be serialized refuses at
  `begin` and at `execute` without running anything. If even the put-back fails the module stops for the session (`PTX_X_FATAL`).
- **Exactly one pull.** The result is looked up by transaction id before anything runs, so a retry, a repeated tap or a restart gets the
  committed result (`PTX_X_DONE`) and never calls `gc_system_gacha_pull` again. Clock and entropy seed are fields of the request, saved at
  `begin`; the pull reads them from the saved record, never from a live clock.
- **A committed result is returned even if the roll-forward failed** (`PTX_X_DONE_APPLY_PENDING`): it is durable, and the files catch up.
  Meanwhile `sysd_save` asks the gate first and **returns false instead of writing the profile**, so nothing can overwrite the candidate
  with a profile that does not contain it; the next save point (or boot) finishes the transaction and then saves.
- **A request is only run on the profile it was made for.** `begin` records the canonical digest of the profile; `execute` refuses
  (`ptx:profileChanged`, closing the request) if it differs. This covers the one hazard of the save library (section 1.6: a damaged main
  file silently rolls the state back by one write): a `ptx` file restored to its PREPARED write after the pull had completed can never run
  the pull a second time. Tested by putting the old file back.
- **The boot does not guess.** If a transaction is COMMITTED and the card's profile is neither the one it started from nor the candidate, or
  the profile is unreadable, the boot reports it in words, writes nothing and refuses new transactions. A `ptx` file that is damaged or
  from a newer version is reported and left as it is.
- **The reward is claimed once, with its voucher.** `ptx_begin_claim` needs the record owed (`cc_reward_entitlement_of`: DEFEATED), the
  voucher is one regular voucher (`CC_REMATCH_REWARD_VOUCHERS`; no currency, no unlock), and the roll-forward writes the claimed flag
  (`custom_store.c`: `cs_claim_reward`, one `cc_reward_claim` transaction) BEFORE the profile, so a voucher is on the card only once the claim
  is. A record claimed meanwhile by another path is refused at `execute` (`cc:notOwed`); a voucher count at the 32-bit maximum is refused
  (`profile:voucherCap`) and the record stays owed.
- **No default wallet.** Both `begin` calls refuse (`PTX_E_UNUSABLE`) when the profile file is unreadable: the defaults a damaged install
  falls back to are not the player's vouchers. Nothing in this module writes a voucher count except the claim.
- **Memory.** The profile is never copied as a struct. A transaction allocates two buffers of 1 MiB (the largest profile gc_system_save can
  write, a synthetic every-species-maxed one, is 714,695 bytes; a normal one is 3 to 90 KB) and frees them; the module's static state is
  14,624 bytes (`ptx_static_bytes`). The `ptx` file holds the candidate profile bytes while a transaction is pending, so it is as large as the
  profile for that time and a few KB otherwise.
- **Boot.** `app_start` calls `ptx_boot()` (installs the gate, finishes a half-applied transaction). Cheap when there is no `ptx` file. The scene
  RNG's seed is supplied once by the app before its first begin (`ptx_set_scene_seed`); C09 must supply it from the real clock.

### 2.15 The capsule machine (C09)

**What exists now.** The title has a custom entry **Capsule Machine** (upstream reaches its gacha from the in-run menu, which this app does not
have yet; the Radar set the precedent for a custom title entry). It opens `gacha_screen.c`: LEFT / RIGHT choose one of the three machines, UP / DOWN
one of the five options, A asks, the question opens on **No**, Yes buys, B leaves. It is a menu over three things that already exist and adds no
rule: game-core's `gc_system_gacha_pull` (machines, options, costs, capacity, pity, guarantees, entropy, display order), C08's `profile_tx`
(the only thing that runs a pull and commits it) and C07's capsule screen (the presentation). Files: `gacha_host.c/.h` (logic, no ui),
`gacha_screen.c/.h` (menu), `capsule_screen.c` (words and `autoplay`), `profile_tx.c` (the shown-mark), `title_screen.c`, `app_flow.c`.

**Upstream core behavior (the source decides; this task compares nothing new and calls nothing by a copy).** Which voucher an option debits,
how many, how many eggs it adds, the capacity, the refusal order (capacity before cost), every egg field, the pity and guarantee rules, the
statistics and the display permutation are `gc_system_gacha_pull`'s, validated against the pinned upstream by `egggachadiff.sh`
(game-core/EGG-GACHA-VALIDATION.md; **not re-run here**, game-core is unchanged). **Original custom presentation:** the title entry, the
menu, the question, the words ("Move Gacha", "Legendary Gacha", "Shiny Gacha", "Regular / Plus / Premium / Golden", the refusal sentences),
that the eggs are shown by tier and an id (no species, no shiny flag: nothing hatches yet), the capsule motion (C07), the shown-mark, and the
restart behavior below. The three machine notes ("better odds of a rare egg move / of a legendary with a daily featured one / of a shiny
egg") say what the source's offsets do and nothing more; the odds are not stated as numbers.

**The table is read from the source.** `gh_rules` runs the real `gc_system_gacha_pull` once per option on a scratch profile with plenty of
every voucher and reads which voucher dropped, by how much, and how many eggs appeared; it finds the capacity by a binary search on a
one-egg pull. Nothing is inferred from the cursor labels. The result is cached (about twelve scratch pulls once per run of the app; not
timed on a device). The test checks it against upstream's documented table (1 Regular -> 1, 10 Regular -> 10, 1 Plus -> 5, 1 Premium -> 10,
1 Golden -> 25, 99 eggs) and the screen's "can be bought / which refusal" against `gc_system_gacha_pull` itself over 7,800 comparisons.

**One accepted pull.**

```
 menu --A--> question (No) --Yes--> gh_buy:  unreadable profile / unusable record -> message, nothing written
                                             a purchase or unshown eggs pending   -> BUSY, nothing written
                                             refused by the table (same keys, same order as the source) -> message, nothing written
                                             ptx_begin_pull (clock and entropy seed supplied ONCE, saved) -> ptx_execute (ONE pull, ONE commit)
                                             -> capsule screen on the committed result (autoplay: already accepted)
 capsule screen settles (time or B/START skip) -> ptx_mark_revealed(txn) -> closing returns to the menu
```

- **Duplicates.** Held buttons act once (`ui_input_repeating`); A on the question's default is "No", so a double tap asks and declines; a
  screen that was left takes no input; `gh_buy` is not re-entrant and is BUSY while `ptx_pending()` or an unshown committed pull exists. So
  no second transaction id can be created before the first is finished and shown. There is no touch path (the framework routes none to ui
  screens): the buttons are the whole control.
- **Refusals.** The table refuses (not enough vouchers, too many eggs; capacity first, as the source) before anything is written: zero
  writes, files byte-identical, no record created. If something still reaches the source and it refuses (the profile changed after
  `begin`: `ptx:profileChanged`), that is the same: nothing changes and nothing is shown. A refused write (SD card) before the commit point
  changes no live profile and no RNG (C08), shows no egg and says "Nothing was spent"; a purchase already begun is finished with A (the same
  transaction, the same request) or, if the player leaves, the next time the machine opens.
- **The shown-mark.** `ptx_unrevealed` / `ptx_mark_revealed` (envelope version 2: one `revealed_txn` watermark; version 1 files are still read
  and count everything they kept as shown, since no screen could show it). Marking happens when the capsule screen settles, by time or by
  skip, and is one `ptx` write that carries a pending transaction unchanged. If the card refuses it the eggs are not offered again in this
  session and the mark is retried; if the app stops before it is made, the **same** eggs are shown once more after the restart (never a new
  pull). If the player closes the app mid-animation, the same.
- **Opening the machine** first finishes what a restart left (`gh_resume`: the pull runs from the SAVED clock and seed, so the result is the
  one the interrupted purchase would have had), then shows any committed pull that was not shown, then the menu.

| Stopped during a purchase | Next time the machine opens |
|---|---|
| before the first write | nothing happened: the menu |
| after `begin` (PREPARED) | the pull is finished from the saved request and its eggs are shown |
| after the commit write, or later in the roll-forward | boot finished the files; the eggs are shown |
| during the animation, or the mark not written | the same eggs are shown again once, then marked |
| after the mark | the menu; nothing is shown |

- **What a result is.** Eggs in the profile (`sys->eggs`), tier, id, hatch waves, machine, clock, variant, shiny and move index as the source
  made them. Not caught, not unlocked, no candy, no hatching call, no custom form or item, no rehydration (`EggData.toEgg` stays excluded). The
  capsule screen shows `Egg <id mod 10^6>` per capsule in the source's display order (not the order the capsules land in) with the tier; the
  dex and starter data are compared byte for byte with the pre-pull profile in the test.
- **Time and entropy.** The request's clock is `time(NULL) * 1000` and its entropy seed text is built from `time`, `clock` and a counter,
  supplied once per request by `gh_set_env`'s defaults (tests inject fixed ones). They are saved in the request; a retry or restart never
  re-reads a clock. The daily featured legendary follows the console's clock; no synchronization is claimed. Animation timing never reaches
  a random or date decision: the animation starts after the commit.

## 3. Choices to confirm

These are version-1 decisions this PR had to make. The handoff either left them open or said "documented policy". Each
is easy to change before C02 builds on it.

1. **Only rematch-supportable enemies are remembered.** The roster is a rematch roster, so a boss, fused, Tera, doubles,
   trainer-owned, mystery, fixed, unattributed or itself-a-rematch enemy creates no record (the post-mortem is still
   kept). Alternative: store them for history and mark them unrematchable, at the price of roster slots.
2. **Eviction.** A full roster overwrites the oldest record that is STORED or REWARD_CLAIMED; SCHEDULED, CHALLENGED and
   DEFEATED (reward not yet claimed) are protected so an entitlement cannot be evicted away.
3. **A defeated Nemesis is retired, not deleted.** DEFEATED to REWARD_CLAIMED is terminal; the record stays visible and
   evictable and is never scheduled again. The handoff says records are cleared "per the documented policy"; this is that
   policy for version 1. A lost rematch returns the record to STORED. A defeat *by* a rematch enemy creates no record.
4. **Run UIDs come from a persisted counter plus an install nonce** (section 2.5), so tracking a run costs one durable
   custom write at run start, and an ordinary run's UID is per observation (section 1.4). The alternative, a random 64-bit
   value, would avoid the write but the app's only entropy is 32 bits of boot time.
5. **Version 1 has no "rematch lost" event in the schedule.** The record returns to STORED; the schedule stays ACCEPTED.

Added by C02 (section 2.8):

6. **A zero `PR_EV_HP` is ignored**, not recorded as a heal or a loss, because the header cannot tell them apart.
7. **A skipped batch id marks the two neighbouring turns incomplete** instead of being tolerated silently. Alternative:
   require contiguous ids and refuse a gap, at the price of a bad batch stopping the reducer for the run.
8. **`cc_obs` gets optional attribution fields.** Alternative: no attribution in version 1, so every event's source is
   unknown. The fields exist because C01's audit names `attacks_received` as a possible narrow source and C03 cannot add
   fields to `custom-core`.
9. **`hp_delta` is a snapshot-to-snapshot measurement**, not a per-hit damage figure (section 2.8, shown versus measured).

Added by C04 (section 2.10):

10. **Report before slot clear** (section 1.3). Alternative: clear first, accepting a missing report after a crash.
11. **A uid is persisted before use, per observation.** A run that is never ended still spent a uid; only the last 8 runs' outcomes
    are kept, so a long gap of uncommitted runs does not matter. Alternative: allocate at the end, which would not let the report
    carry the uid that was observed.
12. **An unusable custom file stops custom tracking for the session, it never repairs itself.** The cost: the player must delete
    `custom.sav` by hand (there is no screen for it) to restart tracking. Alternative: move it aside to a new name, which writes
    to the card on its own and was not authorised.
13. **One retry on leaving the result screen**, no background retries.

Added by C05 (section 2.11):

14. **One draw per run, taken modulo the candidate count.** The bias (below 10^-18) is accepted so the stream is never advanced a
    variable number of times. Alternative: a rejection loop, which would make the stream position depend on the output.
15. **Candidates are STORED, supported and adapter-representable records only**, frozen at the start of the run. A record that becomes
    eligible mid-run (a defeat is stored at the run's end) is not a candidate until the next run.
16. **A record is held only while the run can still use it.** When all four windows are skipped, or the offer is declined, the record
    returns to STORED at once; a crash or quit while it is held is cleaned by `cc_roster_release_except` at the next run start.
17. **Windows are reported in order, each one.** An adapter that never reports wave 27 and then reports wave 77 gets `CC_E_ORDER`, not a
    silent skip. Alternative: infer skipped windows from the waves seen, which hides adapter bugs.
18. **The reward allowance is one regular voucher per record, claimed in its own transaction**, and granted by the adapter only after
    the claim's write succeeded. A failed write leaves the record DEFEATED, so the entitlement is not lost and not repeated.

Added by C06 (section 2.12):

19. **The Radar is a title entry, not a results-screen one.** The result screen belongs to the run that just ended; the roster is
    cross-run. Alternative: also add a button there, once C10 can create records.
20. **The Radar keeps copies, not pointers** (about 6.3 KB roster, `sizeof(cc_schedule)` schedule). Alternative: read the live store
    each frame, which would make the screen change under the player's cursor and keep a pointer the owner may free.
21. **The sprite waits 140 ms of rest.** The SD card is slow (README); loading one sprite per record crossed would stall the cursor.
    The icon stands in meanwhile. The value is a guess at "feels instant when you stop"; it is not measured on a device.
22. **No touch.** The framework has no touch path for these screens. Adding one belongs to the framework, not to this screen.

Added by C07 (section 2.13):

23. **Slot i holds item i; the capsules do not collide with each other.** The wish for a "collision" is met by the floor bounce only:
    capsule-to-capsule collision would make the landing depend on contact order and tempt a result to depend on it. Alternative: a real
    pile, which needs a physics engine or much more tuning and gains nothing the player can use.
24. **Skip is B or START; a skip shows the supplied results at once.** A is only the pull, so a skip is not a second pull.
25. **A held button acts once** (`ui_input_repeating`); the results can be closed only after 300 ms.
26. **No touch gesture.** See section 2.13; touch needs a framework change.
27. **The 1.5 s limit is enforced by a snap, not by tuning.** Natural settling is 1.2 to 1.3 s; if integration ever ran long the capsules
    would jump to their slots at 1.5 s. Alternative: no limit, which is what the card forbids.
28. **Two callbacks, not one.** `on_settled` (the results are shown) and `on_exit` (the player leaves), because the card's "completion
    fires once regardless of skip" and "leaving and reentering" are different events: C09 will grant nothing on either; it grants in C08's
    transaction before the screen opens.

Added by C08 (section 2.14):

29. **A redo log carrying the candidate profile bytes, not one merged file.** Merging system.sav and custom.sav into one file would change
    both formats and the run slots' system-data path; a log in a third file leaves them as they are and costs one extra write. The log holds
    the whole candidate profile (3 to 90 KB, at most 715 KB) only while a transaction is pending.
30. **One transaction in flight.** A second `begin` is `PTX_E_BUSY`; C09 asks `ptx_pending()` when it opens. Simpler to reason about than a queue.
31. **Eight kept results.** Enough for a screen to ask again after a restart; older ones are forgotten (`ptx_result_of` false). The profile
    itself, not the ring, is what a pull changes.
32. **Claim order: custom file, then profile.** Alternative: profile first, which would leave a window with a voucher and an owed record on the
    card; the redo log makes either recoverable, this order matches decision 18.
33. **Divergence is reported, not repaired.** If the card's profile is neither the start nor the candidate, the boot stops and says so; guessing
    could discard a run's progress or double a grant.
34. **A request is bound to the profile it began on** (a digest, refused if the profile changed). Alternative: run it anyway; that is the
    double-pull hazard section 1.6 describes.
35. **The roll-forward writes the candidate bytes, not the live profile.** A run that changed memory in between is saved later through the
    gate (it fails, and says so, until the transaction is finished). Alternative: write memory, which loses the proof that the card holds the
    candidate.

Added by C09 (section 2.15):

36. **The machine is a title entry.** Upstream's gacha lives in the in-run menu, which the app does not have. Alternative: wait for that menu;
    the screen takes plain `on_exit`, so moving it is a wiring change.
37. **The question opens on No, and a held button acts once.** A costs real vouchers and the same A opened the question, so Yes must be a
    second, deliberate input. Alternative: no question (one tap buys), which the held-A hazard makes unsafe.
38. **The option table is read from game-core by pulling, not written in `game/`.** Cost: about twelve scratch pulls (one 724 KB scratch
    profile, freed) the first time the machine opens, not timed on a device. Alternative: a copy of upstream's table, which is the
    "inferred from cursor labels" risk the card names.
39. **Eggs are shown by tier and id, not by species or shiny.** Species and shininess are decided and stored; showing them is hatching's job
    (not in scope) and would turn the reveal into a loot display the original game does not have at this step. The tier is the colour.
40. **Shown is a watermark in the purchase record (envelope version 2), set when the capsule screen settles.** Alternative: a flag per result,
    or marking at commit; the first costs a format change per field, the second would lose the reveal after a crash. Cost: a mark lost
    to a crash shows the same eggs once more.
41. **The words are plain English, not upstream's locale strings** (the machine names, the refusal sentences). The strings are not loaded
    for custom screens (the Radar and the post-mortem do the same); the refusal KEYS are the source's.
42. **The clock is the console's** (`time(NULL)`), supplied once per request and saved with it. A wrong console clock picks a different daily
    featured legendary; it cannot change a result already committed.

## 4. What C02 to C10 inherit, and what they still must decide

| Task | Imports | Still undecided / must verify |
|---|---|---|
| C02 reducer (decided in section 2.8) | `cc_obs`, `cc_event`, `cc_turn`, `cc_history`, the eviction invariant | nothing left open: the raw-event mapping, the batch and sequence rules and how `shown` and `hp_delta` are filled are in section 2.8 |
| C03 observer + UI (decided in section 2.9) | `cc_reducer`, `cc_reduce`, `cc_turn_summarize`, `cc_terminal`, `cc_history`, the "complete only when `flags == 0`" rule | nothing left open for C03, except that stable ids and the finishing source are not derivable from the events (section 2.9); C04 must decide the durable run uid |
| C04 store (decided in section 2.10) | `cc_meta`, wire widths, `cc_insertion_check`, ledger, `cc_nemesis_transition` | nothing left open for the store itself. Live Nemesis insertion needs the finishing enemy's identity (section 1.8): C10 |
| C05 scheduler (decided in section 2.11) | `cc_schedule`, window waves, `CC_RNG_ALGO_V1`, lifecycle | nothing left open for the policy itself. C10 must: supply a seed independent of gameplay and gacha RNG, report each window's encounter kind and flags before the wave is generated, pass the representable mask, key the stored schedule by the uid it saved (not C04's per-observation uid), call `cc_roster_release_except` before `cc_rematch_start`, and persist the schedule (no codec yet). C08 must grant the voucher only after `cc_reward_claim`'s write |
| C06 radar (decided in section 2.12) | `cc_nemesis` read-only, `cc_schedule` | nothing left open for the screen itself. C10 must pass the run's schedule to `radar_open` once it persists one, and create records for it to show |
| C07 capsule preview (decided in section 2.13) | an immutable result list (`cap_fixture`), the framework's `ui` screens | nothing left open for the presentation itself. C09 must build the fixture from results C08 has already committed, never from the screen; the screen may be shown after or before the grant but must not decide or repeat it; touch needs a framework path |
| C08 profile (decided in section 2.14) | `cc_ledger` `REWARD_CLAIM`, `cc_reward_claim`, the shared `sysd` profile | nothing left open for the owner itself. C09 must: supply the scene seed from the real clock (`ptx_set_scene_seed`) and the request's clock and entropy once per tap, run a pull only through `ptx_begin_pull` / `ptx_execute`, keep the transaction id across retries and read `ptx_pending()` when the screen opens, show what `ptx_result_of` returns (never its own draw), and not call `gc_system_gacha_pull` or write a voucher count. C10 claims a won record with `ptx_begin_claim` |
| C09 machine (decided in section 2.15) | `ptx_begin_pull` / `ptx_execute` / `ptx_unrevealed` / `ptx_mark_revealed`, the C07 screen, `gc_system_gacha_pull` (read-only) | nothing left open for the machine itself. C10 must remember that a Nemesis reward voucher is spent through this same machine, that the machine is BUSY while a `ptx` transaction is pending (including a claim), and that the title entry is the only way in |
| C10 live | all of the above | every item of section 1.8 that is still missing |

## 5. Validation record

Run on the final tree of this PR, in a Linux container with gcc 13.3.0, clang 18.1.3 and Python 3.13 (4 vCPU). The PR
description repeats this with the head commit and the CI results.

| Command | Result |
|---|---|
| `make -C custom-core clean && make -C custom-core test` | exit 0 in 2 m 24 s |
| &nbsp;&nbsp;unit + layout, gcc `-O2 -std=c99 -Wall -Wextra -Wpedantic -Werror` | `model_test: 916 checks, 0 failed`; `layout_test: 55 checks, 0 failed` |
| &nbsp;&nbsp;the same under `-fsanitize=address,undefined -fno-sanitize-recover=undefined` | 916 / 0 failed and 55 / 0 failed, no sanitizer report |
| &nbsp;&nbsp;mutation check | `368 run, 368 killed, 0 survived, 0 invalid; 7 more are documented as behavior-neutral and not run` |
| `make -C custom-core CC=clang BUILD=build/clang unit-test` | 916 / 0 failed and 55 / 0 failed, no warnings |
| `make -C custom-core arm-check` with Ubuntu `arm-none-eabi-gcc` 13.2.1 and the 3DS CPU flags (`-march=armv6k -mtune=mpcore -mfpu=vfp -mfloat-abi=hard -mtp=soft`) | all six sources and both test files compile with `-Werror`; this evaluates `layout_test.c`'s static assertions with ARM type sizes |
| the same target in the `custom-core-arm` CI job (`devkitpro/devkitarm` image, `/opt/devkitpro/devkitARM/bin/arm-none-eabi-gcc`), run 38155367297 on head `e71cdb9` | passed: same flags, `-Werror`, all eight files compile |

**Sizes.** `sizeof` was measured on x86-64 (`make -C custom-core report`) and, with a probe file, with the ARM
compiler above; **the two agree for every type**, so the structs have no 64-bit-only padding:

| Type | `sizeof` | Type | `sizeof` |
|---|---|---|---|
| `cc_obs` | 40 | `cc_nemesis` | 392 |
| `cc_actor` | 12 | `cc_roster` (16 records) | 6,280 |
| `cc_event` | 40 | `cc_schedule` | 120 |
| `cc_turn` (64 events) | 2,572 | `cc_counters` | 24 |
| `cc_history` (5 turns) | 12,864 | `cc_ledger` (8 entries) | 200 |
| `cc_terminal` | 48 | `cc_meta` | 19,432 |
| `cc_item` | 38 | `cc_meta` + `cc_schedule` + `cc_obs` | 19,592 (budget 131,072) |

Encoded size of version 1: 35 bytes empty, 18,378 bytes at every limit at once (budget 65,536). The ARM `.text` of the
six objects at `-O2` is 751 + 2,280 + 2,432 + 712 + 1,360 + 1,284 = 8,819 bytes with Ubuntu's compiler and
751 + 2,408 + 2,768 + 716 + 1,372 + 1,392 = 9,407 bytes with devkitARM (CI log). These are compile-time figures. They say
nothing about speed on an Old 3DS; no run, benchmark or hardware measurement was made.

**What the mutation check is.** `custom-core/tools/mutations.py` holds 368 single-change mutations: 14 changes to the wire constants, 27
limits and ceilings, and 327 changes to validators, the lifecycle table, the allocators, the ledger and the size
arithmetic (an off-by-one on every count and ID ceiling, each removed ordering, uniqueness, canonical-zero and lifecycle
check, each removed identity comparison of the insertion gate, a swapped idempotency key, a shifted eviction, a skipped
overflow guard). It applies one change at a time to a scratch copy, rebuilds both tests with ASan/UBSan and requires a
failure. A mutation that does not match exactly once, or does not compile, fails the run. It proves that the tests detect
those changes; it does not prove there is no other defect.

The first run left 37 mutations alive and 5 invalid. Of the 37: 28 were real gaps in the tests (for example the stored-UID
checks of `cc_meta_validate`, the report fields of an empty state, the upper character of an item key, both ends of a
few ID ceilings) and got new tests; 2 were clauses made redundant by an earlier check (`n_frozen < 1` and the
`LOADOUT`-known term of the truncation rule), which were removed from the code; 7 cannot change behavior and are listed
with their reasons in `python3 custom-core/tools/mutations.py --equivalent`. Of the 5 invalid: 3 are killed by
`layout_test.c`'s compile-time assertions (the script now counts that as a kill) and 2 were mistakes in the mutation table.

**Not run locally:** `make -C game-core test` and the 3DS build of `game/` (neither is touched; the existing `CI` workflow
runs them on this PR). The `custom-core-arm` job could not be run in the authoring container (no Docker daemon), which is
why the local ARM compile used Ubuntu's `arm-none-eabi-gcc`; the CI run above is the devkitARM result.

**Cost.** One worst-case `cc_meta_validate` (16 full records, 8 ledger entries, a report with 5 x 64 events) took 9.35 us
on the x86-64 authoring host in a hot loop (gcc `-O2`; scratch benchmark, not committed). The only comparable ratio in the
repo is battle-core's turn: 0.025 ms on a PC against 1.51 ms median on an Old 3DS (`bench-native`), about 60 x. Applied
here that suggests roughly half a millisecond, **an inference from a different workload, not a measurement**. Validation
runs at load and at run end, never per frame; the figure matters only if C02's per-event reducer or C04's codec turn out to
be hot, and those are the places to measure on the console.

### 5.1 C02 (history reducer)

Run on the final tree of the C02 PR, same container (gcc 13.3.0, clang 18.1.3, Python 3.13, 4 vCPU). The PR description
repeats this with the head commit and the CI results.

| Command | Result |
|---|---|
| `make -C custom-core clean && make -C custom-core test` | exit 0 in 7 m 04 s |
| &nbsp;&nbsp;unit + layout, gcc `-O2 -std=c99 -Wall -Wextra -Wpedantic -Werror` | `model_test: 953 checks, 0 failed`; `reducer_test: 6916 checks, 0 failed`; `layout_test: 56 checks, 0 failed` |
| &nbsp;&nbsp;the same under ASan + UBSan | 953 / 6916 / 56 checks, 0 failed, no sanitizer report |
| &nbsp;&nbsp;mutation check | `671 run, 671 killed, 0 survived, 0 invalid; 13 more are documented as behavior-neutral and not run` |
| `make -C custom-core CC=clang BUILD=build/clang unit-test` | 953 / 6916 / 56 checks, 0 failed, no warnings |
| `make -C custom-core arm-check` (Ubuntu `arm-none-eabi-gcc` 13.2.1, 3DS CPU flags) | all seven sources and three test files compile with `-Werror`; `reducer.o` `.text` 5,512 bytes |

The ARM compile found one real defect the host compilers did not: `uint32_t` is `unsigned long` there, and a helper took
`unsigned *`. Fixed. The `custom-core-arm` CI job (devkitARM) is the second ARM result and is reported in the PR.

**Sizes** (x86-64 `make report`; the ARM compiler agrees for `cc_obs` 52, `cc_event` 40, `cc_turn` 2,572, `cc_history` 12,864,
`cc_reducer` 12,928, `cc_turn_summary` 216, `cc_reduce_result` 16, `cc_meta` 19,432, checked with a probe file): the reducer
holds its own copy of the last turns, so the runtime figure is the persisted state 19,432 + schedule 120 + reducer 12,928 +
a full 384-observation batch 19,968 = 52,448 bytes against the 131,072 budget (enforced by a compile-time assertion in
`layout_test.c`). The persisted layout is unchanged (`cc_obs` is runtime only; encoded maximum still 18,378 bytes).

**What the reducer tests are.** `reducer_test.c` has literal fixtures with hand-computed expected events for: empty and
resumed runs; every raw event kind and its known bits; attribution passed through and never inferred (a move animation before
a faint leaves the faint with no source); shown and measured HP kept apart (multi-hit with ordered and with all-final
snapshots, a heal that shows more than it gained, status damage, sign conflicts); the HP tracker (reused field position and a
same-species replacement, switch-out, faint, missing snapshots, species/form change, batch, turn and battle boundaries); 64
and 65 events, 64 + 1 and 40 + 25 split across batches, 60 + 10, a lone 384, 384 on top of a full turn, the saturating
`dropped` counter; five and six turns; repeated, conflicting and older batches (the digest is varied byte by byte over every
observation field and the header); wave and battle boundaries; malformed order; a lost batch and the incompleteness flags;
a recorded session replayed into a fresh reducer (byte-equal), into the same one (refused) and into a new run; per-turn
totals including 64-bit sums and `INT32_MIN`; every structural check of a state. Every accepted call re-runs
`cc_reducer_validate`; every rejection checks the status, the named observation index, an all-zero result and a
byte-identical reducer. A 600-batch differential run compares the reducer with a naive one-event-at-a-time model (seed fixed
in the file).

**Mutation check, C02 part.** 306 new single-change mutations (reducer: batch digest, mapping, tracker, rejection order,
turn keys, eviction arithmetic, window rotation, flags, result fields, state validation, totals; plus the attribution
rules of `cc_obs_validate` and the new constants). Examples that must fail and do: eviction off by one in either direction,
dropping the oldest turn from the wrong end, accepting a replayed batch twice, ignoring the digest, merging identically
numbered turns, letting an ignored `PR_EV_HP` of 0 or a move animation move the tracker. The first run left 6 alive: 4 were
test gaps (a snapshot of 0 HP that is not a faint, a far out-of-range `n_turns`, a full unflagged turn being `exact`) and got
tests; 1 was a clear of a tail that is always overwritten by the same batch, and was removed from the code; 1
(`!t ||` in `cc_turn_summarize`) is behavior-neutral because `cc_turn_validate` refuses NULL with the same status. The
13 documented neutral mutations are listed with reasons by `python3 custom-core/tools/mutations.py --equivalent`.

**Not run / not established.** No Old 3DS timing or memory measurement; the reducer's cost is stated only as operation
counts (section 2.8). No test drives real `battle-core` events: the fixtures are written from the header and the cited
`phases.c`/`switching.c` lines, so whether the adapter (C03) produces exactly these observations is C03's to show.

### 5.2 C03 (observer and post-mortem screen)

Run on the final tree of the C03 PR (`main` after the music PR #225 merged into the branch), same container (gcc 13.3.0, Python 3.13,
Ubuntu `arm-none-eabi-gcc` 13.2.1, devkitARM in Docker, Azahar's libretro build in Old 3DS mode). The PR description repeats this
with the head commit and the CI results. Test commands run from `game/tests`.

| Command | Result |
|---|---|
| `make clean && make observer_test postmortem_test flowtest saves_test startertest starter_data_test starter_ui_test bgmtest runtest` | builds; the only warnings are the existing `ai.c:542` and `run_screen.c` (`pretty_key`, unused parameter) ones, also present on `main` |
| `build/observer_test` (ASan + UBSan) | 188 checks, 0 failed |
| `build/postmortem_test` | 250 checks, 0 failed (includes a check that every line is drawn in a style that was readable in Azahar) |
| `build/flowtest` | 71 checks, 0 failed (64 on `main`) |
| `build/saves_test`, `startertest`, `starter_data_test`, `starter_ui_test`, `bgmtest` | 68 / 737,655 / 51,280 / 4,470 / 11,421 checks, 0 failed |
| `python3 game/tests/observer_mutations.py` | 30 run, 30 killed, 0 survived, 0 invalid |
| `build/runtest 1000 0` (plain and `RUNTEST_SAVE=1`) | digest `1b42b380`, equal to `main`; 1000 reports checked, 50,069 batches, 94,651 observed events, 5,000 turns kept (5,000 complete), 4,331 empty ranges, 0 refused, 0 lost, 0 evicted |
| `RUNTEST_SAVE=resume build/runtest 1000 0` | digest `b764f66a`, equal to `main`; 998 of 1000 reports from a resumed run, 15,900 batches |
| `RUNTEST_SYS=all build/runtest 1000 0` | digest `80268d9a`, equal to `main`; 52,958 batches, 102,160 observed events |
| `make runtest-save` | OK (10 seeds) |

`runtest 1000 0` prints `FAILED` in all four modes **on `main` as well**, at the same digests, with the identical first violations
(`run 1000 wave 9: fewer than 5 evolutions cancelled (3)` plain and SAVE=1; `run 4023 wave 19: fewer than 5 evolutions completed (3)`
resume; `run 1000 wave 10: fewer than 5 evolutions cancelled (4)` SYS=all). That is the harness's coverage floor on evolutions at
1000 runs, not an observer check; it was not changed here.

ARM compile of the new and changed sources with the 3DS CPU flags (`-O2 -Wall -Wextra`, no new warning): `.text` / `.bss` bytes:
`combat_observer.o` 2,348 / 45,952; `custom_core_build.o` 14,787 / 0; `postmortem_screen.o` 5,631 / 6,992; `result_screen.o`
1,267 / 2,064; `app_flow.o` 2,222 / 1,369; `run_screen.o` 17,925 / 1,332. The 3DS build of `game/` in the devkitARM image also succeeds.

**What the tests are.** `observer_test.c` feeds literal gc events and battle events through `co_observe` and checks the stored
turns field by field (see section 2.9 for the rules). `runtest.c` checks every real run's report, including that every
`GC_EV_BATTLE` range was observed once and none was refused, dropped or replayed. `postmortem_test.c` draws against a fake text
layer (6 px per character) and checks the wording (including a list of words that would claim a cause), the pages, scrolling,
button handling, screen bounds and the text styles. The mutation script changes the wiring and the screen one edit at a time.

**Emulator check (Azahar, Old 3DS mode).** The app was built in Docker (`devkitpro/devkitarm`), the `gfx`, `font` and `data` packs
were built from the pinned upstream checkouts with `build_sd.py --only gfx,font,data`, and a run with one Magikarp (`rundriver.txt`:
a fixed seed and `starters=129`) was driven with scripted A presses through the real title, battle and result screens. The post-mortem
opened from the result screen showed the real events of three turns, and LEFT / RIGHT paged them. **The first defeat was forced:** a
Magikarp could not lose to the wave 1 Sentret in 9,000 emulated frames, so those screenshots come from a throwaway copy of `game/`
(outside the repo, not committed) that ends the run after five observed battle ranges; the turns, damage and HP shown are real
observations. **A real faint was then seen** with a second throwaway copy that only sets the first Pokemon's HP to 1 at the first
command (and ignores A and B in the post-mortem so the screen stays up): the engine itself fainted the Magikarp, the run ended on its
own, and the report read "Defeated at wave 1", "Last to fall: Magikarp", "Final blow: source not recorded", "Last 2 turns, nothing
missing", with the rows "Your Magikarp took 1 damage (HP 0)" and "Your Magikarp fainted" (red). **A clear was seen** with a third throwaway copy that ends the
run as a clear after five observed battle ranges ("Cleared at wave 1", no "Last to fall" line). **Scrolling was seen** with a fourth
throwaway copy whose report is made of invented events fed through the real observer and reducer (no battle can fill 64 events in a
turn quickly): "N more above / below" and the incomplete-turn notice drew on screen. That run also showed that a long notice could
run into the "N more above" label, so the notice is now cut short on a turn that can scroll (one test, one mutation). The first emulator run found that the totals,
rows, notice and footer, drawn in the 9 px emerald style, were unreadable; they now use `TS_INSTRUCTIONS_TEXT` (the 8 px pixel font the
party screen uses), and the same flow was re-run on the merged tree to confirm. Screenshots are in the shared project files, not in Git.

**Not run / not established.** No timing on an Old 3DS or a real console. The new game tests are not part of the CI workflow (it
runs game-core tests and the 3DS build). The finishing source of a defeat is never known. Stable Pokemon identity and a durable run
uid are not provided.

### 5.3 C04 (store and terminal transaction)

Run on the final tree of the C04 PR, same container as 5.2 (gcc 13.3.0, clang 18.1.3, Python 3.13, Ubuntu `arm-none-eabi-gcc` 13.2.1, devkitARM in
Docker, Azahar's libretro build in Old 3DS mode). The branch was based on `main` at `27f7c0c`; the digest comparisons below are against a
clean checkout of that commit. The PR description repeats this with the head commit and the CI results.

| Command | Result |
|---|---|
| `make -C custom-core clean && make -C custom-core test` | builds with `-std=c99 -Wall -Wextra -Wpedantic -Werror` under gcc and clang and under ASan + UBSan |
| &nbsp;&nbsp;`model_test` / `reducer_test` / `layout_test` | 980 / 6,916 / 56 checks, 0 failed (gcc, clang and sanitizers) |
| &nbsp;&nbsp;`codec_test` | 3,341 checks, 0 failed: every field's width and offset against hand-computed bytes, round trips, every truncation length, one bit flipped in every byte (each result refused, or a different valid state that re-encodes to the same bytes), every enum, count and canonical rule, a header with each wrong value, 3,000 rounds of random damage (2,914 refused, 86 accepted as other valid states, none non-canonical) |
| &nbsp;&nbsp;`commit_test` | 11,585 checks, 0 failed: insertion, a full roster giving up its oldest overwritable record, 15 protected records, `ROSTER_FULL`, replay, every refusal, a candidate with a record id, a duplicate source, errors leaving the state untouched byte for byte, a long random sequence of run ends and lifecycle moves (385 inserted, 369 of them overwriting, 2,618 report-only of which 1,524 roster-full, 626 replays) with the invariants checked after every 25 steps |
| `python3 custom-core/tools/mutations.py` (also run by `make -C custom-core test`) | 768 mutations run, 768 killed, 0 survived, 0 invalid; 18 more are documented as behavior-neutral and not run (60 codec and 37 commit mutations are new in this task) |
| `make -C custom-core arm-check` | passes with `-Werror`; `.text` bytes: `codec.o` 8,180, `commit.o` 1,168, `ids.o` 1,416, `meta.o` 1,268; no `.data` or `.bss` |
| `make -C game/tests store_test flowtest observer_test postmortem_test` | build; the only warnings are the existing `ai.c:542` and `run_screen.c` ones |
| `game/tests/build/store_test` (ASan + UBSan) | 284 checks, 0 failed |
| `game/tests/build/flowtest` | 96 checks, 0 failed (71 before this task; the 71 are unchanged) |
| `game/tests/build/observer_test`, `postmortem_test` | 188 and 250 checks, 0 failed (unchanged) |
| `python3 game/tests/observer_mutations.py` (numbers 31 to 47 are new) | 30 of 30 earlier and 16 of 16 new mutations killed (46 run, 0 survived, 0 invalid); one more (dropping the `CS_UNREADABLE` guard in `cs_begin_run`) is behavior-neutral, because the zeroed state's counters make `cc_alloc_run_uid` refuse anyway, and is not listed |
| `build/runtest 1000 0` plain, `RUNTEST_SAVE=resume`, `RUNTEST_SYS=all` | digests `2cee3f72`, `3ab8ea70`, `7701f977`, each equal to a clean build of `27f7c0c`; the plain run prints the same evolution-coverage `FAILED` as the base (needs 2,000+ runs; unchanged) |
| Docker `devkitpro/devkitarm` `make` in `game/` | builds `pokerogue.3dsx`; no warning from the new files |

**Storage** (what was measured, not estimated). `store_test` prints it: `sizeof(cc_meta)` 19,432; static storage of the store
57,410 bytes (`cc_meta` state, staging copy, 18,378-byte encode buffer and the module state) against the 128 KiB runtime budget; the
largest file 18,378 bytes (budget 64 KiB metadata); a file with one full report and a full ledger 12,486 bytes. Built for the 3DS
(`.bss`): `custom_store.o` 57,424, `combat_observer.o` 45,976, so about 103 KB for the two modules; `.text`: `custom_store.o` 1,993,
`custom_core_build.o` 25,075 (the whole of custom-core).

**What the tests are.** The codec and the transaction are tested by their own checks and their own mutations (every one must make a
test fail). `store_test.c` runs the real `save.c` in a temporary folder: a missing file, uid issue (written first, never repeated, a
failed write gives no uid and leaves memory as it was), one commit, replay, malformed reports, every kind of unusable file left byte
for byte as it was (garbage, a flipped byte, schema 0/2/3/0xFFFF, wrong magic, truncation, trailing byte, a count over its limit, a zero
counter, an oversized file, main and backup both damaged, an unreadable file), the backup fallback, a write that fails at each of the
four steps of `save_write`, and the budget. `flowtest.c` runs the app's flow over it. The wiring mutations change one line of
`app_flow.c` or `custom_store.c` at a time.

**Emulator check (Azahar, Old 3DS mode).** Same rig as 5.2 (packs from the pinned upstream checkouts, `rundriver.txt` with a fixed seed and
`starters=129`), on a **scratch copy** of the tree that only sets the first Pokemon's HP to 1 at the first command (not committed): the
engine itself fainted the Magikarp. Two runs started from an empty save folder and from a folder holding a 460-byte `custom.sav` of
text:
- Normal run: `custom.sav.bak` (the first write, at the run's start) is 47 bytes, `custom.sav` 250 bytes after the run's end; decoded
  with the codec: one ledger entry, the report of run uid 1 (defeat, wave 1, two turns), an empty roster. The result screen had no note.
- Damaged file: the run was played to the end and the result screen shows "The custom content file is damaged (it failed its
  checksum). It is left as it is, and nothing is saved to it until it is removed." The file is byte for byte the 460 bytes it was, and no
  `.bak` or `.tmp` appeared.
Screenshots are in the shared project files (`game-screens/c04`), not in Git.

**Not run / not established.** No timing of the SD writes on an Old 3DS or a real console (a run now costs two small writes: the first
one at its start, the second at its end). No resumed-run check in the emulator (the flow tests and `RUNTEST_SAVE=resume` cover it on
the host). The write-failure retry and the power-loss windows are exercised on the host with a fake `save_write` that stops at each step;
the real card was not made to fail. The new game tests are not part of CI. **No Nemesis is ever inserted by the app.**

### 5.4 C05 (rematch scheduler)

Run on the final tree of the C05 PR, same container as 5.2 and 5.3 (gcc 13.3.0, clang 18.1.3, Python 3.13, Ubuntu `arm-none-eabi-gcc` 13.2.1). The
branch was based on `main` at `699f9e8` (C04 merged). The PR description repeats this with the head commit and the CI results. The task touches
`custom-core/` and this document only: no game, battle, run or reward folder is read for writing, and the app does not call the scheduler.

| Command | Result |
|---|---|
| `make -C custom-core clean && make -C custom-core unit-test` | `-std=c99 -Wall -Wextra -Wpedantic -Werror`, gcc: model 980, reducer 6,916, codec 3,341, commit 11,585, **scheduler 121,222**, layout 56 checks, 0 failed |
| `make -C custom-core CC=clang BUILD=build/clang unit-test` | the same counts, 0 failed |
| `make -C custom-core san-test` (ASan + UBSan, `-fno-sanitize-recover`) | the same counts, 0 failed |
| `make -C custom-core purity-check` (also run by every `unit-test`) | all 10 library objects reference only `cc_*`, `mem*`, `__stack_chk_fail` (and the sanitizer runtime in the sanitizer build): no `rand`, `time`, `clock`, `malloc`, `printf`. Shown to fail when a `rand()` call is added to `scheduler.c` in a scratch copy |
| `python3 custom-core/tools/mutations.py` (also run by `make -C custom-core test`) | **915 mutations run, 915 killed, 0 survived, 0 invalid**; 22 more are documented as behavior-neutral and not run (147 are new in this task; the 768 earlier ones still die) |
| `make -C custom-core arm-check` | passes with `-Werror`; `scheduler.o` `.text` 4,528 bytes, no `.data` or `.bss` |

**What `scheduler_test.c` holds.**
- The stream: two seeds' first five outputs, and for six seeds the state after one draw, the first output and `output % n` for n = 1..16
  (96 picks), against values produced by the independent `tools/golden_rematch.py`.
- The start: no records, one, 16 (all ids and positions), a roster of mixed lives, the representable mask (a masked record never chosen,
  all masked is `NO_CANDIDATE` with no draw), the frozen list in id order and the stream's stored state; the same inputs give the same
  bytes; every refusal (NULL, run uid 0, an invalid roster, a record still held) leaves both structures byte for byte as they were.
- The windows: the offer at each of the four windows when the earlier ones are skipped, each skip reason of every encounter kind and flag
  (and the precedence between them), all four windows unavailable (`EXHAUSTED`, the record back to STORED, no further offer), a wave
  that is not a window, an out-of-order report, a schedule and a roster that disagree (before and after the offer), and an invalid
  schedule or roster at every entry point.
- The answer and the outcome: accept, decline (consumed: no later window is offered), win, loss (no second try), every replay of each,
  and every wrong answer refused unchanged.
- The reward: owed for DEFEATED only, claimed once as one transaction (counter, ledger entry and record together), replay, a forgotten
  ledger entry, a ledger that contradicts the record, an exhausted transaction id space, and every other life refused unchanged.
- Reload and scouting: a schedule copied at every step and asked again gives the same answers and the same bytes; 1,000 repeated reads
  and non-window questions leave the schedule, the roster, the policy stream's state and the process's ambient `rand()` sequence
  unchanged.
- A 3,000-round random walk (run uids, growing rosters, random masks, random encounters, random answers and outcomes, claims, some
  twice): 1,714 offers (874 accepted, 840 declined), 422 won, 452 lost, 1,280 exhausted runs, 24 runs without a candidate, 421
  rewards paid, 421 replays; after every call both structures are valid, at most one record is held, the stream never moves after the
  draw, and no record id is ever paid twice. The walk uses its own generator with the order of every call fixed, so gcc and clang run
  the same sequence.

**The selection and record-count mutations** (the card asks for them by name): the modulus one short, a shifted or masked or
high-half or reversed output, a second draw, a different seed, always the first or last candidate, a pick shifted by one, the
candidate count capped at 8, the first or last record never counted, one record treated as none, two records required, every one of
the four stream constants and shifts, and the candidate predicate (life, mask, claimed or defeated records allowed): each makes a
test fail. The mutations that survived on the first run (13) each exposed a missing test, now written, or code that did the same
thing twice (two assignments in `fill_offer` that the caller had already made, removed). Two guards remain that cannot change an
outcome and are listed under `--equivalent` with the reason: the `cc_nemesis_support` term of the candidate predicate (a valid
roster cannot hold an unsupported record) and the `run_uid == 0` early return in `cc_rematch_start` (both later checks refuse the
same status); the same list names the pending-window count in the view, which equals the non-skipped count while the schedule is ACTIVE,
and the claim guard on CHALLENGED records, which `cc_nemesis_transition` refuses with the same status before anything is committed.

**Not run / not established.** No emulator or console check: nothing in the app changed and nothing calls the scheduler. No timing. No
wire codec for the schedule (C10 keeps it in its overlay of the run's save; the in-memory size is `sizeof(cc_schedule)`, owned by the
caller). No claim that an enemy was spawned, that any wallet changed, or that the adapter's classification of a real wave is right: the
adapter (C10) and the reward side (C08) do not exist yet. The modulo bias (below 10^-18) is accepted, not eliminated (section 3, item 14).
The scheduler's status for this task: **COMPLETE** against the C05 card's scope (a pure policy with golden fixtures); the live
behavior it describes is **not shipped** until C10.

### 5.5 C06 (Nemesis Radar)

Run on the final tree of the C06 PR (same container as 5.2 to 5.4; devkitARM from the `devkitpro/devkitarm` image). The task touches `game/` and
this document only (`custom-core/` is not changed, so the custom-content workflow does not run for it).

| Command | Result |
|---|---|
| `make -C game/tests radar_test && game/tests/build/radar_test` (ASan + UBSan, `-fno-sanitize-recover`) | 2,618,873 checks, 0 failed (400 fuzz sessions of random buttons over random rosters, schedules, unusable files) |
| `game/tests/build/radar_test 2000` | 10.5 million checks, 0 failed |
| `make -C game/tests flowtest && game/tests/build/flowtest` | 103 checks, 0 failed (96 before, 7 new: title entry, unusable file explained and untouched, absent file not created, refused open not silent, back to the title) |
| `game/tests/run_host_tests.sh` (the CI script, now including `radar_test`) | all passed |
| `python3 game/tests/radar_mutations.py` | **50 mutations run, 50 killed, 0 survived, 0 invalid**; 2 more are documented as equivalent and not run (an unconditional ability or moves line: a valid roster always has both known) |
| `docker run --rm -v "$PWD":/src -w /src/game devkitpro/devkitarm make` | builds `pokerogue.3dsx`, no warning from the new files |
| Azahar (libretro core, the real packs, `game/tests/mkcustom.c` files: `few` 9 records, `full` 16, `empty`, `junk`, `future`) | title entry, list, top-screen sprite and type, Scout pages (minimal record, 8-item loadout, held record), paging, empty roster, unusable file (damaged, schema 2) and back to the title all as designed. After every run the `custom.sav` is byte for byte what the tool wrote. Screens are not in Git |

**What the host test holds.** Every drawn line, on both screens and every page, is inside its screen, in a style that is readable on the
real build, and free of words that offer a fight, invasion, purchase, location or currency. Navigation: order, scrolling, page jumps and
their clamps, the sound only when the cursor moved, A on an empty roster is the error sound, B on each page, one `on_exit` per opening and
no input after it. Words: every life tag and rematch line for every schedule state, a plan and a record that contradict each other, a
record the plan does not name, "not recorded" for each unknown field, the game's item names, stack counts, a cut-short loadout, an unnamed
biome, an 80-letter and a 40-letter name. Scouting: after any sequence of presses the roster, the schedule and the caller's meta are
byte for byte what they were, no RNG is touched, and the screen's copies survive the caller changing or freeing its data. Sprite: none
read before 140 ms of rest, one per stop, none for a record without one, freed on leaving. Tint: only for a known type, one rectangle,
alpha 70. Refusals: a roster or schedule that does not validate.

**Not established.** No Old 3DS device run and no timing (the 140 ms rest is a guess, section 3 item 21). No touch input exists, so no touch
parity was checked. The Radar shows no live rematch window in the app, because the app stores no schedule (C10). A real install shows the
empty state: no record can be created yet (section 2.10). The emulator check used files made by `mkcustom.c`, not records the game made.
Status for this task: **COMPLETE** against the C06 card's scope (the screen and its states over merged contracts); the live behavior it
will show is **not shipped** until C10.

### 5.6 C07 (capsule presentation prototype)

Run on the final tree of the C07 PR (same container as 5.2 to 5.5). The task touches `game/` and this document only.

| Command | Result |
|---|---|
| `make -C game/tests capsule_test && game/tests/build/capsule_test` (ASan + UBSan, `-fno-sanitize-recover`) | 223,456 checks, 0 failed (300 fuzz sessions of random buttons and frame times) |
| `game/tests/build/capsule_test 2000` | 1,253,784 checks, 0 failed |
| `make -C game/tests flowtest && game/tests/build/flowtest` | 108 checks, 0 failed (103 before, 5 new: `capsule=7` opens the preview with 7 results and not the title, leaving it is the title, 26 / 0 / -3 ignored) |
| `game/tests/run_host_tests.sh` (the CI script, now including `capsule_test`) | all passed |
| `python3 game/tests/capsule_mutations.py` | **43 mutations run, 43 killed, 0 survived, 0 invalid**; 4 more are documented as equivalent and not run (the `CAP_MAX_STEPS` loop bound, which one call's 100 ms clamp can never reach; the screen's repeated `settled_fired` and `exited` guards; the debug entry's repeated 1..25 clamp). Includes a duplicate-callback mutant (the settle reported every time it is asked; on_exit twice) and dt-unit mutants (dt read as seconds in the opening and in the gravity), all killed |
| `docker run --rm -v "$PWD":/src -w /src/game devkitpro/devkitarm make` | builds `pokerogue.3dsx`, no warning from the new files |
| Azahar (libretro core, the real packs, the Docker-built `pokerogue.3dsx`, `rundriver.txt` with `capsule=25`, `capsule=10`, `capsule=1`) | `capsule=25`: idle (both screens), the knob pulled with A, capsules falling and bouncing on the top screen (three moments), settled with the 25-result list and the outline on item 1, UP / DOWN scrolling, A closing to the title. `capsule=10`: A held for 45 frames (about 0.75 s, so the auto-repeat would have fired) pulled once; B skipped to the results while it was dropping; the screen was still open 100 frames later with the hold released. `capsule=1`: B before the pull went to the title. Screens are not in Git |
**What the host test holds.** The fixture contract (0, 1, 10, 25, 26 items, bad tier, empty label, unterminated label are refused or accepted
as stated; the supplied copy is never written). Timeline for 1, 10 and 25 items: stage order, settle times pinned (1216 ms for one item,
1280 ms for 5, 10, 25) and always before the 1500 ms deadline, a bounce is observed, no capsule below the floor or outside the field, slot i
holds item i, the landing order differs from the fixture order (capsules are not pushed into place). Time: the same fixture stepped at 8,
16.7, 33 and 50 ms frames gives the same state; dt = 0, negative, NaN, infinite and 1e9 do not move or break anything; a hand-damaged
body is still placed by the deadline. Skip at the knob turn, at every stage of the drop and after the settle; skip then repeated skip,
pull, input. The C library's `rand()` state is not advanced by the model's whole timeline, and the test binary links only the two capsule files and the fake drawing layer, so a call to a wallet, profile or unlock function would not link. Screens: every drawn line is on its screen, readable, and free of
Buy, Purchase, Price, voucher, coin, currency, unlock, caught and hatch; capsule rectangles are at most 60 px wide and inside the field;
at most 265 rectangles in the busiest frame (25 capsules). Input: a held button's repeats are ignored (pull once; no skip-then-close);
A / B / START cannot close before 300 ms of settled time; repeated submit pulls once; the callback counts are exactly 1 after a skip, a
natural settle, a skip and a settle; leaving mid-animation calls nothing and the stale screen calls nothing whatever it is given; a
second opening has its own callbacks; reopening from inside a callback is safe.

**Not established.** No Old 3DS device run: the frame cost and the allocation behaviour on hardware are not measured. The host figure is
at most 265 rectangle draws per frame on the top screen with 25 capsules and no allocation in the model; the emulator ran at real speed
but is not a timing measurement. No touch gesture (no framework path). The mutation proof is of the model, screen and debug entry; the
drawing layer is a fake in host tests, so how the strips look is judged only from emulator screenshots (not in Git). The preview is not
reachable in normal play and is connected to no profile, voucher, egg or RNG: the voucher-to-egg pull is C09 over C08.
Status for this task: **COMPLETE** against the C07 card's scope (a debug / test presentation over a fixture result contract).

### 5.7 C08 (profile and reward transactions)

Run on the final tree of the C08 PR (same container as 5.2 to 5.6). The task touches `game/` and this document only; `game-core/` and
`custom-core/` are unchanged (`git diff origin/main -- game-core custom-core` is empty), so the game-core differential and the custom-content
workflow do not run for it.

| Command | Result |
|---|---|
| `make -C game/tests profile_tx_test && game/tests/build/profile_tx_test` (ASan + UBSan, game-core's real `gc_system_gacha_pull`, the real `save.c` on a temporary folder, one fault-injecting layer under system.sav, custom.sav and ptx.sav) | 2,289 checks, 0 failed: pull matrix (4 writes in a clean run: 5 power cuts and 4 failed writes), claim matrix (5 writes: 6 power cuts and 5 failed writes), refusal matrix, 150 fuzz scenarios (464 pulls and 87 claims committed through 160 power cuts and 178 failed writes) |
| `game/tests/build/profile_tx_test 1000` | 12,819 checks, 0 failed (3,123 pulls and 568 claims committed, 1,187 power cuts, 1,216 failed writes) |
| `python3 game/tests/profile_tx_mutations.py` | **36 mutations run, 36 killed, 0 survived, 0 invalid**; 2 more documented as equivalent and not run (a repeated busy check, a last-resort guard in `sysd_write_bytes`). Includes grant-before-commit (a failed commit write leaves the pull in the live profile), a commit write whose failure is ignored, a result revealed before the commit, a retry that would pull again, a scene RNG that is not saved or is re-seeded, a seed or clock that is not the saved one, the roll-forward steps removed one at a time, the gate removed, the boot's recovery cases (including finishing onto an unreadable profile), a request run on a changed profile, an unreadable wallet, a newer file version, a reward of two vouchers or a plus voucher, a wrapping voucher count, a claimed record still owed and a claim write failure ignored |
| `make -C game/tests flowtest && game/tests/build/flowtest` | 114 checks, 0 failed (6 new: a pull committed while the profile write failed is finished by `app_start`, the egg is in the profile, the voucher spent, the result kept) |
| `game/tests/run_host_tests.sh` (the CI script, now including `profile_tx_test`) | all passed |
| `make -C game-core ... egg_gacha_host_test` (game-core's own host test, not changed) | 191 checks passed |
| `docker run --rm -v "$PWD":/src -w /src/game devkitpro/devkitarm make` | builds `pokerogue.3dsx` (2,401,664 bytes, +12 KB over the C07 build), no warning from the new files |
| Azahar (libretro core, the real packs, the Docker-built 3dsx) | The app boots with `ptx_boot()` in `app_start`: the title appears and the cursor moves as before; no `ptx` file is created by a boot with nothing pending. No purchase is reachable from the UI, so nothing else could be checked on the emulator. Screens are not in Git |

**What the matrix holds.** After every power cut and every failed write, and after the boot: the card's profile is exactly the one before or
exactly the one after (never half, never twice); the profile after is on the card exactly when the result is on record; a retry (or the
boot) ends in the clean run's result, eggs, display order, profile and scene RNG; a failed commit write leaves the live profile and the
scene RNG as they were and reveals nothing; for a claim the voucher and the claimed flag are always both or neither, and a second claim is
refused. Also: the pull equals the real `gc_system_gacha_pull` run directly on the same profile (same eggs, same order, same profile bytes);
duplicates and a restart return the same result without a second pull; a refusal (not enough vouchers, capacity, voucher cap, not owed, a
profile that changed) changes nothing but the pending record; the damaged, newer-version and backup-restored `ptx` file; a `ptx` file put back
to its PREPARED write (refused, no second pull); a committed transaction whose card profile was replaced by something else (reported, nothing
overwritten); an unreadable profile (nothing written, the record kept); the gate on `sysd_save`; an unserializable profile; the result ring.

**Not established.** Nothing in the app calls a purchase yet: no screen, no clock or entropy source, no hatching (C09). The power-cut model is
"write n and everything after never happens"; save.c's own windows inside one write (tmp written, rename not done) are the ones listed in
section 1.6 (the old state wins), they are not re-simulated. The upstream differential `egggachadiff.sh` was **not run** here: it needs the
pinned upstream checkout and Node 22, which this container does not have; game-core is byte for byte unchanged, so its owning counts and seeds
are as they were (CI runs it only when game-core changes). No Old 3DS device run: the cost of the extra writes (a pull is 4 writes of a profile
of 3 to 90 KB, a ptx file of the same size while pending) is not measured. Status for this task: **COMPLETE** against the C08 card's scope
(the owner, the envelope, the idempotent pull and reward transactions, their fault tests); no user-visible purchase exists until C09.

### 5.8 C09 (the capsule machine)

Run on the final tree of the C09 PR. The task touches `game/` and this document only; `game-core/` and `custom-core/` are unchanged
(`git diff origin/main -- game-core custom-core` is empty), so the game-core differential and the custom-content workflow do not run for it.

| Command | Result |
|---|---|
| `game/tests/build/gacha_host_test` (ASan + UBSan; game-core's real `gc_system_gacha_pull` behind `profile_tx`, real `save.c` on a temporary folder, fault-injecting layer) | 22,615 checks, 0 failed. Includes the table read from the source against upstream's documented table, 7,800 comparisons of the screen's can-buy / refusal rows with `gc_system_gacha_pull` itself, the capacity edges, refusals writing nothing (files byte-identical), one and many pulls, no second pull for a repeated call, the saved result equal to the source's order, and a restart after every C08 crash point finishing with the same eggs shown once |
| `game/tests/build/gacha_screen_test` (the real screen driven by a fake `ui`: held input, double tap, skip, re-entry, stale input, the fuzz of restarts) | 34,487 checks, 0 failed |
| `game/tests/build/profile_tx_test` | 2,330 checks, 0 failed (new: the shown-mark watermark, envelope version 1 still read) |
| `python3 game/tests/gacha_mutations.py` | **43 run, 43 killed, 0 survived, 0 invalid**, 1 documented as equivalent and not run. Includes two mutants that make a real double pull (a left screen that takes input while a second purchase may start; a callback that buys twice with nothing stopping it), cost checked before capacity, costs, egg counts and capacity wrong against the source, a refusal that begins a transaction, a purchase started while another is unfinished or unshown, an unreadable profile treated as an empty wallet, shown eggs offered again or never recorded, storage order shown instead of the saved display order, a question that opens on Yes, a held button that repeats, and a restart that neither finishes the purchase nor shows the eggs |
| `python3 game/tests/profile_tx_mutations.py` | 47 run, 47 killed, 0 survived, 0 invalid (36 from C08 plus 11 for the watermark) |
| `python3 game/tests/capsule_mutations.py` | 42 run, 42 killed, 0 survived, 0 invalid (the capsule screen still passes its own suite with `live` / `autoplay`) |
| `make -C game/tests flowtest` | 116 checks, 0 failed (the title entry, the machine reachable from the title) |
| `game/tests/run_host_tests.sh` (the CI script) | all passed; `capsule_test` 223,456 checks, `radar_test` 2,618,873 checks, the retained suites unchanged |
| `docker run --rm -v "$PWD":/src -w /src/game devkitpro/devkitarm make` | builds `pokerogue.3dsx` (2,423,860 bytes, +22 KB over the C08 build); the warnings are the ones already on main (`run_screen.c`, `icon.c`, `summary.c`), none from the C09 files |
| Azahar (libretro core, the real packs, the Docker-built 3dsx, `system.sav` written by `game/tests/mksystem.c`: 5 Regular, 3 Plus, 2 Premium, 1 Golden, 7 eggs) | Run A: title (5 entries) -> Capsule Machine -> machines with L/R (wraps) -> option 5 -> question on **No** -> UP -> **Yes** -> A -> fade -> "Here they come..." -> 25 capsules -> a held A has no effect -> eggs list (25 eggs, tier colours, "20 more below") -> A -> menu: golden voucher 1 -> 0, eggs 7 -> 32 / 99, the 25-egg row greyed. Run B: A held for 90 frames on the question stays on No: no purchase, no `ptx.sav` created, `system.sav` byte-identical (after release a single DOWN moves to Yes). Rows that cannot be bought are greyed (10 Regular and 25 Golden in the run-C screens); the refusal sentence itself was not captured on the emulator, the host tests cover its text. Run C: purchase, app stopped at frame 960 mid-animation (`ptx.sav` present, profile already updated) -> the next start opens the machine and shows the 25 eggs once (eggs 32 / 99, voucher still spent once) -> closing goes to the menu -> a third start goes straight to the menu. Screens are in `/mnt/project-files/game-screens/c09/` (not in Git) |

**Found on the emulator.** Nothing needed a code change: the question's default is No and survives a held A, the screen fades through black
between the menu and the capsule screen (not a hang: later frames show the animation), and a restart mid-animation shows the same
eggs once. Egg numbers differ between runs A and C because each is a separate purchase on a separate profile; the identical-result property is
what the host tests prove (saved clock and seed, 22,615 checks), not the screenshots.

**Not established.** The upstream differential `egggachadiff.sh` was **not run** (it needs the pinned upstream checkout and Node 22; game-core is
byte for byte unchanged, CI runs it only when game-core changes). No Old 3DS device run: the option table costs about twelve scratch pulls once
per start and a transient 724 KB scratch profile, a purchase writes the profile and a ptx file of up to 2 MiB; none of it is timed on a device.
The power-cut model is "write n and everything after never happens" (as in 5.7). No touch path (the framework routes none to ui screens). Eggs
are shown by tier and id only: no species, no shiny flag, no hatching, no egg moves screen. The words are plain English of this port, not
upstream strings. The clock is the console's `time(NULL)`; no synchronization is claimed. Status for this task: **COMPLETE** against the C09
card's scope; the title entry is the only way in (no in-run menu exists yet), and the custom mode is not live before C10.

## 6. Not established by this PR

- No upstream behavior was consulted or compared. There is no differential, no seed, no Node 22 run, and none is needed
  for a module that ports no upstream rule.
- No emulator or console check by C01/C02 (no screen or app change then). C03's emulator check, and what it did not cover, is in section 5.2.
- No Old 3DS memory or time measurement. `sizeof` and code-size figures come from compilers (x86-64 host, Ubuntu's
  `arm-none-eabi-gcc`, and devkitARM in CI, with the 3DS CPU flags), not from a device or emulator.
- No profile or wallet code, no live use of the scheduler (C05 is a pure policy: nothing in the app calls it, it has no wire codec and nothing spawns a rematch enemy). (C03 added the live observer hook and the post-mortem screen, section 2.9; C04 added the codec, the transaction and the one custom file, section 2.10.) The app never inserts a Nemesis: the finishing enemy is not identifiable. C06 added the read-only Radar (section 2.12) C07 a debug-only capsule presentation over fixed results (section 2.13) and C08 the profile transaction owner (section 2.14); C09 added the Capsule Machine (section 2.15), the first caller of a purchase, from the title. The rest is C10.
- Range checks only for species, form, move and ability: membership in the game's tables is the adapters' job.
