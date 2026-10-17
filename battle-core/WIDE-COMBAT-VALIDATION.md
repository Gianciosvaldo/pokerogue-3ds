# Wide damage and Substitute state

Pinned upstream `8555c08c823b856cbec4eb99ca84ea52a955836d`, locales
`270ed0a2938b2c3e2e34e7de940e84aecadf548b`, Node 22.23.3.

Native `toDmgValue` in `src/utils/common.ts` returns
`Math.max(Math.floor(value), 1)`. Both battle-core and AI narrowed that result
to int32 before the later HP clamp. Wide attacks could therefore become zero
or negative. Both helpers now retain the floored double. Native
`MoveEffectPhase.applyMoveDamage` also subtracts the entire raw hit from
Substitute HP before end-of-move cleanup; the transient negative value needs
the same width. Damage bookkeeping still counts only the absorbed positive HP.

## Evidence

`tools/wide-combat-diff.sh UPSTREAM 777` and then `2034` reproduce the native
producers, comparisons and fault checks. Native BattleScene battle RNG is
captured before constructing GameManager and restored after battle startup.
Gameplay formulas are executed by upstream; observations do not replace them.

- Full turns: 8,192 per seed, all 16,384 exact, no source exceptions or unsupported
  cases. Four field actors and benches, levels 1 through 1,000,000 including
  255/256 and 65,535/65,536, Endless waves through 1,000,000, ordinary, fixed,
  OHKO, variable-power and spread moves, Substitute, switching and multiple
  faints. The established doubles comparator and its coverage floors remain;
  the new wrapper additionally requires every input turn to be compared.
  An observational passthrough spy verifies actual raw damage exceeds int32.
- Owning private damage leaf: protocol v2, 8,192 per seed, all 16,384 exact.
  Compares four actors' 125-field frames, the complete return tuple, negative
  Substitute HP, absorbed-damage bookkeeping, message queue levels/order/text
  and both RNG states. The C test includes the actual private phase source.
  API probes include supplied effectiveness and same-side targets; this is
  evidence for the callback boundary, not every input being a legal turn.
- Protocol v3 retains that matrix and adds actual native simulated damage
  calls before the leaf, including critical and ability-ignore inputs.
  All 16,384 exact; 406 simulated hits exceed int32 in each seed. Simulation
  preserves the observed frames, queues and RNG. The C side calls the actual
  AI implementation without adding a production testing API.
- Before the production fix, the corrected wide-level turn parser exposed
  2,230 mismatches in seed 777; the properly initialized private-leaf replay
  exposed 386. Preliminary captures with an incorrect native constructor or
  incomplete wave/turn initialization are not validation evidence.
- Ten compiled runtime faults caught: battle/AI int32 damage, wrong rounding,
  int32 Substitute subtraction, premature clamp, bookkeeping, critical text,
  dropped/unsigned legacy HP and accepting nonfinite saved HP. Seventeen
  malformed captures rejected, including fractional and overflowing levels.
- The old comparator itself truncated levels to uint8. It now strictly parses
  the modeled positive int32 domain. Substitute text formatters use doubles;
  no comparison is omitted and no established coverage floor is reduced.
- Retained doubles covered IDs: 1,969/1,958/1,970. Retained mixed covered IDs:
  1,978/1,973/1,993/1,996. Previously validated callback captures remain exact.

## Save compatibility

The existing named-field save format records field sizes. Its loader now
converts only historical signed-int32 tag HP to double; unrelated fields keep
their existing handling. Eight actual old writer buffers exercise both signs
and boundaries with adjacent tag metadata. The normal game-core suite includes
221 checks for migration, round trips, wide values and malformed new values.
New nonfinite or fractional tag HP is rejected. SAVE_FORMAT remains 2; this
claims old-to-new migration, not compatibility with an old reader.

The fixture provenance and old-writer reproduction are in
`game-core/tests/fixtures/SUBSTITUTE-HP.md`.

Normal app/custom/save sanitizer tests and exact-head CI test, game-tests and
3DS build remain required. Only the dedicated custom-only Task63 job is skipped.
These bounded matrices do not establish natural high-wave generation, every
fusion/boss/ability combination, level-up learning/evolution, all remaining
callbacks or complete Endless support. Those remain separate inventory work.
