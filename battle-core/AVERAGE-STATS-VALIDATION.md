# D02: Power Split / Guard Split evidence and validation

Implementation/host validation complete; exact-head CI pending. Base `3f75a3940ed6ea34fa7cb252a44e6461d637902a` (same battle code as `f486ae30e2a6ea21142b24cdf15d0de27e56567f`); pinned upstream
`8555c08c823b856cbec4eb99ca84ea52a955836d`, Node 22.23.3. Duplicate source/PR
check found generated AverageStatsAttr but no dispatcher leaf or dedicated proof.
Scope is battle-core, ordinary singles; no generated files or established checks change.

Evidence collected before production edits:

| Source / symbol | Relevant lines and intended C behavior | Fixture |
|---|---|---|
| `src/data/moves/move.ts` AverageStatsAttr.apply | 8814–8828: parent gate, payload-selected stats, floor of two getStat(false) values, setStat(false) on both, payload message | Power/Guard Split, both sides, odd/even sums, unequal overrides |
| `src/field/pokemon.ts` getStat/setStat | 1360–1385: zero override uses base; <=0 writes ignored; positive override is summonData only | zero/fractional overrides, unchanged base and stages |
| `Pokemon.resetSummonData`; C `pr_reset_summon_data` | C phases.c:2501 clears all six summon_stats, ordinary switching uses this API | ordinary switch reset control |
| generated `pr_attr_AverageStatsAttr` | `stats` and `msgKey` carry factory behavior, not move-ID dispatch | both identities and mutation per move |

Baseline H at #194/#195: host test exit0; normal/save each 1000 ended,
0 clears/stuck/crashed/timed-out, 50406 turns, matching 66edb535, 14701 saves.
ITEMS missing texts and trainer metadata failure remain known failures.

## Independent caller proof

Only the generated AverageStatsAttr dispatcher leaf was added. Payload stats and
message choose the behavior; no production move-ID dispatch. The source
observer delegates actual AverageStatsAttr.apply; the actual MovePhase and
MoveEffectPhase run independently before replay. Rendering/tween/party-selection,
party experience and asset refresh are bounded headless stubs inherited from the
owning turn fixture. Ordinary singles, no fusion/Tera/items or broad forms.
Substitute/Protect/Taunt/sleep remain requested cases and compare their refusals.
Neither base stats nor stages are replaced; positive fractional overrides are
averaged and floored into summon storage only.

Four seeds 777/9001/4242/31337: requested/completed/covered/identical **2000 each**,
zero skipped/throws/parse/stat/state/text differences/untranslated messages.
Each has Power/Guard 1000/1000; applied 502/502; parent leaf returns false 0/0;
not reached because turn gates 498/498; repeated actual splits252; both sides1000.
Eight states252/252/252/252/248/248/248/248. Every seed and identity observes
nonzero even, odd and fractional input sums. Subsequent actual getAttackDamage
versus pr_get_attack_damage also compares 2000/2000 each with matching RNG.
Additional actual ordinary switches:128 per seed, all512 full state/text/damage
comparisons identical, including bench order and cleared temporary overrides.
These are additional caller comparisons, not part of the requested8000.

Reproduction (source checkout outside this repository):

```sh
. /workspace/shared/activate-parity.sh
battle-core/tools/average-stats-diff.sh /workspace/shared/upstream-lane-a 2000 '777 9001 4242 31337'
AVERAGE_STATS_SWITCH=1 battle-core/tools/average-stats-diff.sh /workspace/shared/upstream-lane-a 128 '777 9001 4242 31337'
TURNTEST_LOCALES=/workspace/shared/upstream-lane-a/locales/en python3 battle-core/tools/average-stats-malformed.py battle-core/build/average-stats-test <trace-2000>
TURNTEST_LOCALES=/workspace/shared/upstream-lane-a/locales/en python3 battle-core/tools/average-stats-mutations.py <trace-2000>
```

The wrapper enforces source SHA/Node22, integer bounds/nonempty unique seed list,
unique retained directories and original source/replay status. The owning guard
requires ordered cases, complete before/after records, two distinct commands,
TRACE/TEXTS/QUEUE/damage probe, declared completed count/footer, legal numeric
ranges, finite stat lists and no trailing records.42 malformed variants exit2;
semantic mutants exit1, intact0. Existing Growth producer/comparator remain
unchanged; six anchored generated overlays add the owning damage probe and
explicit zero override observability. Anchor drift fails the build.

| Real dispatcher mutation, seed777 N2000 | State failures | Damage probe failures | Exit |
|---|---:|---:|---:|
| Power Split ceil instead of floor |281|77|1|
| Guard Split ceil instead of floor |292|58|1|
| Write permanent instead of summon stats |1004|420|1|
| Intact after mutations |0|0|0|

Mutations ran on the earlier fully passing corpus before the final additional
fractional fixtures. Final clean replays use the final8000+512 source corpus.
Development failures were retained: initial32-case missing-zero-override print
and switch-probe stale storage lookup. Neither is counted as a passing run.
The final probe reads the current battle field after switching.

## Retained regressions

H baseline was captured before production edits. B and broader source corpora
were captured afterward and replayed against the isolated, immutable pre-edit
`f486ae3` snapshot and final library; this is a reconstructed code baseline,
not a claim that those source logs were captured before the first edit.
Same source traces are used for both; no established producer/comparator changed.
All rows request2000; coverage can increase as the formerly missing class runs.

| Mode/seed | Baseline covered/pass/fail | Final covered/pass/fail | Final missing text |
|---|---|---|---:|
|cond-31337|1767/1767/0|1768/1768/0|0|
|cond-4242|1772/1772/0|1775/1775/0|0|
|cond-777|1768/1768/0|1768/1768/0|0|
|cond-9001|1757/1757/0|1759/1759/0|0|
|heal-31337|1947/1947/0|1948/1948/0|0|
|heal-4242|1923/1923/0|1924/1924/0|0|
|heal-777|1935/1935/0|1935/1935/0|0|
|heal-9001|1929/1929/0|1930/1930/0|0|
|items-31337|1946/1946/0|1948/1948/0|1|
|items-4242|1932/1932/0|1933/1933/0|2|
|items-777|1928/1928/0|1930/1930/0|1|
|items-9001|1935/1935/0|1937/1937/0|1|
|plain-777|1947/1947/0|1948/1948/0|0|
|rich-777|1839/1839/0|1841/1841/0|0|
|stat-31337|1893/1893/0|1893/1893/0|0|
|stat-4242|1892/1892/0|1892/1892/0|0|
|stat-777|1884/1884/0|1884/1884/0|0|
|stat-9001|1890/1890/0|1890/1890/0|0|
|status-31337|1973/1973/0|1974/1974/0|0|
|status-4242|1971/1971/0|1971/1971/0|0|
|status-777|1965/1965/0|1966/1966/0|0|
|status-9001|1971/1971/0|1971/1971/0|0|
|summon-777|1867/1867/0|1867/1867/0|0|
|switch-777|1890/1890/0|1890/1890/0|0|
|trainer-31337|1938/1938/0|1941/1941/0|0|
|trainer-4242|1932/1931/1|1935/1934/1|0|
|trainer-777|1939/1939/0|1940/1940/0|0|
|trainer-9001|1936/1936/0|1938/1938/0|0|
|weather-31337|1966/1966/0|1966/1966/0|0|
|weather-4242|1956/1956/0|1957/1957/0|0|
|weather-777|1957/1957/0|1957/1957/0|0|
|weather-9001|1960/1960/0|1960/1960/0|0|

Trainer4242 CASE1051 remains FAILED (optional queue index transport), exit1;
ITEMS strict text completeness remains FAILED1/1/2/1 despite state exit0.
These failures were not repaired, excluded or reclassified. Legacy damage
coverage2000/2000/2000/1999 and FusionFlare throw remain historical failures;
that unrelated source damage generation was not rerun for this bounded leaf.
Focused statitemdiff/trainerswitchdiff/growthdiff:2000 × all four seeds each,
all24000 source comparisons pass with unchanged tests. Psych Up's final corpus
replays8000/8000 on this build. Focused source traces also pass against the
immutable pre-edit baseline binaries.

H final: host test passes; normal/save1000 runs each end1000, clear0, stuck0,
crashed0,timed-out0,50406 turns, paired digest176e408e,14701 save round trips,
maximum11472 bytes. Baseline paired66edb535; the digest includes summon state,
so newly executing AverageStatsAttr legitimately changes it. The200-run host
pair remains4a43b96f. This is host consistency, not a legal Classic completion.
No hardware execution, no whole-run or natural doubles claim.

Evidence retained under `/workspace/work/batch-2026-10-11/` (d02 logs,
regressions and mutation reports), and ignored battle build directories
`average-stats-evidence.wsJGKs` (final8000), `average-stats-evidence.3bFXXI`
(switch512), `average-stats-evidence.I7wRBS` (earlier mutation corpus).

Regression SHA256 manifest hash: `a3b295c099f2c52106f4899f415fd06a9222958c5f7d6afc1de666f1c2917230`.

| Final source trace | SHA256 |
|---|---|
|average-stats-2000-31337.txt|`f5475a84e17059f37554ec5c79028c49687e26b4dac307e77fbcdaa48ea6bc3c`|
|average-stats-2000-4242.txt|`7d55b56a01cb07efde4b90a4932e32816ab5a83f1388c230a8384f899ad5e184`|
|average-stats-2000-777.txt|`1f0dbf4108de35f3a7c3b89e0ba62727fa761235fae3d4c640bcb3837da60669`|
|average-stats-2000-9001.txt|`edee9c902f4734e58ac58d0dcdd924987ae1466f6bc3040dcc318495fd7cd6f4`|
|average-stats-128-31337.txt|`0d542a5cba1f7699e2ef411a08edea464c96bd756d14dfee3850b3ed5abd5f92`|
|average-stats-128-4242.txt|`4d670b38eed369fd1a956c68e2b21e57d66cbba9a6647e6a871727cb43b12948`|
|average-stats-128-777.txt|`f5cf8690514e760afd510e4e4b346a98d50e4fa2d007b06d851320755feaed19`|
|average-stats-128-9001.txt|`acf258ec74712f1788fb87f4bdb473dfb0b8a42b7bb0b386125112b27350412f`|

Producer SHA256: `4526e0d93a29d97ea65919e835ae620b4161997e572d3d54c05a61d9b2869ab2`. GCC14.2.0, Node22.23.3.
