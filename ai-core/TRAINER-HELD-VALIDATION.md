# Card 108: bounded trainer and held-item AI recheck

Source: `pagefaultgames/pokerogue` at
`8555c08c823b856cbec4eb99ca84ea52a955836d`, Node **22.23.3**.
Port base: `5fce401` after Card 103 (#185). Checked 2026-10-09.
This is an additive proof of existing C behavior; no production AI math changed.

## Source boundary

`tests/trainer-held.producer.test.ts` invokes actual `EnemyCommandPhase.start`,
`Move.getUserBenefitScore/getTargetBenefitScore`, `Pokemon.getAttackDamage`
(simulated), and trainer `getPartyMemberMatchupScores/getNextSummonIndex`.
The command, scores, chosen targets, switch cursor, switch counter, global/battle
RNG, move queue, condition queue, applied/revealed abilities and faint pick use
`aitest.c`'s existing comparisons unchanged. Explicit benefit/damage probes add
exact numerical comparisons. No expected score is calculated in the producer.

Eight profiles cover U-turn, Baton Pass, Parting Shot, Roar, Thief, Knock Off,
Tackle/Ember KO selection and Quick Attack. Each seed has 250 cases per profile.
Eight states exercise ordinary reserves, last enemy, wrong trainer-slot reserves,
last player, fainted reserves, low HP/positive stages, a voluntary switch and
negative stages. Rattata levels, IVs and natures vary through the seeded generator.
The last-enemy Roar state exercises #185's actual enemy-party guard; the last-player
state separately checks refusal without player reserves.

Held stock is canonical `AttackTypeBoosterModifier` (Normal, multiplier 0.2,
real stacks 1–3, virtual 0). Profiles vary no stock, exact target holder,
teammate holder and exact enemy holder. Tackle/Ember KO alternatives straddle
actual source-calculated damage HP thresholds. Knock Off and Thief run their
actual scoring and command paths with/without target stock. Stock appears in
872 cases per seed. Successful actual trainer switches are required independently
of scored commands.

The dedicated reader synchronizes the loaded `AIMON` slot into both
`pr_ai_mon.trainer_slot` and `pr_pokemon.trainer_slot`, as documented for callers.
It handles the new `XMOD` records without changing the established reader's old
opaque-modifier exclusion. All new cases must compare: an unsupported reason is
a failure, never a new exclusion. Generated source records, seeds, counts,
tolerances and established suite classifications are unchanged.

## Results

| Seed | Requested / covered / identical | Mismatches | Actual switches | Scored commands | Held contexts |
|---|---|---|---|---|---|
| 777 | 2000 / 2000 / 2000 | 0 | 52 | 1948 | 872 |
| 9001 | 2000 / 2000 / 2000 | 0 | 54 | 1946 | 872 |
| 4242 | 2000 / 2000 / 2000 | 0 | 46 | 1954 | 872 |
| 31337 | 2000 / 2000 / 2000 | 0 | 43 | 1957 | 872 |

Fresh unchanged established producers generated 20,000 cases under Node 22;
the current comparator retained covered/identical **4938/4946/4942/4936**,
zero mismatches, zero stats mismatches and zero skipped records. The existing
62/54/58/64 exclusions remain. These are not promoted by the new suite.

Three temporary mutations to actual C callsites failed all four source traces:

| Mutation | Failing cases: 777 / 9001 / 4242 / 31337 |
|---|---|
| Skip `move_can_ko` pool filtering in `ai.c` | 263 / 260 / 254 / 262 |
| Remove held target benefit in `ai_score.c` | 374 / 374 / 374 / 374 |
| Ignore missing reserve penalty in `force_switch_ubs` | 189 / 189 / 189 / 189 |

The mutations were restored before clean control replays and host checks.
The new transport requires a versioned request-count header, sequential complete
cases, all eight profiles/states, required state/output records, finite exact
probes and a matching zero-throw footer. **25 malformed/request probes exit 2**:
missing/duplicate RNG/AIMON/probe and duplicate keyed AIMON/APPLIED owners, omitted queue/next/after/party/end, wrong
header/footer/count/order, trailing/unknown records, nonfinite values, unknown
modifier and virtual stock. Nothing changes `aitest.c` or old comparators.

## Reproduction

Activate the named Node 22 environment, then run from the port:

```sh
for seed in 777 9001 4242 31337; do
  BUILD=/tmp/ai108-$seed sh ai-core/tools/trainerhelddiff.sh <upstream> 2000 "$seed"
done
python ai-core/tests/trainer-held-malformed.py /tmp/ai108-777/trainerheldtest /tmp/ai108-777/ai108-777.txt /tmp/ai108-malformed
for seed in 777 9001 4242 31337; do
  BUILD=/tmp/ai-retained-$seed sh ai-core/tools/difftest.sh <upstream> 5000 "$seed"
done
make -s -j2 -C game-core test
game-core/build/gcplay --auto 1000 --jobs 8
game-core/build/gcplay --auto 1000 --jobs 8 --save-test
```

Host baseline: the 200 normal/save pair is **bc0fbf1c**. Both 1000-run variants
finish all 1000 runs, zero stuck/crash/timeout, **50394 turns**, digest
**5b6c9ee2**; saves have **14679** round trips, largest **11472 bytes**.
Both full normal and saved logs are byte-identical before/after. The final
200 normal/save pair remains bc0fbf1c. Source proof, host
consistency, exact-head CI and console acceptance remain distinct.

## Limits retained

This is partial Card 108, following #185's bounded Card 103 proof. It does not
establish boss segments, natural partner doubles, challenge legality, Transform,
Sketch/Mimic/Imposter, natural Illusion, unknown/virtual item families, general
held-item lifecycle, complete trainer battles, or whole Classic run parity.
Existing loaded-Illusion/doubles proofs keep their own scope.

The independent broad battle trainer replay's seed 4242 CASE 1051 has a
baseline-identical queued-phase undefined-metadata serialization mismatch,
recorded by Card 103. This bounded AI suite does not repair or waive that result;
it compares actual AI queues for its explicitly loaded contexts.
