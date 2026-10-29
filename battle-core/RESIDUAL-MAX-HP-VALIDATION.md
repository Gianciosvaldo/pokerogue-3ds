# Permanent maximum HP in tags, Pledge and Wish

Source: `8555c08c823b856cbec4eb99ca84ea52a955836d`; locales:
`270ed0a2938b2c3e2e34e7de940e84aecadf548b`; Node 22.

Seven remaining direct HP reads used summon overrides where native callbacks
call `getMaxHp`. Use permanent HP for the residual helper (Curse, Salt Cure,
Nightmare, Aqua Ring, Ingrain), Leech Seed, Spiky Shield, Powder, damaging traps,
the Fire/Grass Pledge arena effect and Wish creation. This completes the direct
literal `pr_get_stat(..., STAT_HP, false)` candidates found in this audit. It
does not establish all variable-stat or other HP interactions.

## Native owners

All **2,560 cases** retain both sides and active slots, Rattata/Blissey/Shedinja/
Squirtle identities, levels 1/1,000,000 and four public HP-setter contexts on
all six party members: none, 1, twice permanent HP plus one, and 34,359,738,371.
Twenty actions cover:

- Actual `Pokemon.lapseTag` for Leech Seed, Curse, Salt Cure, Nightmare, Aqua
  Ring, Ingrain and all ten damaging traps: Bind, Wrap, Fire Spin, Whirlpool,
  Clamp, Sand Tomb, Magma Storm, Snap Trap, Thunder Cage and Infestation.
- Powder under an actual Fire MovePhase, and Spiky Shield under an actual
  contact MoveEffectPhase, including failed-move/target-list/hit-count changes.
- Actual arena lapse of Fire/Grass Pledge on either side.
- The registered Wish callback, followed by two actual positional-manager
  activations that remove the stored Wish and queue its healing phase.

Native tags are added through `Pokemon.addTag` and asserted admitted before the
boundary. Their constructed fields and immediate setup state are transported
into C. Nightmare starts with a Sleep status. The Water identity covers Salt
Cure's alternate divisor. The C entry points are `pr_lapse_tag`, arena lapse,
move-effect dispatch and the positional-tag phase owner. Tag-add behavior itself
is outside this comparison; the C input copies native constructor observations.

Wish is enabled in the pinned move registration, but `getAttrs("WishAttr")`
cannot resolve its missing named registry entry. The producer selects that
existing attribute via `getAttrs("MoveEffectAttr")`, as the owning effect
dispatcher does, and checks its constructor name and unmocked `apply`. No
registry entry or source gameplay method is changed, and no synthetic attribute
is constructed to bypass the owning registration.

## Validation

The old owner mismatches **1,708 cases**. The fixed replay performs
**4,192,000 checks with zero mismatches**: projected actors, counters/status,
ordered battler and arena tags, Wish storage, move context, phase queues,
messages, animation/heal/status/faint payloads and both RNG streams. Native
battle RNG is captured before GameManager construction and restored after
startup. Twelve compiled faults, eleven malformed captures and four forged
observations must fail. Fresh records and ten pinned locale namespaces match
byte-for-byte. No case or unsupported result is omitted.

Scoped ASan/UBSan/LSan instruments the edited battler-tag, arena-tag and
move-effect units plus the replay; supporting phase/archive units remain
unsanitized. Normal core/app/custom/save/football checks and the actual ARM
build remain merge gates. Other retained proofs stay enabled.

```sh
python3 battle-core/tools/check-residual-max-hp-proof.py
RESIDUAL_MAX_HP_SANITIZE=1 python3 battle-core/tools/check-residual-max-hp-proof.py --replay-only
battle-core/tools/reproduce-residual-max-hp.sh /path/to/pinned/upstream /tmp/residual-max-hp
```

This is API/lapse/positional-owner evidence. Queued healing, status and faint
phases are observed, not executed. Held items, suppression/passives, Magic
Guard's Spiky Shield distinction, alternate contact moves, multiple concurrent
tags/Wishes, arbitrary source removal, fusion, complete turns/runs, saves,
permanent HP outside the modeled int32 stats, other languages and hardware
remain separate. These bounds do not establish complete battle-core parity or
all HP semantics.
