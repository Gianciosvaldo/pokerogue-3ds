# Bad Dreams in doubles

Upstream `8555c08c823b856cbec4eb99ca84ea52a955836d`, locales
`270ed0a2938b2c3e2e34e7de940e84aecadf548b`, Node22.23.3.

PostTurnHurtIfSleepingAbAttr.canApply checks active opponents in field order:
at least one must be asleep or have Comatose, without an active indirect-damage
blocker. apply returns immediately for simulation, then visits opponents using
getOpponentsGenerator, which uses native speed order. Each sleeping/Comatose
victim is processed even if that particular victim has Magic Guard, provided
another opponent satisfied canApply. The actual blocker callback is deliberately
invoked on the holder at this pin. Damage is floor(maxHP/8), minimum one, and
queues native indirect-damage/faint handling and the localized torment message.

The port already implemented the singles leaf. This change removes its explicit
doubles refusal and uses the existing speed iterator for the apply loop. The
canApply field-order scan, holder blocker quirk and simulation gate remain.

## Evidence

The additive producer overlays the unchanged complete doubles driver. Its four
naturally active actors and bench use canonical species to isolate the callback;
this is a new dedicated matrix, not a replacement for the original random
corpus. Native BattleScene RNG is captured before GameManager and restored
after startup. A default prototype spy observes native apply without replacing
its implementation. Each holder position must execute at least100 calls and
source exceptions fail generation. Existing ally/spread/multiple-faint coverage
floors are retained through control turns. The strict replay adds a requirement
that all requested turns are covered, with a minimum8,192; it retains every
comparison and coverage floor in the unchanged doubles comparator.

- Seeds777/2034:8,192 complete, exact turns each, zero exceptions, unsupported
  cases or mismatches. Each source run executes4,800 real Bad Dreams callbacks.
  State, turn bookkeeping, phases/queues, English text and both RNG states match.
- Primary/passive/both Bad Dreams, holder/victim Magic Guard, Comatose,
  suppression, awake/asleep/waking, HP1 and odd maximum HP, both sides/all four
  positions, speed ties/reversed speed, faint/replacement and battle-end paths.
- Seven compiled semantic faults caught: doubles refusal, wrong blocker owner,
  rounding up, only one victim, an extra victim Magic Guard gate, missing
  Comatose, and field-order iteration. Five malformed/short captures rejected.
- Unchanged singles producer regenerated at its original2,000 cases each at
  seeds777/9001/4242/31337:8,000 exact. Historical captures were unavailable in
  this environment; the original producer and all its case/coverage requirements
  were retained. These captures are saved alongside their logs.
- Broad doubles1,969/1,958/1,970 exact; mixed1,978/1,973/1,993/1,996 exact.
  No previously covered IDs lost; three additional rich doubles cases covered.
- Full normal game-core tests pass; saved/unsaved200-run digests both361c1d0e.

Reproduce `tools/bad-dreams-doubles-diff.sh UPSTREAM 777`, then2034.
Retain singles with `tools/bad-dreams-diff.sh UPSTREAM 2000
"777 9001 4242 31337"`. Normal app/custom/save and 3DS CI remain required;
the dedicated custom-only Task63 check is excluded.

The matrix does not prove every fusion, Transform, held-item or simultaneous
ability combination, or complete battle-core. Other unsupported paths remain
explicit. No run-core changes are included.
