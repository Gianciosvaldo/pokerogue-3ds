# Final-boss first-form faint cancellation

Pinned upstream: `8555c08c823b856cbec4eb99ca84ea52a955836d`; locales
`270ed0a2938b2c3e2e34e7de940e84aecadf548b`; Node 22.

`FaintPhase.start` resets summon data and registers player participants before
checking `currentBattle.isClassicFinalBoss && !this.player`. That predicate is
`gameMode.isClassic && gameMode.isWaveFinal(waveIndex)`; at this pin it admits
Classic and Challenge at wave 200, not Endless/Fusion Endless/Daily at that wave.
The comparison records the predicate's truthiness as used by the source `if`.

For a first-form enemy, `handleFinalBossFaint` increments HP, queues a zero-amount
INDIRECT `DamageAnimPhase` for that battler, and ends without ordinary `doFaint`.
That damage phase owns the existing phase-two check. The port previously marked
all enemy wave-200 faints unsupported and continued ordinary faint handling.
It now follows the cancellation and restricts the final-boss gate to the actual
modes. A full engine pool still reports the engine-limit guard and overflow.

For form index >0, upstream waits for its victory dialogue callback before
`doFaint`. The follow-up [dialogue proof](FINAL-BOSS-DIALOGUE-VALIDATION.md) now
covers that separate path; the 480 cases here retain their original first-form
scope. Shield damage arithmetic, abilities and form rules are unchanged here.

## Native comparison

`final-boss-faint.producer.test.ts` starts a real native battle using GameManager,
then invokes actual `FaintPhase.start` in 480 prepared contexts: five modes,
waves 199/200/201/5850, either side, HP 0/1, move queues of length 0/1/4, and a
charging tag present/absent. All 480 rows are required in fixed order. The producer
captures native `BattleScene.prototype.randBattleSeedInt` before constructing
GameManager and restores it after starting the battle. Global and battle RNG
states are seeded and recorded separately.

The native private `doFaint` is intercepted, so this proof signs off the prefix
and first-form cancellation only. The C replay observes at its existing faint
observer boundary, before ordinary faint effects. It compares selected HP, tag
types, move queue IDs, participants, queue levels/phase type/recipient/amount/
result, and both RNG states. Positive-HP early returns and canceled faints compare
the full resulting prefix state; canceled faints must not add faint counts or
history. Ordinary downstream faint execution is covered by retained separate
proofs, not by this intercept.

Results: 480 contexts, 15,096 checks. Fourteen separately compiled faults are
caught: admitting wrong modes/waves/players, excluding Challenge, failing HP
restoration, wrong damage amount/result/recipient/queue level, continuing into
ordinary faint, omitting summon reset/participants, and drawing either RNG.
One compiled pristine control passes. Eleven malformed/forged captures fail,
including partial/extra records, context/footer/count changes, nonfinite values,
and forged HP, RNG, recipient and ordinary-faint admission.

A fresh pinned-source reproduction is byte-identical after gzip decompression.
Targeted ASan/UBSan/LeakSanitizer of the replay and changed phase unit passes.
Retained faint-history replay passes 72 cases/7,864 observations; retained sleep
cancellation passes 3,072 cases/102,330 checks. Normal CI remains required.

Reproduce with Node 22:

```
bash battle-core/tools/reproduce-final-boss-faint.sh PINNED_UPSTREAM OUTPUT_DIR
python3 battle-core/tools/check-final-boss-faint-proof.py
```

The committed compressed fixture SHA-256 is
`d16055f7fa2108514decf869a18453d0d37b2297f771e6810c0a67305cf72f63`.

These are prepared phase/API contexts, not natural threshold-bypass encounters,
legal-start Classic clears, second-form dialogue acceptance, full Endless runs
or complete battle-core coverage. Existing unsupported accounting outside the
proved prefix stays explicit.
