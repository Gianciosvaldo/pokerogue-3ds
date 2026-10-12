# Beat Up party hit count

Base main `7d555af669539bff2e4e28e3d1dfd9291807d83b`. Source
`8555c08c823b856cbec4eb99ca84ea52a955836d`, locales
`270ed0a2938b2c3e2e34e7de940e84aecadf548b`, Node 22.23.3 / GCC 14.2.

MoveEffectPhase now calls the source Beat Up hit-count case. It counts the
user by identity regardless of status, plus other party members with no status.
The pin does not test HP or field activity here. Missing party context remains
explicitly unsupported. No new Pokemon/save fields or generated data are needed.

The existing per-hit power matches the pin's unusual indexing: take the hit
index modulo the eligible count, then scan the original, unfiltered party from
that index to find a contributor. This can repeat a contributor. The implementation
is preserved rather than replaced by a filtered-list interpretation. Power uses
the contributor's primary base species Attack, not current stats/form/fusion.

The additive producer delegates actual MultiHitAttr.getHitCount and BeatUpAttr.apply
over the unchanged Speed Boost scaffold. Twenty-four states on both sides cover
party sizes 1–6, poison/paralysis/faint/zero-HP boundaries, self status, Parental
Bond/Skill Link/Multi-Lens exclusions, KO interruption, Stamina/Weak Armor,
Substitute, temporary stats, a changed form, same-species fusion, Tera and Protect.
An initial new fixture omitted fusionAbilityIndex and produced 82 parse refusals
on seed777; the fixture now initializes the real source fusion fields and name.
No established producer, comparator, tolerance, count or seed was changed.

Separate actual source helpers cover 72 side/party-size/status-pattern combinations,
including users outside the first party position, zero HP without status, changed
Deoxys form and mixed-species fusion. Their 324 power probes include two extra
indices per case to exercise modulo boundaries. Actual source getters do not
advance global or battle RNG. Mixed-fusion display/full-turn behavior is outside
this proof; those mixed identities are direct numeric API boundaries.

```sh
source /workspace/shared/activate-parity.sh
for seed in 777 9001 4242 31337; do
  battle-core/tools/beat-up-diff.sh /path/to/pinned/upstream 2000 "$seed"
done
python3 battle-core/tools/build-beat-up-helper-source.py \
  /path/to/generated/beat-up.producer.test.ts /path/to/helper-upstream/test/tests/zz-dump/beat-up.producer.test.ts
# From the separate pinned helper checkout:
BEATUP_HELPERS=1 BEATUP_OUT=/absolute/helpers.txt node node_modules/vitest/vitest.mjs \
  run test/tests/zz-dump/beat-up.producer.test.ts --maxWorkers=1
make -C battle-core build/beat-up-helper-test
battle-core/build/beat-up-helper-test /absolute/helpers.txt
TURNTEST_LOCALES=/path/to/pinned/upstream/locales/en \
  python3 battle-core/tools/beat-up-mutations.py battle-core/build/beat-up-777.txt /absolute/helpers.txt
TURNTEST_LOCALES=/path/to/pinned/upstream/locales/en \
  python3 battle-core/tools/beat-up-malformed.py battle-core/build/beat-up-test battle-core/build/beat-up-777.txt
make -C battle-core all test
make -C game-core test
game-core/build/gcplay --auto 1000 --jobs 8
game-core/build/gcplay --auto 1000 --jobs 8 --save-test
```

Results:

- Before: all 2,000 turns per seed are explicitly unsupported and the owning
  wrapper rejects each subset. After: all 8,000 complete turns match, with zero
  skipped/unsupported/parse/stat/state/RNG/phase/text differences or missing text.
  All 72 count checks and 324 power probes match the actual source.
- Twelve compiled production mutations are detected after intact turn/helper
  exits 0/0: all-party counting, excluding the statused user, HP/field filters,
  wrong side, missing caller, wrong user, filtered contributor indexing, current
  form/current stats/fusion species power and missing modulo. Current-form,
  fusion-species and modulo mutations are detected by direct helpers; other
  mutations are detected by complete turns (and most also by helpers).
- Nine malformed full-turn traces reject with exit 2 after intact 0.
- Retained 24,000 before/after cases have no covered mismatch or loss. Coverage:
  plain 1,958→1,964; rich 1,929→1,932; switch 1,891 unchanged; summon 1,869→1,871;
  ITEMS 1,939/1,945/1,940/1,956→1,944/1,948/1,945/1,957;
  TRAINER 1,946/1,943/1,945/1,953→1,949/1,948/1,947/1,959.
  All 64,000 retained dedicated ability turns pass, including existing numerical probes.
- Battle build/boundaries and game-core host tests pass. Paired 200-run digest
  `e607cdcb`; paired 1,000-run normal/save digest `20d5c034`, 49,463 turns,
  1,000 ended each, zero stuck/crashed/timed out and 14,528 save round trips.

Exact-head host/3DS CI is recorded on the PR. Custom-core checks are excluded
by user instruction. No hardware, broader doubles, boss or complete core parity
is claimed. Boss scoring and the remaining battle-core flee differential are
explicit completion gates in REMAINING-BATTLE-CORE.md.
