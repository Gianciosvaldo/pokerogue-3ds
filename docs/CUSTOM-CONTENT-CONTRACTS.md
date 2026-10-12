# Custom content contracts, version 1 (tasks C01, C02, C03)

Written for task C01 of [CUSTOM-CONTENT-ADVANCED-AGENT.md](CUSTOM-CONTENT-ADVANCED-AGENT.md). It does three things:
audits the interfaces the custom content will touch, fixes the version-1 data contracts, and records what the new
`custom-core/` module proves. It is not a record of a working custom mode: the module has no live hooks, no file
access, no wallet access and no spawn code. Since C03 the app feeds the reducer and shows a read-only post-mortem screen
(sections 2.9 and 5.2); nothing is saved and no custom mode exists yet.

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

## 4. What C02 to C10 inherit, and what they still must decide

| Task | Imports | Still undecided / must verify |
|---|---|---|
| C02 reducer (decided in section 2.8) | `cc_obs`, `cc_event`, `cc_turn`, `cc_history`, the eviction invariant | nothing left open: the raw-event mapping, the batch and sequence rules and how `shown` and `hp_delta` are filled are in section 2.8 |
| C03 observer + UI (decided in section 2.9) | `cc_reducer`, `cc_reduce`, `cc_turn_summarize`, `cc_terminal`, `cc_history`, the "complete only when `flags == 0`" rule | nothing left open for C03, except that stable ids and the finishing source are not derivable from the events (section 2.9); C04 must decide the durable run uid |
| C04 store | `cc_meta`, wire widths, `cc_insertion_check`, ledger, `cc_nemesis_transition` | the codec, file name, version-refusal before write, the clear/commit order (section 1.3), the run-UID write |
| C05 scheduler | `cc_schedule`, window waves, `CC_RNG_ALGO_V1`, lifecycle | the algorithm; the draw; the eligibility predicate |
| C06 radar | `cc_nemesis` read-only, `cc_schedule` | touch layer if wanted (section 1.7) |
| C08 profile | `cc_ledger` `REWARD_CLAIM`, the one-envelope rule | the profile owner (section 1.5), the envelope |
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
| `build/postmortem_test` | 248 checks, 0 failed (includes a check that every line is drawn in a style that was readable in Azahar) |
| `build/flowtest` | 71 checks, 0 failed (64 on `main`) |
| `build/saves_test`, `startertest`, `starter_data_test`, `starter_ui_test`, `bgmtest` | 68 / 737,655 / 51,280 / 4,470 / 11,421 checks, 0 failed |
| `python3 game/tests/observer_mutations.py` | 29 run, 29 killed, 0 survived, 0 invalid |
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
opened from the result screen showed the real events of three turns, and LEFT / RIGHT paged them. **The defeat itself was forced:** a
Magikarp could not lose to the wave 1 Sentret in 9,000 emulated frames, so the screenshots come from a throwaway copy of `game/`
(outside the repo, not committed) that ends the run after five observed battle ranges. The turns, damage and HP shown are real
observations; a real faint, a real "Last to fall" line, a scroll past ten rows and a clear were not seen in the emulator (the host
tests cover them with a fake font). The first emulator run found that the totals, rows, notice and footer, drawn in the 9 px emerald
style, were unreadable; they now use `TS_INSTRUCTIONS_TEXT` (the 8 px pixel font the party screen uses), and the same flow was
re-run on the merged tree to confirm. Screenshots are in the shared project files, not in Git.

**Not run / not established.** No timing on an Old 3DS or a real console. The new game tests are not part of the CI workflow (it
runs game-core tests and the 3DS build). The finishing source of a defeat is never known. Stable Pokemon identity and a durable run
uid are not provided.

## 6. Not established by this PR

- No upstream behavior was consulted or compared. There is no differential, no seed, no Node 22 run, and none is needed
  for a module that ports no upstream rule.
- No emulator or console check by C01/C02 (no screen or app change then). C03's emulator check, and what it did not cover, is in section 5.2.
- No Old 3DS memory or time measurement. `sizeof` and code-size figures come from compilers (x86-64 host, Ubuntu's
  `arm-none-eabi-gcc`, and devkitARM in CI, with the 3DS CPU flags), not from a device or emulator.
- No codec, no file I/O, no scheduler and no profile or reward code. (C03 added the live observer hook and the post-mortem screen; section 2.9.) The rest is C04 to C10.
- Range checks only for species, form, move and ability: membership in the game's tables is the adapters' job.
