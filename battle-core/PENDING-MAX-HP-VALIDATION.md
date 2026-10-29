# Pending Healing Wish and Lunar Dance maximum HP

Source: `8555c08c823b856cbec4eb99ca84ea52a955836d`; locales:
`270ed0a2938b2c3e2e34e7de940e84aecadf548b`; Node 22.

Native `PendingHealTag.apply` tests the recipient's HP ratio and queues its
permanent maximum HP. Both getters bypass temporary summon-stat overrides.
The C owner instead used temporary HP for eligibility and queued heal amount.
It could consume an effect on a full-health recipient, omit a needed heal, or
queue the wrong amount. Both reads now follow native permanent maximum HP.

## Evidence

The existing producer adds an opt-in protocol 4, leaving its ordinary protocol 2
and fused protocol 3 captures admitted. The new **8,192 cases** cross all four
active doubles slots, the existing 64 stored-effect/recipient states, all four
API/entry modes and four public HP-setter contexts: no override, 1, twice
permanent HP plus one, and 34,359,738,371. The states include full/injured/zero
HP, poison, PP-only restoration, Heal Block, absent and invalid sources, duplicate
move requests, another recipient index and empty effect lists. Native battle RNG
is captured before GameManager construction and restored after battle startup.

Actual `PendingHealTag.apply`, simulated application, arena lapse and the
PostSummon entry owner are invoked. Queued healing phases and resulting status
reset phases execute. The replay retains immediate/final HP, status and PP,
ordered stored effects, exact heal amounts/options/messages, tag membership,
rendered final text and both RNG streams. It now observes maximum HP using the
same permanent getter as the native snapshot; a compiled fault restoring the
temporary snapshot getter must fail too.

Before the production fix, **1,440 cases mismatch**. All **8,192 cases now match
exactly**, with no skipped or unsupported cases. Eight compiled faults, eleven
malformed captures and four forged observations are rejected. Fresh source
records and all ten pinned locale namespaces reproduce byte-for-byte. The
initial isolated-locale replay omitted `status-effect`, causing 192 text-only
differences; capturing that pinned namespace fixes fixture context without
changing source output or the comparator's text equality.

The retained ordinary 8,192-case doubles capture also passes unchanged. Scoped
ASan/UBSan/LSan instruments the changed arena unit and replay; supporting archive
units, including heal/status phase execution, remain unsanitized. Normal
core/app/custom/save/football checks and the actual ARM build remain merge gates.

```sh
python3 battle-core/tools/check-pending-max-hp-proof.py
PENDING_MAX_HP_SANITIZE=1 python3 battle-core/tools/check-pending-max-hp-proof.py --replay-only
battle-core/tools/reproduce-pending-max-hp.sh /path/to/pinned/upstream /tmp/pending-max-hp
```

This proves the pending-effect API/entry boundary, not complete doubles turns,
natural high-wave generation, ordinary gameplay reachability of arbitrary HP
setters, every identity/form history, saves or hardware behavior. The existing
[pending-heal proof](PENDING-HEAL-VALIDATION.md) retains its sacrifice/turn/save
evidence. Other maximum-HP callers remain an explicit source-audit obligation;
this does not close all HP interactions or battle-core.
