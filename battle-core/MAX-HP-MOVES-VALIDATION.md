# Permanent maximum HP in move callbacks

Source: `8555c08c823b856cbec4eb99ca84ea52a955836d`; locales:
`270ed0a2938b2c3e2e34e7de940e84aecadf548b`; Node 22.

Six move-effect reads used temporary summon HP where the pinned methods call
`getMaxHp`, which bypasses those overrides. Use permanent HP for HP-based recoil,
Flame Burst's ally damage, HalfSacrificialAttr, Rest's queued heal, Ghost Curse's
cost and the shared HealAttr/VariableHealAttr amount. This follows the same
getter contract as the separate Substitute and pending-heal fixes.

## Native evidence

All **1,344 cases** invoke actual registered attributes for Struggle, Flame Burst,
Steel Beam, Rest, Curse, Recover and Moonlight. Every callback returns true
upstream. Struggle uses actual `pushMoveHistory` with a successful last move;
Curse uses a Ghost type override and asserts the native type query recognizes it.
The matrix crosses both sides and active slots, three species identities, levels
1/50/255/1,000,000 and four public HP-setter contexts on all six party members:
none, 1, twice permanent HP plus one and 34,359,738,371. The other members stay
level 30. Abilities/passives and held items are not part of this basis.

The replay compares six actors' projected states, damage bookkeeping, pending
status and Curse tags, exact level/dynamic phase queues, text and both RNG
streams. Payload checks retain damage amounts/results/critical flags; heal
amounts/options/text; Rest's reset recipient and dynamically queued status
effect/source/sleep duration/text; and queued faint recipients/source/options.
Native battle RNG is captured before GameManager construction and restored
after startup. The source's methods and expectations are unchanged.

The original getter owner mismatches 1,008 cases in the initial projection.
The final strengthened replay performs **2,084,352 checks, zero mismatches**.
Nine compiled faults, eleven malformed captures and four forged observations
must fail. A duration fault initially survived because only Rest's dynamic
phase name was captured. The final producer/replay retain that phase's payload,
and the duration fault is now caught. No case or text comparison is dropped.
Fresh native records and ten pinned locale namespaces reproduce byte-for-byte.

Scoped ASan/UBSan/LSan instruments the changed move-effect unit and replay;
supporting phase/archive units remain unsanitized. The retained strengthened
Flame Burst capture passes all **16,384 cases** unchanged. Its older five-field
IX draft is rejected by the current seven-field parser; the retained proof
includes hit-count/hits-left fields and is used without transport normalization.
Normal core/app/custom/save/football checks and the ARM build remain merge gates.

```sh
python3 battle-core/tools/check-max-hp-moves-proof.py
MAX_HP_MOVES_SANITIZE=1 python3 battle-core/tools/check-max-hp-moves-proof.py --replay-only
battle-core/tools/reproduce-max-hp-moves.sh /path/to/pinned/upstream /tmp/max-hp-moves
```

These are raw callback owners with queued phases observed, not executed. External
move conditions and complete turns are not claimed; C's void dispatch is not a
return-value comparison. One-HP inputs and their queued faints are retained,
without claiming all natural Wonder Guard cases. Boss shields, held items,
suppression/passives, fusion, arbitrary histories, changed recipient identities,
other healing constructors, Wish, residual tags, other languages, saves and
hardware acceptance retain their separate evidence or audit obligations. This
does not establish every maximum-HP caller or complete battle-core parity.
