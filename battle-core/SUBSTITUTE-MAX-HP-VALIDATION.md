# Substitute and Shed Tail maximum HP

Source: `8555c08c823b856cbec4eb99ca84ea52a955836d`; locales:
`270ed0a2938b2c3e2e34e7de940e84aecadf548b`; Node 22.

Native `AddSubstituteAttr.getHpCost` and `SubstituteTag.onAdd` both call
`Pokemon.getMaxHp`, which calls `getStat(HP)` with the default bypass of summon
stat overrides. C's condition closure already did that, but its cost callback
and doll constructor read the temporary HP override instead. They now use the
same permanent-HP getter. Doll HP stays a floored double without an int32 cast.

An actual `setStat(HP, 1, false)` leaves permanent maximum HP unchanged upstream;
the old C path instead paid one HP and created a zero-HP doll. Large temporary
values could also reach the constructor's narrowing cast. The initial
432-case native comparison had 324 mismatches, and the fix matched every case.

## Evidence and scope

The final proof retains those semantic cases and adds level 1,000,000:
**576 cases, 890,304 checks, zero mismatches**. It varies both sides and active
slots, Rattata/Blissey/Shedinja identities, levels 1/50/255/1,000,000, and no
override versus public setter values 1, twice permanent HP plus one, and
34,359,738,371. Custom abilities are disabled; the Shedinja identity here does
not establish the natural Wonder Guard one-HP case, which has separate retained
HP-cost evidence.

Three owners run: actual `Pokemon.addTag` and the registered Substitute and
Shed Tail `AddSubstituteAttr.apply` methods. These are raw public callback
boundaries, not full move turns. External move conditions are not invoked;
their existing condition closure is unchanged. Native battle RNG is captured
before GameManager construction and restored after battle startup. Source
gameplay methods and formulas are not replaced. Presentation/phase advancement
is bypassed to inspect queued work.

The replay compares six actors' projected states, doll HP/focus/source fields,
both RNG streams, exact ordered phase queues and rendered messages, damage
animation amounts/results/critical flags, and Pokemon animation recipient
identities/keys. Nine pinned locale namespaces are retained. Seven compiled
faults, eleven malformed captures and four forged observations are rejected.
Fresh native records and locales reproduce byte-for-byte. Scoped
ASan/UBSan/LSan covers both changed units and the replay; supporting archive
units are unsanitized. Normal core/app/custom/save/football checks and the
actual ARM build remain merge gates.
Both retained 12,928-case HP-cost captures pass exactly, including their
temporary-HP, boss, Enduring and natural one-HP controls; no case is dropped.

```sh
python3 battle-core/tools/check-substitute-max-hp-proof.py
SUBSTITUTE_MAX_HP_SANITIZE=1 python3 battle-core/tools/check-substitute-max-hp-proof.py --replay-only
battle-core/tools/reproduce-substitute-max-hp.sh /path/to/pinned/upstream /tmp/substitute-max-hp
```

This basis does not establish ordinary gameplay reachability of arbitrary HP
setters, complete turns, switching source identities, traps removed by a doll,
boss shields, fusion/passives, permanent HP beyond C's int32 stat domain, all
maximum-HP callers, save migration or hardware behavior. Existing wide damage,
Substitute save migration and HP-cost proofs remain separate. Other temporary
HP getter candidates still require direct source/owner review; no blanket
replacement or full battle-core completion claim is made.
