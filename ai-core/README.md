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

- **Doubles** beyond what the doubles suite (Task 74, below) builds: a disguise in doubles, held items, bosses, mystery encounters, and a Pokemon commanding that is also disguised or Transformed.
- **Illusion, the ability.** Pinned upstream registers no Illusion ability attrs, so the ability sets no disguise in a real battle and ai-core doesn't model it turning on: a useIllusion read of a Pokemon that has the ability but no loaded disguise still reports `PR_AI_FEAT_ILLUSION` and the case is not covered (131 cases over the four seeds, unchanged). A disguise loaded by hand (`pr_set_illusion*` in battle-core, Task 67's helpers) is modelled, see Task 68 below.
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

- `src/ai_damage.c`: `getMoveEffectiveness` and `getAttackDamage` with upstream's `ignoreAbility` / `ignoreAllyAbility` branches (battle-core's versions take no flags because no battle path passes them), and `getAttackTypeEffectiveness` with `useIllusion` (`ai_attack_type_effectiveness`: battle-core's `pr_get_attack_type_effectiveness_opts` has no such flag, so with a disguise loaded the same steps run here on the disguise's types; without one it calls battle-core's own).
- `isMoveSelectable`: the restricting tags (Torment and Imprison included) and the moves' own restrictions (Gravity, Belch, Stuff Cheeks, consecutive use). The player's fight menu needs the same check.
- `isTrapped`: `CheckTrappedAbAttr` with Arena Trap's, Shadow Tag's and Magnet Pull's conditions (matched to upstream's source by `pr_closure_matches`).
- `waveData.abilityRevealed` lives in `pr_ai_mon`; battle-core should set it where an ability is shown.
- The session state on Tri Attack's and Moonlight's attributes, which battle-core should record when those attributes apply.

## Files

- `include/pr/ai.h`: the API. `include/pr/ai_data.h` and `gen/out/ai_data.c`: what each move attribute's scores read (owner classes, levels, stats, tag scores, restrictions...), dumped from upstream's live objects by `gen/dump-ai.test.ts` (`tools/redump.sh <upstream>`). `pr_ai_tables_match()` checks the dump against battle-core's move tables; the scores report `PR_AI_FEAT_TABLES` if they drift apart.
- `src/ai.c`: `EnemyCommandPhase`, `getNextMove`, `getNextTargets`, matchup and switch-in scores, trapping, move selection rules. `src/ai_score.c`: the benefit scores. `src/ai_damage.c`: the flagged damage path.
- `tests/ai.producer.test.ts`, `tests/aitest.c`, `tools/difftest.sh`: the diff test.
- `tests/illusion.producer.test.ts`, `tests/illusiontest.c`, `tools/illusiondiff.sh`: the same test with disguises loaded by hand (Task 68, below). Both are copies of the established pair plus marked lines; the established files are unchanged.
- `tests/doubles.producer.test.ts`, `tests/doublestest.c`, `tools/doublesdiff.sh`: the doubles suite (Task 74, below). The producer is a doubles copy of the established one; the replay includes `tests/aitest.c` unchanged and reuses its reader.


## Task 88: runtime-dependent matchup scores

Checked main `ca1401075ccc80c4111fa12333d6f66e1164efd3` on 2026-10-03.
The existing C AI is clean under HANDOFF's specified Node 22 reference runtime:

| Seed | Generated | Covered / identical | Mismatches | Not covered |
|---|---|---|---|---|
| 777 | 5000 | 4938 / 4938 | 0 | 62 |
| 9001 | 5000 | 4946 / 4946 | 0 | 54 |
| 4242 | 5000 | 4942 / 4942 | 0 | 58 |
| 31337 | 5000 | 4936 / 4936 | 0 | 64 |

Total: 20,000 generated, 19,762 covered/identical, zero mismatches, 238 not
covered. Stats mismatches and skipped records are zero. Coverage did not decline
from the previous Node 24 runs. The uncovered Illusion/move-condition reasons
remain; this does not establish complete AI parity or unlock Illusion integration
before Task 67. Existing C, producers/comparators, tolerances, seeds, counts and
coverage classifications are unchanged.

Runtime: Node **22.23.3**, GCC **13.3.0**, upstream
`8555c08c823b856cbec4eb99ca84ea52a955836d`. Node **24.19.0** remains a different
reference: its previously recorded 2/9/9/12 failures are real, not waived.
Stealth Rock uses `Math.pow(damageHpRatio, damageHpRatio)` in
`src/data/arena-tag.ts`. Directly measured values:

| Input x (`Math.pow(x,x)`) | Node 22 / port js_pow | Node 24 |
|---|---|---|
| 0.0625 | 0.84089641525371461 | 0.84089641525371450 |
| 0.125 | 0.77110541270397048 | 0.77110541270397037 |
| 0.25 | 0.70710678118654746 | 0.70710678118654757 |

The port's existing `battle-core/src/js_math.c` deliberately reproduces Node
22/V8's power implementation. Node 24's differing final bits propagate through
hazard matchup multipliers. All 12 failing cached seed-31337 cases contain an
enemy-side Stealth Rock tag; all four required suites pass with Node 22.
Switching to host libc pow would silently change the existing target and is not
portable evidence for the 3DS; no such substitution was made.

**Reference-runtime conflict remains:** HANDOFF says Node 22, but the pinned
upstream's own `package.json` declares Node `>=24.9.0`. Node 22 passing establishes
the named HANDOFF runtime's comparisons, not validation under upstream's declared
Node 24 environment or every JavaScript engine. Choosing/migrating a reference
runtime needs an explicit reviewed contract. Task 88 diagnoses the recorded
failures; it does not erase the Node 24 gap.

Exact producer/replay commands used (upstream checkout outside the repo):

```sh
# Seed 777, from the port; script copies the unchanged producer and invokes it:
PATH=/root/.npm/_npx/52027bd8fc0022aa/node_modules/node/bin:$PATH BUILD=/tmp/ai88-node22-777 sh ai-core/tools/difftest.sh /tmp/pokerogue-upstream 5000 777
# Each of 9001, 4242, 31337, from /tmp/pokerogue-upstream (substitute seed):
PATH=/root/.npm/_npx/52027bd8fc0022aa/node_modules/node/bin:$PATH AI_SEED=<seed> AI_N=5000 AI_OUT=/tmp/ai88-node22-<seed>.txt /root/.npm/_npx/52027bd8fc0022aa/node_modules/node/bin/node node_modules/vitest/vitest.mjs run test/tests/zz-dump/ai.producer.test.ts
/tmp/ai88-node22-777/aitest /tmp/ai88-node22-<seed>.txt
# Focused berry regressions, from the port:
make -s -C ai-core BATTLE_CORE=../battle-core test-berry
ai-core/build/berrytest /tmp/ai-berry-cases.txt
```

Each upstream producer reports one test passed. Fresh-built comparator replays
all four files with the counts above. Berry host: 404/404, zero mismatches; replay
of the prior recorded upstream berry trace: 268/268, zero mismatches (not a newly
generated berry differential here). No new hardware/emulator or 3DS result.

## Task 68: loaded Illusion

The AI reads a Pokemon's apparent typing in two places: `getMatchupScore` (`opponent.getTypes({useIllusion: true})`,
the second-type defence read with `useIllusion`, and the attack-score read on the opponent with `useIllusion`) and
the move scores (`target.getMoveEffectiveness(..., useIllusion = true)` in `getNextMove`). ai-core now reads the
disguise there when `pr_pokemon.illusion.active` is set (battle-core's Task 67 helpers: `pr_get_types` with
`PR_TYPES_USE_ILLUSION`; the effectiveness steps are the copy in `src/ai_damage.c`, see above). Everything else
upstream reads without `useIllusion` still reads the real Pokemon: the first defence read, STAB (`isOfType`), Ground
immunity, the strong-winds check and Flying Press's inner read (upstream's own TODO in `move.ts`).

What this does not do: it never turns a disguise on. Natural Illusion (the ability, its summon and break dispatch)
stays outside, as in upstream at the pin. A Pokemon with the ability and no loaded disguise reports
`PR_AI_FEAT_ILLUSION` exactly as before.

### How it is checked

`tests/illusion.producer.test.ts` is `tests/ai.producer.test.ts` plus marked "LOADED ILLUSION" lines. After each
case's state is built (the same random wild and trainer battles), a third of the two Pokemon on the field and a
quarter of every bench Pokemon get a real `summonData.illusion` from upstream's own `Pokemon.setIllusion`: another
Pokemon with a random species, form, fusion and gender, or only a species; a quarter of those also get a summon
typing and some an added type, which a disguise hides from a `useIllusion` read, and one in six a Foresight, Miracle
Eye or Smack Down tag. One in ten of the two Pokemon on the field gets Scrappy or Mind's Eye, and one case in twelve gives
the enemy a Stellar Tera Blast. Only the presentation calls
`setIllusion` makes (assets, the info box, the animation, the shiny sparkle) are stubbed. The case then runs upstream's
`EnemyCommandPhase` as the established producer does, and each disguised Pokemon gets an `ILL` record.
`tests/illusiontest.c` is `tests/aitest.c` plus the marked lines that read `ILL` (the established comparator stops at
an unknown record, so the two cannot be mixed up). `--mutate-identity` replaces every disguise with its Pokemon's real
identity before the replay, which a test that ignored disguises would not notice.

```sh
sh battle-core/tools/setup-upstream.sh <upstream>          # Node 22; once
sh ai-core/tools/illusiondiff.sh <upstream> 5000 <seed>    # seeds 777 9001 4242 31337
```

The script refuses a Node that is not 22.x (the Node 24 conflict above is unresolved), replays the cases, then replays
them with the identity planted and fails unless that run fails.

Checked 2026-10-04 on main `ff35abf`, Node v22.22.0, GCC 13.3.0, vitest 4.1.11, upstream `8555c08`, 5,000 cases per seed:

| Seed | Disguised Pokemon (player / enemy / bench) | Covered / identical | Mismatches | Not covered | Passing cases with a disguise | Of those, field matchup score moved by it | Identity planted: failing cases |
|---|---|---|---|---|---|---|---|
| 777 | 6,824 (1,732 / 1,654 / 3,438) | 4,945 / 4,945 | 0 | 55 | 3,823 | 2,259 | 1,138 of 4,949 |
| 9001 | 6,679 (1,662 / 1,569 / 3,448) | 4,959 / 4,959 | 0 | 41 | 3,774 | 2,141 | 1,116 of 4,961 |
| 4242 | 6,765 (1,709 / 1,628 / 3,428) | 4,947 / 4,947 | 0 | 53 | 3,809 | 2,163 | 1,127 of 4,947 |
| 31337 | 6,860 (1,722 / 1,669 / 3,469) | 4,959 / 4,959 | 0 | 41 | 3,809 | 2,269 | 1,117 of 4,958 |

Over the four seeds: 20,000 cases, 27,128 disguises (13,472 by species, 6,679 with a fusion, 6,724 with a summon
typing), 19,810 covered cases, all identical. The 190 not-covered cases: 88 read a Pokemon that has the Illusion
ability and no loaded disguise (the unchanged exclusion) and 103 hit a move condition battle-core hasn't ported (one
case has both). Upstream's own scores moved with the disguise in 8,921 cases for `getMatchupScore` and 4,289 for the
move effectiveness the AI scores with (counted in the producer, on a fresh state). With the identity planted,
4,498 covered cases fail.

Planted faults in the C (one at a time, 5,000 cases on each seed, failing cases per seed in the order above):

| Planted fault | Failing cases |
|---|---|
| matchup: opponent's types read without the disguise | 491 / 510 / 476 / 502 |
| matchup: own second-type read without the disguise | 314 / 313 / 293 / 325 |
| matchup: attack-score read without the disguise | 501 / 515 / 484 / 511 |
| matchup: first defence read with the disguise (upstream has none) | 603 / 616 / 591 / 632 |
| matchup: second-type read simulated (upstream passes `simulated: false`) | 3 / 0 / 3 / 2 |
| move scores: effectiveness without the disguise | 604 / 579 / 592 / 594 |
| copy: types without the disguise | 1,105 / 1,092 / 1,092 / 1,105 |
| copy: strong-winds halving dropped | 1 / 1 / 1 / 0 |
| copy: type-chart overrides (Freeze-Dry...) dropped | 4 / 5 / 3 / 3 |
| copy: forcibly-ungrounded Ground immunity dropped | 0 / 2 / 1 / 0 |
| copy: grounded Flying type loses its Ground immunity, dropped | 6 / 3 / 0 / 3 |
| copy: ignore-type-immunity abilities (Scrappy, Mind's Eye) dropped | 5 / 3 / 4 / 4 |
| copy: exposed-tag (Foresight, Miracle Eye) immunity bypass dropped | 3 / 2 / 2 / 3 |
| copy: Stellar-move early return dropped | 5 / 11 / 12 / 9 |
| move scores: disguise also used for the powder / Prankster immunity block | 0 / 0 / 0 / 0 |

Every fault is caught on at least one seed except the last, and that one cannot change a result: the AI's move scores
pass `useIllusion` only for attack moves, and the powder and Prankster conditions concern status moves. Several of the
copy's branches are caught by a handful of cases in 20,000 (strong winds, 3; Ground immunity, 3), because the random
states reach them rarely; the producer adds Foresight, Odor Sleuth, Miracle Eye and Smack Down's tags to disguised Pokemon,
Scrappy or Mind's Eye to the Pokemon on the field and a Stellar Tera Blast to the enemy (in 1 case in 12) so the rest are reached
at all. They stay as thinly tested as that.

Not covered by this suite: a disguise on a Pokemon in doubles, natural Illusion, nickname and fused display names,
and anything about the disguise's gender beyond carrying it. The disguise is always loaded by hand.

### Established checks, before and after

The established AI differential (`tools/difftest.sh <upstream> 5000 <seed>`) was run on unchanged main and again with
this change, same upstream checkout, Node v22.22.0. The printed summaries are byte-identical on all four seeds:

| Seed | Covered | Identical | Mismatches | Not covered | Of those, AI: Illusion |
|---|---|---|---|---|---|
| 777 | 4,938 | 4,938 | 0 | 62 | 34 |
| 9001 | 4,946 | 4,946 | 0 | 54 | 25 |
| 4242 | 4,942 | 4,942 | 0 | 58 | 34 |
| 31337 | 4,936 | 4,936 | 0 | 64 | 38 |

`make -C ai-core test-berry` still prints 404 cases, 404 identical; the upstream berry differential 268 cases, 268
identical. `make -C game-core test` exits 0 with the same paired 200-run digests before and after (`8ee22b13`).

## Task 74: doubles command selection

In a double battle the AI scores and aims moves against two opponents and an ally: `getNextMove`'s scores flip the
sign for the ally and divide the ally's effectiveness and STAB out, `getNextTargets` weighs both sides, shifts the
weights by the lowest, cuts at half the best and draws, Commander skips a commanded Pokemon's turn and makes its
Tatsugiri a -20 target, and a double trainer's switch-in scores go per trainer slot. ai-core already followed those
paths; this card checks them against upstream's own commands and fixes what the check found.

### What changed in the C

`src/ai_damage.c`, the KO check's damage path, on double battles only (every change sits behind
`bt->double_battle`, so singles take the same path as before):

- the spread multiplier: 0.75 when the move hits more than one target (`getMoveTargets`, `multiple`), as upstream's
  `getAttackDamage` does;
- `FieldPriorityMoveImmunityAbAttr` (Queenly Majesty, Dazzling, Armor Tail) over the defender's whole side in speed
  order (`getAlliesGenerator`), not only the defender;
- Friend Guard: the defender's ally's `AlliedFieldDamageReductionAbAttr` when that ally is active
  (`currentBattle.double && ally?.isActive(true)`), inside the `!ignoreAbility` block.

Before, all three reported doubles as unported, so no doubles case could be compared. The spread check calls
`pr_get_move_targets`, which draws a random-near-enemy move's target from the battle RNG as upstream's
`getMoveTargets` does; the RNG records match.

Nothing in battle-core changed. One thing a caller must know: battle-core's switch-out condition (U-turn, Baton
Pass, Parting Shot, Roar...) reads `pr_pokemon.trainer_slot`, the AI reads `pr_ai_mon.trainer_slot`, so a game that
builds a double trainer's party sets both. The suite's reader does (`tests/doublestest.c`); without it two cases in
8,000 mismatched (a Parting Shot score, a Baton Pass pick).

An upstream quirk the C keeps: in the KO check `ignoreAllyAbility` is set from whether the defender's ally's ability
is revealed, but `getAttackDamage` passes it to the attacker's Attack read, so it decides whether the attacker's
ally's Flower Gift counts. The defender's ally's abilities go through `ignoreSourceAllyAbility`, which the KO check
leaves false.

### How it is checked

`tests/doubles.producer.test.ts` is the established producer made doubles (marked "DOUBLES"): `battleStyle("double")`,
two Pokemon a side, a third of the abilities drawn half from the AI list and half from abilities that act on an
ally or a side (Friend Guard, Flower Gift, Plus, Minus, Telepathy, Trace, Queenly Majesty, Lightning Rod, Commander,
Receiver...), move picks weighted to spread moves, ally-targeting moves (Helping Hand, Pollen Puff, Heal Pulse,
Decorate, Instruct, Beat Up...), random-near-enemy moves, and Flame Burst / Expanding Force / Tera Starstorm.
Three quarters of the trainers are double trainers, each enemy Pokemon in the trainer slot upstream gives it. Each
case can also carry:

- a Tatsugiri commanding from inside its ally (one case in ten, either side; the ally is Dondozo three times in four
  and gets upstream's `CommandedTag`);
- a Pokemon fainted in a field slot, as `FaintPhase` leaves it (one in ten);
- a KO-check edge (one in six): Friend Guard on the defender's ally, Flower Gift in sun on the attacker's ally, or a
  fainted Friend Guard ally, with the defender's HP put between upstream's own `getAttackDamage` with and without
  the ability. This runs before the state is written, so the replay starts from its result.

It runs upstream's `EnemyCommandPhase(f).start()` for each enemy `TurnInitPhase` would give one (field index 0 then 1,
fainted ones skipped) and records each step's command, pre-turn command, switch counter, RNG states, move queue and
logged scores, then the phases left queued, the abilities applied and revealed for all four Pokemon, and for each of
the two trainer slots the switch-in scores and the slot `getNextSummonIndex` picks. `tests/doublestest.c` includes
`tests/aitest.c` unchanged and uses its reader and record comparison for every record that is the same in doubles;
the doubles-only records (`STEP`, the second enemy's move queue, `APPLIED` for four Pokemon, `MATCHUP`/`NEXT` per
slot) are compared the same way. It exits 2 if the file's contract lines are missing or don't add up, and 1 on any
mismatch or if the compared cases don't include a command at the ally, one with two or more targets, one at both
opponents, a commanding Tatsugiri, a fainted Pokemon in a field slot and partner-slot switch-in scores.

```
sh ai-core/tools/doublesdiff.sh <upstream> 2000 <seed>    # seeds 777 9001 4242 31337
```

Checked 2026-10-05 on main `68a60c9` plus this change, Node v22.22.0, GCC 13.3.0, vitest 4.1.11, upstream `8555c08`,
2,000 cases per seed; upstream threw on none:

| Seed | Covered / identical | Mismatches | Not covered (Illusion / move condition) | Commands at the ally | Two or more targets | At both opponents | Commanding | Fainted in a slot | Double trainers (partner-slot scores) |
|---|---|---|---|---|---|---|---|---|---|
| 777 | 1,980 / 1,980 | 0 | 20 (11 / 9) | 724 | 1,322 | 1,212 | 182 | 239 | 767 (388) |
| 9001 | 1,987 / 1,987 | 0 | 13 (7 / 6) | 760 | 1,358 | 1,241 | 202 | 242 | 761 (394) |
| 4242 | 1,971 / 1,971 | 0 | 29 (13 / 16) | 761 | 1,285 | 1,185 | 199 | 255 | 797 (394) |
| 31337 | 1,974 / 1,974 | 0 | 26 (19 / 7) | 732 | 1,332 | 1,226 | 199 | 233 | 734 (364) |

Over the four seeds: 8,000 cases, 7,912 covered and identical, 350 switches and 15,078 moves compared. The 88
not-covered cases are the established exclusions: 50 read a Pokemon with the Illusion ability and no loaded
disguise, 38 hit a move condition battle-core hasn't ported.

Planted faults in the C (one at a time, clean build each, on the same four traces; failing cases per seed in the
order above):

| Planted fault | Failing cases |
|---|---|
| targets: no cutoff at half the best weight | 118 / 108 / 135 / 122 |
| targets: ally's benefit score sign flipped | 1,012 / 1,027 / 1,008 / 1,030 |
| targets: no shift by the lowest weight | 231 / 232 / 212 / 214 |
| targets: second target's weight dropped from the draw | 900 / 882 / 850 / 816 |
| move scores: ally effectiveness not divided out | 10 / 8 / 15 / 20 |
| move scores: ally STAB not divided out | 5 / 3 / 4 / 3 |
| move scores: ally benefit sign flipped | 249 / 254 / 263 / 275 |
| move scores: first target instead of the best | 324 / 353 / 339 / 361 |
| KO check: ally's ability never ignored | 15 / 15 / 12 / 14 |
| KO check: the ally counted as a KO target | 211 / 242 / 221 / 263 |
| damage: no spread 0.75 | 65 / 65 / 69 / 72 |
| damage: spread 0.75 without `multiple` | 126 / 132 / 130 / 100 |
| damage: no Friend Guard | 23 / 23 / 25 / 20 |
| damage: side immunity read on the defender only | 0 / 2 / 0 / 1 |
| command: commanded Pokemon not skipped | 90 / 89 / 100 / 86 |
| move scores: no -20 for its Tatsugiri | 61 / 81 / 73 / 83 |
| trapped: COMMANDED doesn't trap | 1 / 2 / 4 / 4 |
| switch: matchup against one opponent | 66 / 70 / 77 / 78 |
| switch: fainted opponents counted | 5 / 8 / 6 / 8 |
| switch-in scores: bench counted from slot 1 | 986 / 981 / 1,021 / 952 |
| switch-in scores: a player field of one | 672 / 657 / 711 / 639 |
| switch-in scores: no trainer-slot filter | 554 / 544 / 586 / 533 |
| switch-in scores: slot filter in a single-trainer battle | 187 / 177 / 180 / 165 |
| move scores: no Flame Burst -5 | 25 / 22 / 19 / 15 |
| terrain: one opponent read for the opponents' side | 4 / 0 / 2 / 2 |
| damage: Friend Guard from an inactive ally | 0 / 0 / 0 / 0 |

25 of 26 are caught. The last can't change a result: an inactive ally in a field slot is a fainted one, and
`canApplyAbility` already refuses a Pokemon at 0 HP, so the `isActive` check only matters for a Pokemon a challenge
bars, which this suite (and battle-core) doesn't build. Side immunity (Queenly Majesty and co. over the side) is
caught on two seeds of four: it changes the KO check only when the immunity holder is the defender's ally and the
move is a priority one that would KO.

Not covered by this suite: held items, bosses, mystery encounters, a disguise in doubles, natural Illusion, a
commanding Tatsugiri that is also Transformed, and anything the turn engine does after the commands (that is
battle-core's Task 73 suite).

### Established checks, before and after

Same upstream checkout, Node v22.22.0:

- `tools/difftest.sh <upstream> 5000 <seed>`: 4,938 / 4,946 / 4,942 / 4,936 covered and identical, 0 mismatches,
  62 / 54 / 58 / 64 not covered on 777 / 9001 / 4242 / 31337. Replaying the saved traces with main's build and with
  this change gives byte-identical output on all four seeds.
- `tools/illusiondiff.sh <upstream> 5000 <seed>`: 4,945 / 4,959 / 4,947 / 4,959 covered and identical, the planted
  identity still detected, as in Task 68's table.
- `make -C ai-core test-berry`: 404 cases, 404 identical.
- `make -C game-core test`: exits 0 with the same output as main (`8decbf9d` both runs' digests).
