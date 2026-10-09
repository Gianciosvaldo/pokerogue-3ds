# Growth actual move-effect proof (Card 105, 2026-10-09)

Growth previously reached the unknown-effect dispatcher and reported
`GrowthStatStageChangeAttr`; it had no C handler. The bounded repair dispatches
its generated fields through the existing stat-change pipeline. After the parent
canApply/condition/chance gates, Growth's level calculation adds one to its base
stage change for effective SUNNY or HARSH_SUN. The existing effective-weather
helper applies the user's weather override before field suppression. No weather,
stat-stage phase, Simple, Contrary, RNG or text formula is replaced.

Source is `pagefaultgames/pokerogue` pinned at
`8555c08c823b856cbec4eb99ca84ea52a955836d`, Node 22.23.3. The owning source
boundaries are `move.ts` Growth factory, `GrowthStatStageChangeAttr.getLevels`
(line 4291), its parent `StatStageChangeAttr.apply` (line 3922), and
`weather.ts:getEffectiveWeatherForMove` (line 234). The source getLevels observer
delegates to the original method. The factory sets Attack/Sp. Atk, base +1,
self-targeted, guaranteed move effect. The inherited pipeline queues and runs
the real StatStageChangePhase.

## Source turn proof

`tests/growth.producer.test.ts` runs real MoveEffectPhase and complete recorded
turns. `growth-replay.inc` copies the established turn comparator, with only the
already established additive five-stat EFFECTIVE input/output transport used by
the trainer proof. Its comparison rules, tolerances and unsupported admission
remain unchanged. Fresh actual getEffectiveStat snapshots are compared before
and after turns, alongside HP, stages, abilities, PP, histories, arena timers,
phase and pending queue, RNG and exact displayed text.

Four seeds **777, 9001, 4242, 31337**, each **2000** requested cases:

| Evidence per seed | Before handler | Final handler |
|---|---:|---:|
| Covered / exact turns | 132 / 132 | 2000 / 2000 |
| Unsupported Growth turns | 1868 | 0 |
| Parse / numerical input mismatches | 0 / 0 | 0 / 0 |
| Full turn failures / text differences | 0 / 0 | 0 / 0 |

Each seed observes **1072 actual +1 levels**, **796 actual +2 levels** and
**132 Taunt-refused turns**, with **1000 player / 1000 enemy users**. All final
**8000 turns and texts are exact**. Thirty bounded states cover no weather,
SUNNY, HARSH_SUN, RAIN, SANDSTORM and SNOW; opposing Cloud Nine suppression;
Mega Sol overriding weather despite opposing Air Lock; a suppressed Cloud Nine
under SUNNY; the user's own Cloud Nine; Simple and Contrary including sunny /
harsh-sun boosts; asymmetric +5/+6 clamps, both +6 caps, and Contrary at -6/-5.
Taunt cases include a Mega Sol user: refused Growth cannot activate its move
level calculation. Loaded tags are constructed using the actual source addTag;
initial construction announcements are cleared before the recorded empty-queue
turn boundary, rather than treated as events in that turn.

This proves these loaded ordinary singles states and their actual callers. It
does not claim all abilities, doubles, fusion, challenge policy, weather creation
policy, AI scoring, or whole-run upstream parity. The source constructors and
stat-stage handlers supply behavior; no fixture formula supplies expected levels.

## Mutation and malformed-input gates

Five mutations replace actual C runtime code and replay unchanged seed777:

| Actual caller mutation | Covered / full turn failures | Exit |
|---|---:|---:|
| Remove Growth dispatch | 132 / 0; 1868 unsupported | 1 |
| Omit SUNNY extra stage | 2000 / 596 | 1 |
| Omit HARSH_SUN extra stage | 2000 / 134 | 1 |
| Read raw arena weather, bypass effective-weather caller | 2000 / 664 | 1 |
| Queue only Attack, omit Sp. Atk at actual dispatcher | 2000 / 1802 | 1 |

The first mutation fails complete requested coverage; the other four retain
all 2000 covered turns and fail actual state/text comparisons. Production code
is never changed while these separate copied-object mutants run.

The owning wrapper requires integer request/version, sequential case IDs, one
Growth command per input, RUN/END pairing, exact completion count and no trailing
records or modifiers. **17 malformed traces exit 2**: missing header/footer,
wrong request/version/footer, fractional or extra header, extra footer,
post-footer data, missing END, duplicate case, unexpected MOD, missing Growth
command, invalid RUN, and overflowing header/footer/case counts. Fractional source request `2000.5` exits 1 before
fixture production. No exception or incomplete case receives pass credit.

## Retained checks and explicit historical failures

All **72 established battle pairs** and **56 established game pairs** replay
unchanged fixtures with unchanged drivers against fresh main
`5fce401cc23d0369b146967e2185dbd8b84dcaba` and this implementation. The battle
pairs retain original plain/rich/switch/summon/ITEMS/COND, Wide Lens, Reviver,
Multi Lens, Golden Punch, Soul Dew, type boosters, critical items, effective-stat
items, four legacy raw damage seeds, WEATHER/ARENA/forms/Field Effect, temporary
boosters, and Card103 trainer proof.

**58 battle outputs are byte-identical.** The remaining 14 add **27 newly
covered exact turns**, solely by implementing formerly flagged Growth:

| Suite / seed | Covered before → after | New failures |
|---|---:|---:|
| plain / 777 | 1942 → 1944 | 0 |
| rich / 777 | 1831 → 1832 | 0 |
| ITEMS / 777 | 1922 → 1923 | 0 |
| ITEMS / 9001 | 1932 → 1934 | 0 |
| ITEMS / 4242 | 1929 → 1930 | 0 |
| ITEMS / 31337 | 1939 → 1943 | 0 |
| COND / 777 | 1767 → 1768 | 0 |
| COND / 9001 | 1756 → 1757 | 0 |
| COND / 4242 | 1770 → 1771 | 0 |
| WEATHER / 4242 | 1955 → 1956 | 0 |
| WEATHER / 31337 | 1963 → 1964 | 0 |
| ARENA / 777 | 1852 → 1856 | 0 |
| ARENA / 9001 | 1842 → 1845 | 0 |
| ARENA / 4242 | 1832 → 1836 | 0 |

Coverage never decreases; all covered state comparisons pass. **Historical
ITEMS text-completeness checks remain FAILED**, with missing-text counts
777=1, 9001=1, 4242=2, 31337=1 on both baseline and final. These checks receive
no pass credit, even though the original comparator returns success for their
covered state checks. No old producer, comparator, assertion, tolerance, count,
exclusion or guard was altered to hide them. The separate historical trainer
pending-queue transport failure is documented in
[Card103 validation](TRAINER-SWITCH-VALIDATION.md); this task does not repair or
claim completion of that transport contract.

All **56 game source outputs are byte-identical**, including the full retained
item bridges, Temp raw/caller, Wide raw/acquisition, and Multi raw/acquisition.

## Host integration

Fresh baseline and final `make -j2 -C game-core test` pass. The original host
chooser runs 1000 normal and 1000 saved simulations, jobs 8, without changes to
its chooser, completion rules, flags or digest:

| Host measure | Before | After |
|---|---:|---:|
| Ended / cleared / stuck / crashed / timed out, each path | 1000 / 0 / 0 / 0 / 0 | 1000 / 0 / 0 / 0 / 0 |
| Turns, each path | 50394 | 50356 |
| Normal and saved digest | 5b6c9ee2 / 5b6c9ee2 | e789ef81 / e789ef81 |
| Saved round trips / largest bytes | 14679 / 11472 | 14686 / 11472 |
| Original 200-run normal / saved test digest | bc0fbf1c / bc0fbf1c | 6741e0a8 / 6741e0a8 |

Growth was flagged **177 times in 112 host runs** before; its flag disappears
with the only runtime change, the source-proved handler. Real stage effects
change subsequent host outcomes, so trajectories and digests legitimately
change. Source turns above independently prove the resulting +1/+2, caps and
ability interactions; host digest agreement proves host save/resume consistency,
not upstream run parity or hardware playability.

Existing host stand-ins remain visible (runs / occurrences):

| Stand-in | Before | After |
|---|---:|---:|
| Last four level moves | 1000 / 15094 | 1000 / 15096 |
| Doubles played as singles | 769 / 1588 | 767 / 1583 |
| Evolution condition unchecked | 2 / 3 | 2 / 3 |
| Biome weather roll | 1000 / 1902 | 1000 / 1901 |
| Enemy generation stand-in | 1000 / 2301 | 1000 / 2305 |
| Post-battle abilities / Pay Day / item lapse | 167 / 1380 | 166 / 1375 |
| Trainer title omitted | 314 / 424 | 311 / 421 |
| Reward without effect | 148 / 185 | 145 / 181 |

## Reproduction

```sh
. /workspace/shared/activate-parity.sh
for seed in 777 9001 4242 31337; do
  battle-core/tools/growthdiff.sh /path/to/pinned/upstream 2000 "$seed"
done
make -j2 -C game-core test
game-core/build/gcplay --auto 1000 --jobs 8
game-core/build/gcplay --auto 1000 --jobs 8 --save-test
```

The tool rejects a wrong source pin or Node version. Trace data, source producer
logs, before/final replay logs, copied-object mutants, malformed guards, retained
pair summaries, host outputs and PR/CI metadata are saved under
`/workspace/validation/lane-a/eight/growth/`; fixture dumps and binaries are not
committed. CI host tests and 3DS cross-compilation are separate final gates.
