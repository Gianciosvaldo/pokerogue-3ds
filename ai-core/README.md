# ai-core: what an enemy does each turn

Native C port of PokeRogue's enemy AI: `EnemyCommandPhase` (a trainer's Pokemon switching out, the move and its targets, instant Tera), `EnemyPokemon.getNextMove` (RANDOM, SMART_RANDOM and SMART), `getNextTargets`, every move attribute's user and target benefit scores, `Pokemon.getMatchupScore`, `isTrapped`, `isMoveSelectable` / `PokemonMove.isUsable`, and the trainer's `getPartyMemberMatchupScores` / `getNextSummonIndex` / `shouldTera`. Like battle-core and run-core it is diff-tested against upstream `pagefaultgames/pokerogue` @ `8555c08`. It reads battle-core's state (`pr_battle`, `pr_pokemon`), RNG, damage path and generated tables **read-only**: nothing in `battle-core/` is changed, and `make` builds battle-core's library into ai-core's own build folder. It compiles with devkitARM for the 3DS (armv6k, hard float) without warnings: about 22 KB of code and 64 KB of tables.

## Using it

`include/pr/ai.h` is the whole API. After the player's command and before `pr_run_turn`, the game calls

```c
pr_ai_enemy_command(bt, &ai, field_index);   /* fills bt->commands[2 + i] (and bt->pre_commands[2 + i] for Tera) */
```

once per enemy on the field, as upstream runs one `EnemyCommandPhase` per enemy. `pr_ai` holds what the AI reads that `pr_pokemon` doesn't (yet): per Pokemon its `aiType`, `waveData.abilityRevealed`, `initialTeamIndex` and `trainerSlot`; the trainer (`isBoss`, double, Tera mode and instant-Tera slots, NULL in a wild battle); the session seed (`globalScene.seed`, for the tie roll between equally good switch-ins); `currentBattle.enemySwitchCounter`, which the call raises on a switch and lowers otherwise, as upstream; and a mystery encounter's skip flag. Set `bt->engine` to the game's engine between turns: a few move conditions the scores call read the phase queue (Protect and co. count as failing unless a MovePhase is queued, which it never is at command time) or add to it as upstream does at that point (Damp showing itself against Explosion, an immunity message).

`pr_ai_next_summon_index(bt, &ai, TRAINER_SLOT_TRAINER)` is `Trainer.getNextSummonIndex`: the enemy party slot a trainer sends in after a faint or a forced switch. battle-core stops at `PH_SWITCH` for the enemy side and the run loop passes this slot to `pr_resume_switch`.

`ai.move_scores` / `ai.n_move_scores` hold the last scored choice's move scores (upstream's `Move Scores:` log) and `pr_ai_party_matchup_scores` gives the trainer's switch-in scores, for a debug overlay.

Upstream keeps a little session-wide state on move attribute objects that the scores read: the status Tri Attack or Dire Claw rolled last (`MultiStatusEffectAttr.effect`) and the ratio Moonlight, Synthesis, Shore Up or Swallow healed last (`VariableHealAttr.healRatio`). battle-core doesn't record them yet, so the game sets them with `pr_ai_session_set_multi_status` / `pr_ai_session_set_heal_ratio` when those moves are used (or leaves upstream's initial values). Sleep Talk's and Assist's remembered pick is battle-core's own (`pr_called_selected_*`).

## How it is checked

`tests/ai.producer.test.ts` runs inside an upstream checkout (`tools/difftest.sh <upstream> 5000 <seed>`). Each case builds a random singles state in a real upstream battle: wild or trainer battle (a third of the trainers bosses, instant Tera now and then, 0 to 5 Pokemon on the enemy bench, some fainted, two of them identical a quarter of the time so their scores tie), random species, levels, HP (a quarter at a fifth or less), statuses, stat stages, movesets (half attacks, some without PP), abilities (a third from a list of ones the AI path reads: Arena Trap, Shadow Tag, Magnet Pull, Levitate, Wonder Guard, Sturdy, Volt Absorb, Prankster, Illusion...), the AI type (mostly the constructor's), each side's ability revealed half the time, move history, restricting tags on the enemy (Encore, Disable, Taunt, Torment, Throat Chop, Heal Block, Gorilla Tactics, Imprison, Mean Look's trap, Laser Focus, a rampage), Substitutes, a queued move one case in ten, entry hazards (layers included), screens, Tailwind, Gravity, Fairy Lock, weathers (the primal ones too), terrains, the switch counter, and the session state above set at random. It runs upstream's own `EnemyCommandPhase(0).start()` and records the enemy's command and pre-turn command, the switch counter, the global and battle RNG states, the enemy's move queue, the move scores it logged, Sleep Talk's and Assist's remembered pick, the phases left queued, the abilities applied and revealed, and in a trainer battle the switch-in scores (`getPartyMemberMatchupScores(TRAINER, true)`, hazards included) and the slot `getNextSummonIndex` picks. `tests/aitest.c` reads the same state (battle-core's turntest records plus `AIMON`, `TRAINER`, `AI`, `SESS`), runs `pr_ai_enemy_command`, `pr_ai_party_matchup_scores` and `pr_ai_next_summon_index`, and compares every record, the scores as doubles. One move pick in ten comes from a short list of moves whose scores read rare states (Growth in sun, Gust or Earthquake against a Pokemon mid-Fly or Dig, Snore, Fake Out, Revival Blessing...), and the player is mid-Fly, Dig, Dive or Shadow Force one case in sixteen.

| Seed | Cases | Covered | Identical | Mismatches | Not covered |
|---|---|---|---|---|---|
| 777 | 5,000 | 4,923 | 4,923 | 0 | 77 |
| 9001 | 5,000 | 4,936 | 4,936 | 0 | 64 |
| 4242 | 5,000 | 4,933 | 4,933 | 0 | 67 |
| 31337 | 5,000 | 4,929 | 4,929 | 0 | 71 |

Over the four seeds the covered cases hold 951 switches, 18,770 moves (2,003 taken from the move queue, 10,669 chosen by score), 493 instant Teras and 7,669 trainer faint picks. Not covered (279): 131 read a Pokemon with Illusion (below), 108 hit a move condition battle-core hasn't ported (mostly Revival Blessing, then Transform and Sketch), 42 a trainer's switching move (below).

To check the test itself, 78 one-line bugs were planted in ai-core one at a time (a changed constant, a flipped comparison, a dropped condition, a skipped branch) and each build was run over the four seeds: 65 made covered cases fail. The 13 that didn't:
- Paths singles can't reach: the second target's cutoff and weight in `getNextTargets`, an ally's effectiveness in the scores, `ignoreAllyAbility` in the KO check.
- Changes upstream can't show either: Toxic Spikes' 1.25 for a Poison type (a Poison type can't be poisoned, so upstream returns 1 first), Fake Out's -20 after the first turn (a move whose condition fails scores -20 anyway), Revival Blessing's trainer check (a wild enemy has no fainted party member).
- Situations the test leaves out (below): Baton Pass in a trainer battle, Illusion (dropping its flag moves 28 cases into coverage and they still match, so the flag is only cautious).
- Rare states the random cases reached without the bug changing a result: Growth in sun (the few cases had no physical attack, which upstream's quirk requires for both stats), HP at exactly half for Super Fang's score, Sturdy on an unrevealed full-HP player facing a KO, a NaN score.

## What isn't covered yet

- **Doubles.** The code follows upstream's doubles paths (two opponents, allies' scores, Commander) but the test is singles only, as battle-core's turn engine is; battle-core's damage path reports doubles as unported.
- **Illusion.** `getMatchupScore` and the scores read the disguised Pokemon's types with `useIllusion`; Illusion isn't modelled, so a read of a Pokemon with Illusion reports `PR_AI_FEAT_ILLUSION`.
- **Held items.** The test has no modifiers (battle-core's records skip them), so what the scores read from items is untested: Thief, Knock Off and co. (`StealHeldItemChanceAttr`, `RemoveHeldItemAttr`), the KO check's item damage boosts, and Stuff Cheeks, which needs a berry: battle-core doesn't tell berries from other held items, so any held item of an unknown class reports `PR_AI_FEAT_RESTRICTION`.
- **A trainer's switching moves.** battle-core's `pr_switch_out_condition` reports `PR_FEAT_TRAINER` for the trainer's side (it doesn't know trainer slots), so U-turn, Baton Pass, Parting Shot and Roar in a trainer battle are uncovered.
- **Boss enemies** (boss segments are unported in battle-core) and **Transform** (`summonData.moveset`) aren't in the test.
- **Messages at command time.** What a condition queues on `bt->engine` (above) is what upstream queues, but `pr_run_turn` starts the engine afresh, so the game doesn't show them yet.

### Stuff Cheeks restriction

The move selection restriction recognizes `PR_MOD_BERRY` anywhere on the user's side,
including a teammate's berry. Upstream's restriction does not filter the modifier's
owner, berry type or stack count. Unknown held-item subclasses still report
`PR_AI_FEAT_RESTRICTION` when no known berry resolves the check.

`make -C ai-core test-berry` runs focused host checks derived from upstream
`8555c08`, `src/data/moves/move.ts` (Stuff Cheeks) and `BattleScene.findModifiers`.
For a focused differential, copy the additive `tests/berry.producer.test.ts` into
the prepared upstream checkout's `test/tests/zz-dump/`, then run:

```sh
cd <upstream>
BERRY_OUT=/tmp/ai-berry.txt npx vitest run test/tests/zz-dump/berry.producer.test.ts
cd <port>
make -C ai-core build/berrytest
ai-core/build/berrytest /tmp/ai-berry.txt
```

The producer invokes upstream's actual `checkRestrictions` on both sides for all
11 berry types, three owners (user, teammate, unowned), zero/nonzero stack counts,
opposite-side berries, no modifier and a known non-berry held item. This focused
check covers selection availability only; it does not validate berry consumption,
held-item scores, doubles or a full enemy command with modifiers.

Checked on 2026-10-03: the focused host test printed **404 cases, 404 identical,
0 mismatches** (including unknown-subclass handling); the upstream differential
printed **268 cases, 268 identical, 0 mismatches**. Replaying those same 268
upstream cases with the original AI code printed **132 mismatches**.

The existing 5,000-case AI differential was also rerun without modifying its
producer or comparator. It is **not clean in this environment**:

| Seed | Covered | Identical | Mismatches | Not covered |
|---|---|---|---|---|
| 777 | 4,938 | 4,936 | 2 | 62 |
| 9001 | 4,946 | 4,937 | 9 | 54 |
| 4242 | 4,942 | 4,933 | 9 | 58 |
| 31337 | 4,936 | 4,924 | 12 | 64 |

The original AI code replayed the same records with identical failure output
and counts on all four seeds. Printed first differences concern trainer matchup
score floating-point values. This bounded restriction fix does not resolve those
pre-existing failures; full AI differential validation remains blocked.

## What ai-core carries that belongs in battle-core

These are local copies or shims, kept in step with battle-core by hand until it exports them:

- `src/ai_damage.c`: `getMoveEffectiveness` and `getAttackDamage` with upstream's `ignoreAbility` / `ignoreAllyAbility` branches (battle-core's versions take no flags because no battle path passes them).
- `isMoveSelectable`: the restricting tags (Torment and Imprison included) and the moves' own restrictions (Gravity, Belch, Stuff Cheeks, consecutive use). The player's fight menu needs the same check.
- `isTrapped`: `CheckTrappedAbAttr` with Arena Trap's, Shadow Tag's and Magnet Pull's conditions (matched to upstream's source by `pr_closure_matches`).
- `waveData.abilityRevealed` lives in `pr_ai_mon`; battle-core should set it where an ability is shown.
- The session state on Tri Attack's and Moonlight's attributes, which battle-core should record when those attributes apply.

## Files

- `include/pr/ai.h`: the API. `include/pr/ai_data.h` and `gen/out/ai_data.c`: what each move attribute's scores read (owner classes, levels, stats, tag scores, restrictions...), dumped from upstream's live objects by `gen/dump-ai.test.ts` (`tools/redump.sh <upstream>`). `pr_ai_tables_match()` checks the dump against battle-core's move tables; the scores report `PR_AI_FEAT_TABLES` if they drift apart.
- `src/ai.c`: `EnemyCommandPhase`, `getNextMove`, `getNextTargets`, matchup and switch-in scores, trapping, move selection rules. `src/ai_score.c`: the benefit scores. `src/ai_damage.c`: the flagged damage path.
- `tests/ai.producer.test.ts`, `tests/aitest.c`, `tools/difftest.sh`: the diff test.

