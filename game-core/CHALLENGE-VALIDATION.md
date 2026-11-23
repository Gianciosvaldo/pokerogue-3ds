# Challenge Mode, part 1: the foundation and Fresh Start

Upstream pin 8555c08c823b856cbec4eb99ca84ea52a955836d. What this change makes playable: Challenge Mode (`GAME_MODES_CHALLENGE`) in game-core, with
**Fresh Start** (values 1 and 2) as the only challenge. Every other challenge is refused by `gc_run_set_challenges` and by `gc_run_load` until its own PR.

## What upstream has (checked, not assumed)

* `allChallenges` holds 10 challenges. `LOWER_MAX_STARTER_COST` and `LOWER_STARTER_POINTS` exist as ids but are **not** in that list: no value, no effect
  (the earlier plan counted 12; the producer showed the budget staying 10 with them set). Their maxima are 0 here.
* Challenge Mode is Classic with `isChallenge` and the challenge hooks: every strict Classic check in game-core now reads `gc_is_classic_mode`.
* `species.getLevelMoves()` = the species' level moves + `formLevelMoves[forms[0].formKey]`, no duplicate removal, stable-sorted by level
  (Fresh Start's moveset rule reads it; Calyrex and Indeedee have form rows that matter).
* A starter without a tera type gets `starterPokemon.species.type1` (the species', not the form's). game-core used the form's: fixed in `gc_mon.c`
  (Oricorio Pom-Pom was the case the differential caught; the change is for every mode).

## Differential (upstream functions replayed)

`tools/challengestartersdiff.sh <upstream checkout> [N] [seed]` runs `tests/challenge-starters.producer.test.ts` in the pinned upstream
(Node 22, vitest): per case a random profile (dex, natures, abilities, passives, IVs, egg moves, value reduction), then for Fresh Start 0/1/2
12 starter queries (cost, `checkStarterValidForChallenge`, `getStarterData` through the real `applyChallenges`) and a party through the real
`SelectStarterPhase.initBattle` (id, form, nature, ability, passive, luck, tera, HP, stats, IVs, moves, and the RNG state after). The comparator
`tests/challenge_starters_diff_test.c` replays the same inputs through game-core.

    cd game-core && make -s -j8 BUILD=/tmp/gcb /tmp/gcb/libgc.a
    BUILD=/tmp/gcb tools/challengestartersdiff.sh <upstream> 400 <seed>

| seed | cases (Fresh Start on) | queries | party members | checks | TOTAL mismatches |
|---|---|---|---|---|---|
| 777 | 400 (342) | 4800 | 1418 | 113304 | 0 |
| 9001 | 400 (342) | 4800 | 1404 | 112912 | 0 |
| 4242 | 400 (342) | 4800 | 1407 | 112996 | 0 |
| 31337 | 400 (342) | 4800 | 1369 | 111932 | 0 |

`tools/challengestartersplant.sh <trace>` replays a trace through copies of game-core with one line changed each (21 plants: cost, each
select-view mask, nature mask, IV cap, form reset, ability wrap, passive, move level bound, form moves, level sort, fill duplicates, luck, IV cap,
tera type, the hard-valid rule, the default tera). On the seed 777 trace every one exits 1 except `default-tera-form`, whose case (a form
with a different first type and no tera type) is not in that trace; on the seed 4242 trace it exits 1 too (`TOTAL mismatches=1`).

## Host test (what has no upstream function to replay)

`tests/challenge_host_test.c`, 184 checks (`make -C game-core challenge-host`): admission (Classic run refuses values; above-maximum, unplayed
challenges, Lower* ids refused; flags synced to run-core; refused after the starters), Fresh Start's nature filter and starter rules through
`gc_run_set_starters`, the ribbon table (Moveset Randomizer none; Flip/Inverse keep only their own; Passives on all; Nuzlocke needs all three;
Single Generation/Type bits; Limited Support), a Challenge win counted as a Classic one with the ribbons, a win with an unplayed challenge
refused and unsavable, the `challenges` save section round trip and its refusals (value too high, unplayed challenge, section missing, Classic
save has none). `tools/challengehostplant.sh`: 9 plants, all exit 1.

## Gaps (stated, not hidden)

* The ribbon table follows upstream's `awardRibbons` as read, **not** replayed against upstream; the host test pins that reading.
* `ChallengeAchv` achievements on a challenge win are not ported.
* `checkStarterValidForChallenge`'s soft step (a species' item-triggered form change to a valid form) is not done; no challenge played so far
  looks at the form. The evolution walk of the soft path is only exercisable with Single Type/Generation (later PRs): `softOnly=0` here.
* Single Generation, Single Type, Inverse, Flip Stat, Passives, Hardcore, Limited Catch/Support and Moveset Randomizer are refused until their PRs.
* Daily-Run/Challenge choices in the app (`game/`) still force Classic: that is PR 1b.
* run-core edit (disclosed, approved by lorenzo per PR): `run-core/src/shop.c` flags the shop UNPORTED for a Challenge run only when it has
  a challenge game-core does not play (`pr_run_has_unported_challenges`), otherwise as for Classic.
