# Retained Tailwind sources through Court Change

Pinned source: `8555c08c823b856cbec4eb99ca84ea52a955836d`; locales:
`270ed0a2938b2c3e2e34e7de940e84aecadf548b`; Node 22.

Native `TailwindTag.onAdd` resolves its caster with the scene's full-party
`getPokemonById`. Court Change quietly removes and re-adds Tailwind while
retaining that identity. The caster can therefore be benched or fainted when
the callback runs. C previously looked only on the field, omitted Wind Power
and Wind Rider callbacks and marked these cases unsupported.

The dispatcher now uses the full-party lookup. It retains the pinned callback's
original-caster side, even when Court Change moves the tag to the other side.
A missing caster with both parties supplied is a native no-op. Missing party
context remains explicitly unsupported; an undefined source remains a no-op.
Two separate C context guards retain those last boundaries.

## Evidence

The native producer captures `BattleScene.prototype.randBattleSeedInt` before
constructing GameManager and restores it after battle startup. It invokes the
actual registered Court Change attribute and arena tag callback. No gameplay
formula or expected queue is replaced. Presentation/assets and phase execution
are bypassed so the raw queued observations can be inspected.

All 2,048 matrix cases are retained: both source sides and tag sides; active,
benched, absent and fainted-benched casters; direct quiet addition and Court
Change; four primary/passive ability patterns; charged and uncharged recipients;
speed ties and distinct speeds; Attack stages -6, 0, 5 and 6. Each case supplies
two complete three-member parties, the actual wave seed and both RNG states.
The same six level-30 Rattata and distinct nonzero IDs are used throughout.

The replay compares projected Pokemon state, Charged tag fields, ordered arena
tags, both RNG streams and exact level/dynamic phase queues. It retains markers,
rendered English messages, cached displayed abilities, stat changes and stat
source object identity. Nine pinned locale namespaces are captured directly.
The final replay performs **3,208,960 checks with zero mismatches**. Against the
old field-only lookup, the complete-context capture has 1,536 mismatching cases.

The checker rejects ten compiled implementation faults, eleven malformed
captures and four forged observations. A fresh native run reproduces the
records and locales byte-for-byte. Scoped ASan/UBSan with leak detection passes
for the changed arena unit and replay; supporting archive units are unsanitized.
Normal core/app/custom/save tests and the actual ARM build remain merge gates.

```sh
python3 battle-core/tools/check-tailwind-owner-proof.py
make -C battle-core tailwind-owner-host-test
battle-core/tools/reproduce-tailwind-owner.sh /path/to/pinned/upstream /tmp/tailwind-owner
```

This proves the retained-source callback and Court Change owner for this basis.
Queued stat/display phases are observed, not executed here. It does not prove
whole doubles turns, all Court Change users, multiple simultaneous arena tags,
arbitrary ability suppression or fused identities, other languages, wide levels,
all source removal histories or hardware behavior. No coverage flag is cleared
to admit the previously missing callbacks.
