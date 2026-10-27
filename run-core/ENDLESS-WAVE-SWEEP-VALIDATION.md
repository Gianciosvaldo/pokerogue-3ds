# Endless wave generation through the source terminal wave

Pinned upstream `8555c08c823b856cbec4eb99ca84ea52a955836d` ends Endless after
wave 5,850 in `BattleEndPhase`. This proof retains native observations for
every wave 1–5,850 in both Endless modes: 11,700 cases, generated from seed 777.
It adds retained evidence, not a production mechanics change or full-run
acceptance. It does not sample every RNG input at each wave.

The producer calls actual `GameMode`, `Battle` and `BattleScene` methods for:

- Fifteen game-mode rules, including difficulty, boss/final-wave predicates,
  fixed battles, modifier chance and starting values.
- Normal and ignore-cap EXP level caps, including the no-current-wave input.
- Direct level generation and trainer selection with their input and output
  RNG states, plus resetSeed/battle-seeding-chain level and trainer results.
- Encounter boss segments and double-battle chance with 0–3 native lure
  modifiers. Species, biome, gym offset, force-boss and level inputs are retained.

Native `BattleScene.prototype.randBattleSeedInt` is captured before constructing
`GameManager` and restored after battle startup. The rows preserve every
observed result; exceptions produce nonfinite fields and are rejected rather
than converted to passing cases. The fixture capsule records the source,
locale and asset pins, source file hashes, producer hash and every raw row.
Fresh reproduction must match the decompressed capsule byte for byte.

The unchanged `wavetest` replay compares 175,500 game-mode results and 11,700
results in each of the other six groups: 245,700 comparisons in total, with
zero mismatches and zero uncovered results. Its historical exit status alone
does not reject uncovered cases, so this proof also requires the exact complete
summary and case counts. Empty, reduced or uncovered-only runs cannot pass.

Observed maxima are 69,653 for direct generated levels, 69,710 for levels in
the seeding chain, and 69,222 for the normal EXP cap. These cross the 16-bit
boundary. Ordinary non-boss observations remain below it (maxima 58,155 direct
and 58,129 seeded). The exploratory ordinary-level 16-bit narrowing probe is
retained and required to survive; it is **not** counted as a detected fault or
evidence that this branch crosses 65,535. Native boss-level narrowing and cap
narrowing are separately detected.

The checker rejects six compiled faults in the real wave owner: narrowed cap,
narrowed boss level, omitted ordinary/boss RNG draws, wrong battle seed offset,
and comparisons flagged uncovered. An unmodified build through the identical
link path must pass first. Eight malformed captures and four forged native
observations are also rejected. Build failures do not count as fault detection.
Integer-to-16-bit fault conversions use defined integer narrowing.

```sh
python3 run-core/tools/check-endless-wave-sweep.py
# Node 22, installed dependencies and a clean checkout of the exact pin:
bash run-core/tools/reproduce-endless-wave-sweep.sh /path/to/upstream /path/to/output
```

This covers wave-rule and level-generation methods. It does not execute a full
run, construct complete high-wave enemy parties, compare every generated
species/form/moveset/held inventory, or establish all battle damage, reward,
save and display paths. The chain compares its level/trainer results; it does
not retain an additional post-chain RNG snapshot. Other slices retain their
own evidence and limits. Whole Endless acceptance and the final battle-core
source inventory remain open. Normal core/app/custom/save tests and the actual
ARM build/stack checks remain required; only dedicated Task63 is skipped.
No Windows builder is released.
