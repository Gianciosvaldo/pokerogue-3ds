# Custom content contracts, version 1 (task C01)

Written for task C01 of [CUSTOM-CONTENT-ADVANCED-AGENT.md](CUSTOM-CONTENT-ADVANCED-AGENT.md). It does three things:
audits the interfaces the custom content will touch, fixes the version-1 data contracts, and records what the new
`custom-core/` module proves. It is not a record of a working custom mode: the module has no live hooks, no file
access, no wallet access and no spawn code, and nothing in the app calls it.

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

All functions are pure and in `cc/model.h`.

| Function | Purpose |
|---|---|
| `cc_obs_validate` | range checks on one observation |
| `cc_actor_validate`, `cc_event_validate`, `cc_turn_validate`, `cc_history_validate`, `cc_terminal_validate` | history structures, canonical-unknown rule, ordering, eviction invariant |
| `cc_nemesis_validate`, `cc_nemesis_support`, `cc_insertion_check`, `cc_nemesis_validate_stored`, `cc_roster_validate` | records and the roster |
| `cc_life_can_transition`, `cc_life_overwritable`, `cc_nemesis_transition` | lifecycle |
| `cc_schedule_validate` | the frozen schedule |
| `cc_counters_validate`, `cc_alloc_record_id`, `cc_alloc_txn_id`, `cc_alloc_run_uid` | identifiers |
| `cc_ledger_validate`, `cc_ledger_find`, `cc_ledger_append` | idempotency |
| `cc_meta_init`, `cc_meta_validate` | the whole persisted state, including cross-structure checks (ids below the counters, ledger below `next_txn_id`, an empty report is empty) |
| `cc_size_add`, `cc_size_mul`, `cc_wire_meta_size_for`, `cc_wire_meta_size` | checked size arithmetic and encoded length |
| `cc_status_str`, `cc_refusal_str` | text for statuses and refusals |

Errors are `cc_status`: `CC_E_NULL, RANGE, COUNT, UNKNOWN_BITS, NONCANONICAL, ORDER, DUPLICATE, CONSISTENCY, LIFECYCLE,
OVERFLOW, VERSION`.

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

## 4. What C02 to C10 inherit, and what they still must decide

| Task | Imports | Still undecided / must verify |
|---|---|---|
| C02 reducer | `cc_obs`, `cc_event`, `cc_turn`, `cc_history`, the eviction invariant | which raw events become which `CC_EV_*`; the batch/sequence rejection rule; how `shown` and `hp_delta` are both filled |
| C03 observer + UI | `cc_terminal`, `cc_history`, the "complete only when `flags == 0`" rule | how stable ids are derived (section 1.8, item 1); where in `drain()`/`handle_event` to observe; copy by value in `on_end` |
| C04 store | `cc_meta`, wire widths, `cc_insertion_check`, ledger, `cc_nemesis_transition` | the codec, file name, version-refusal before write, the clear/commit order (section 1.3), the run-UID write |
| C05 scheduler | `cc_schedule`, window waves, `CC_RNG_ALGO_V1`, lifecycle | the algorithm; the draw; the eligibility predicate |
| C06 radar | `cc_nemesis` read-only, `cc_schedule` | touch layer if wanted (section 1.7) |
| C08 profile | `cc_ledger` `REWARD_CLAIM`, the one-envelope rule | the profile owner (section 1.5), the envelope |
| C10 live | all of the above | every item of section 1.8 that is still missing |

## 5. Validation record

*(Filled in below after the checks ran; see the PR description for the exact head.)*

RESULTS_PLACEHOLDER

## 6. Not established by this PR

- No upstream behavior was consulted or compared. There is no differential, no seed, no Node 22 run, and none is needed
  for a module that ports no upstream rule.
- No emulator or console check: there is no screen or app change.
- No Old 3DS memory or time measurement. `sizeof` figures are x86-64 host numbers.
- No codec, no file I/O, no live hook, no scheduler, no reducer, no profile or reward code. Those are C02 to C10.
- Range checks only for species, form, move and ability: membership in the game's tables is the adapters' job.
