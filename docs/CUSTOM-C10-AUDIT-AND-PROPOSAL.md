# C10 (live optional Nemesis rematch): audit, blocker and interface proposal

Status: **BLOCKED.** No code is changed by this document. It is the "stop with the exact interface proposal and blocker" the C10 card asks for
(`docs/CUSTOM-CONTENT-ADVANCED-AGENT.md`, C10). It was written at `main` = `ea2aaaf` (C01 to C09 merged). Every `file:line` below was read at that
commit; anything I could not establish is marked **unknown**.

## 1. Why C10 cannot be built now

The card needs three things that do not exist, and one permission I do not have.

| # | Blocker | Evidence | Who can remove it |
|---|---|---|---|
| B1 | **Lane C is not reserved.** The card requires "explicit lane reservation with the current core owner". I have none, and the project rule since 2026-10-12 is that no project thread edits battle-core while an outside agent works there. | `CUSTOM-CONTENT-ADVANCED-AGENT.md`, lane table ("Reserve lane C with its current owner before C10") | lorenzo |
| B2 | **No causal source for a defeat.** `cc_insertion_check` only admits a defeat whose finishing actor is the enemy, so without a reliable finisher **no Nemesis record is ever created in real play**; the roster stays empty and a rematch adapter would have nothing to spawn. The only record of an attacker is `attacks_received` (newest 8 direct hits). It is intact when `FaintPhase` runs (`battle-core/src/phases.c:2736`, "attacksReceived was cleared by nothing yet") and cleared when the fainted Pokemon leaves the field (`pr_leave_field`, `phases.c:2832`; called at `phases.c:2826`). game-core drives a whole turn in one call (`pr_run_turn`, `game-core/src/gc_battle.c:623`), so nothing outside battle-core can look between the faint and the clear. | contracts 1.1, 1.8 items 1, 2, 8; the lines above | an edit in **battle-core** (off limits) |
| B3 | **No hook at encounter generation.** The wild enemy's species and every roll come from run-core (`pr_run_gen_wild_enemy`, `game-core/src/gc_encounter.c:220`, read-only lane), then `gc_mon_new_enemy` (`:222`), the moveset, `pr_run_enemy_pokemon_finish` (`:228`) and `refresh_enemy`. There is no optional callback, and the card forbids hand-assigning a `pr_pokemon` into a live field. | `gc_encounter.c:211-232`, contracts 1.8 item 4 | a small edit in **game-core** (lane C) |
| B4 | **A custom run cannot be saved or resumed yet.** `gc_run_save` is legal only from `GC_EV_SAVE_POINT` to the first turn (`gc_save.c:296`), in format `PR3DSRUN` 2 with fields stored by name hash. The overlay (sticky flag, frozen roster and schedule, run UID, reward ids) and a separate custom slot need an envelope that commits with the run bytes. Nothing in the repo defines it. | `gc_save.c` header, contracts 1.3 and 1.6 | design + game-core/app work in C and E |

B2 is the one that decides the card. Even with a perfect spawn hook, a test roster would be the only way to see a rematch, and the card says not to
call that a live system. So the honest order is: B1 reservation, then B2 in battle-core, then B3 and B4, then C10 proper.

## 2. Proposed contracts (not implemented)

All of them are optional and **disabled by default** (a NULL pointer): with them off the game must be byte-identical to today, which the existing
differentials and `game/tests` already measure.

**P1. Faint observer (battle-core, B2).** One optional callback in `pr_battle`, called from `FaintPhase` at the point of `phases.c:2736`, before the
clear:

```
typedef struct {
  int64_t  fainted_id;    /* Pokemon.id of the one that fainted */
  uint8_t  fainted_side;  /* 0 player, 1 enemy */
  int64_t  source_id;     /* attacks_received[0].source_id when it exists AND the source is on the field; else -1 */
  uint16_t move;          /* attacks_received[0].move, else 0 */
  uint8_t  cause;         /* PR_CAUSE_DIRECT_HIT (attacks_received) or PR_CAUSE_UNKNOWN (indirect, status, weather, recoil, ...) */
} pr_faint_note;
void (*faint_observer)(void *user, const pr_faint_note *n);
```

It reports only what `attacks_received` already holds; anything else is `UNKNOWN`, which the C04 gate already refuses ("unknown cannot create a
Nemesis"). It must not change a phase, an RNG draw or an event. Whether `attacks_received[0]` is the killing hit in every ordering (multi-hit,
Destiny Bond, recoil, a switch-out between) is **unknown**; the proposal is only valid with a test set that compares it to the damage that took the
last HP.

**P2. Encounter hook (game-core, B3).** `gc_run_set_custom(run, const gc_custom_hooks *)`, default NULL, with:
- `classify(run, wave, &kind, &flags)` returning the C05 `cc_encounter` (ordinary wild / trainer / fixed / mystery / boss / final / unsupported; fused, Tera, doubles flags). `gc_new_battle` already knows `battle_type`, `me_battle`, `is_boss`, `pr_run_is_wave_final`.
- `enemy_override(run, wave, pr_wild_enemy *w)`, called in `gen_wild` between `pr_run_gen_wild_enemy` and `gc_mon_new_enemy`, allowed to set species, form, gender, shiny, nature, ability, passive and IVs from the accepted record. The level stays the wave's ordinary level; moves and items follow the game's own paths. **Unknown:** whether replacing the species after the roll keeps RNG alignment for the rest of `pr_run_enemy_pokemon_finish` and `enemy_items`. That needs an experiment before any claim; the card's "RNG boundaries" check covers it.
- the accept/decline question needs a new `gc_screen_kind` value (appended last) and so a `GC_API_VERSION` bump, which touches the save format. That is a core-owner decision.

**P3. Overlay and save (B4).** One envelope that holds the core run bytes and the overlay under a single write-ahead commit, in a custom slot
namespace, with the sticky flag, so an overlay mismatch never silently resumes. C08's `profile_tx` is the model for the commit; the run slot is
not covered by it today.

**P4. Reward.** Already built: C08's `ptx_begin_claim` grants one regular voucher once per record, and C09's machine spends it. C10 only has to call
it on a rematch victory.

## 3. What exists and what is missing

| Piece | State |
|---|---|
| History reducer, post-mortem, store, schedule policy, Radar, capsule presentation, profile transactions, voucher-to-egg machine | merged (C02 to C09), none of it creates or spawns a Nemesis |
| Nemesis creation from a real defeat | missing (B2) |
| Rematch offer, enemy construction, combat, result, reward | missing (B3, B4); only the policy (C05) and the reward owner (C08) exist |

## 4. Decision for lorenzo

1. Reserve lane C and allow **one narrow battle-core edit (P1)** by a named owner, or
2. leave C10 BLOCKED and treat the custom work as shipped without live rematches (history, Radar, capsule machine), which is what the code supports today.

Until one of them is chosen I will not write guessed code for B2 to B4. Claim boundaries stay as the card states: no live custom mode, no core
parity, no hardware acceptance, no final-boss claim.
