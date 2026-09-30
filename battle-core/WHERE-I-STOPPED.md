# Where the battle logic slice stopped (2026-09-30, paused by lorenzo)

Branch `claude/project-thread-rpdgp5`. Not merged yet; no PR opened because the re-check below is not all green.

## On this branch, beyond main (PR #30 already landed pr_continue, trainer switches, `pr_event_field.visible`)
- `PR_ARG_TEMPLATE` (text.h/text.c): titled trainer names render ("Gym Leader Brock would like to battle!").
- `pr_species.starter` comment fixed (root species id, not starter cost).
- Command screen checks (src/commands.c, declared in engine.h): `pr_fight_move`, `pr_move_usable`,
  `pr_move_selectable`, `pr_is_trapped`, `pr_can_switch`, `pr_can_run`, each with upstream's refusal text.
- Battle start (work in progress, tests pass): `pr_start_battle` (EncounterPhase.end: SummonPhase, sparkles,
  ToggleDoublePosition, CheckSwitchPhase, InitEncounterPhase/PostSummon) and `pr_answer_check_switch`
  (stop PH_CHECK_SWITCH, "Will you switch?"). `bt->battle_style_set` = the Set battle style option.
- Test modes: TURN_CPROBE (command checks) and TURN_START (battle start) in turntest.producer + turntest.c.

## Upstream re-check at the stop
- CPROBE, seed 777: 2,125 of 2,125 checks match, texts included.
- START, seed 777, N=300: wild 276/276, trainer 291/291, texts identical. Removing the enemy sparkle is caught.
- Full regenerated set (84 files, 4 seeds): 80 files 0 mismatches. The 4 `trainer_*` files are NOT clean:
  777 1 fail, 4242 1, 31337 1, 9001 32 fails (28 text diffs).
  Pattern seen: the enemy uses Roar/Whirlwind in a trainer battle and upstream prints "But it failed!"
  (e.g. seed 9001 cases 946, 959, 964); native differs there. Not yet diagnosed. This may already be on main
  (trainer switches landed with PR #30), so check main against trainer_9001 first.
- Not run yet: CPROBE and START on seeds 9001/4242/31337.

## Next steps
1. Diagnose the trainer Roar/Whirlwind failure (FAIL CASE lines in `build/turntest trainer_9001.txt`).
2. Run CPROBE (N=2000) and START (with and without TURN_TRAINER) on all 4 seeds; all must be 0 mismatches.
3. README update for the new API and test modes, then open the PR.
