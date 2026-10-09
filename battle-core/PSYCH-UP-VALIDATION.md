# Psych Up (CopyStatsAttr) validation — D01

Scope: one bounded leaf, `CopyStatsAttr`, reached by the actual `PSYCH_UP` factory in ordinary
wild singles turns. This record makes **no** claim of whole-move, ability, fusion, doubles or
full-game parity; a later critical-hit helper call is probed once per case, not all future turns.

| | |
|---|---|
| Port base (`origin/main`) | `f118549017c98ad53d31cc999a46096ff50230da` |
| Upstream (external checkout) | `8555c08c823b856cbec4eb99ca84ea52a955836d`, locales `270ed0a2938b2c3e2e34e7de940e84aecadf548b` |
| Node / pnpm deps | Node `v22.23.3`, `tools/setup-upstream.sh` |
| Host C toolchain | gcc 11.4.0 (Ubuntu 22.04), GNU Make 4.3 |
| 3DS hardware | not run |

## Source evidence (pinned upstream)

| Source | Behavior | Port |
|---|---|---|
| `src/data/moves/move.ts` `CopyStatsAttr.apply` | `super.apply` gate first; `user.setStatStage(s, target.getStatStage(s))` for each `BATTLE_STATS`; `target.getTag(CRIT_BOOST)` ? `user.addTag(CRIT_BOOST, 0, move.id)` : `user.removeTag(CRIT_BOOST)`; `updateInfo` (UI only); queues `moveTriggers:copiedStatChanges` {pokemonName: user, targetName: target} | `PR_AC_CopyStatsAttr` in `pr_apply_move_effect` (`src/move_effects.c`) |
| `MoveEffectAttr.apply/canApply` | `!(selfTarget ? user : target).isFainted()` (plus trigger filtering in `MoveEffectPhase`) | existing `effect_can_apply(ef, user, target)` and dispatcher trigger handling |
| `src/enums/stat.ts` `BATTLE_STATS` | ATK, DEF, SPATK, SPDEF, SPD, ACC, EVA | `stat_stages[0..6]` (`stat - 1`) |
| `src/field/pokemon.ts` `setStatStage` | direct assignment, clamped to [-6, 6]; no `StatStageChangePhase`, Simple or Contrary | direct assignment, same clamp; `stat_stage_change` not used |
| `Pokemon.addTag` | an existing tag only gets `onOverlap` (no metadata change), returns false | existing `pr_add_tag` (overlap path) |
| `Pokemon.removeTag` | `onRemove` (`battlerTags:critBoostOnRemove`) then removal | existing `pr_remove_tag` |
| `src/data/battler-tags.ts` `CritBoostTag` | `TURN_END`, turns 1, `critStages` 2 set in `onAdd` (`critBoostOnAdd`); `DRAGON_CHEER` is a different tag type and not copied | existing tag port |
| `PSYCH_UP` factory | `StatusMove(NORMAL, acc -1, pp 10, prio 0).ignoresSubstitute().attr(CopyStatsAttr)`; no `ignoresProtect` | generated `pr_moves`/`pr_attr_CopyStatsAttr`; `MOVE_ID_PSYCH_UP = 244` |
| `Pokemon.getCriticalHitResult/getCritStage` | HighCrit + `CritBoostTag.critStages` → chance `[24,8,2,1]`, battle-seed roll unless stage ≥ 3 | `pr_get_critical_hit_result` (`src/damage.c`) |

Before this change the dispatcher had no `CopyStatsAttr` case: Psych Up turns were reported
uncovered (`CopyStatsAttr`) by the established turn replays. Generated files are unchanged.

## Changed files / APIs

- `src/move_effects.c`: new generic `PR_AC_CopyStatsAttr` case (no move-ID special case). Uses
  only existing `effect_can_apply`, `pr_get_battler_tag`, `pr_add_tag`, `pr_remove_tag`,
  `pr_queue_msg`/`pr_msg2`/`pr_arg_pokemon`. No new API.
- `Makefile`: additive `$(BUILD)/psych-up-test` target (in `all`).
- New owned test files: `tests/psych-up.producer.test.ts` (upstream producer),
  `tests/psych-up-replay.inc` (copy of `tests/growth-replay.inc`; the only addition is the
  `CRITPROBE` record and its `pr_get_critical_hit_result` check), `tests/psych-up-test.c`
  (strict contract guard), `tools/psych-up-diff.sh` (wrapper), `tools/psych-up-malformed.sh`,
  `tools/psych-up-mutations.sh`. No established producer, comparator, count, seed or trace
  format is changed.

## Fixtures (actual source turn)

Each case is a real `TurnStartPhase` … `TurnEndPhase` turn: the holder (player on even cases,
enemy on odd, 1000/1000 per seed) commands Psych Up from its moveset, the other Pokemon Splash or
Protect. 24 states (`floor(i/2) % 24`) = 6 groups × 4 critical-tag modes:

- stages: all seven stats set on both Pokemon, unequal; every third state draws both from
  {-6, 0, +6}; accuracy and evasion always differ between user and target. Each of the 42
  (stat × side × {-6,0,+6}) cells is required non-empty (producer fails otherwise).
- critical tag: none / target only / user only (stale) / both; loaded tags have random
  `turnCount` 1–5, `sourceMove` Focus Energy/none/Psych Up, `critStages` 1–2, optional
  `sourceId`; a third of cases also carry `DRAGON_CHEER` on one side (never copied).
- groups: plain; user Simple; user Contrary; Substitute on the target (and on the user half
  the time; target sometimes Simple/Contrary) — Psych Up ignores it; target uses Protect (user
  Simple half the time) — protected, effect not reached; user Taunted or asleep — move refused.
- after the recorded state: `other.getCriticalHitResult(holder, SLASH)` (HighCrit +1, so a
  copied tag makes it certain with no roll and a missing/stale/1-stage tag changes the roll),
  recorded with both RNG states as `CRITPROBE` and replayed through `pr_get_critical_hit_result`.

Compared per case by the turn comparator: all MON lines (stats, stages, HP, status, ability),
SUMMON/TURN data, `BTAG` metadata (type, turns, source move/id, crit stages, substitute HP),
EFFECTIVE stats, MOVES (PP), HIST, MQ, phase TRACE, QUEUE levels/deferred/dynamic, END stop,
rendered TEXTS, and both RNG states. Stubs are the established producer ones (rendering,
tweens, party UI, `applyPartyExp`). No held items: MOD records are rejected by the guard, so
held-item non-transfer rests on the leaf not touching modifiers.

## Commands and results

Environment: `TURNTEST_LOCALES=<upstream>/locales/en`; upstream prepared by `tools/setup-upstream.sh`.

### Psych Up suite (new)

```sh
tools/psych-up-diff.sh <upstream>                     # N=2000, seeds 777 9001 4242 31337
tools/psych-up-malformed.sh <upstream> <trace>        # malformed-trace guard
tools/psych-up-mutations.sh <upstream> <trace>        # real-dispatcher mutations
```

`psych-up-diff.sh` checks the pinned SHA and Node 22, validates integer N/seeds, writes
`$BUILD/psych-up-<N>-<seed>.txt`, runs the producer then `psych-up-test`, and keeps each exit code.

| seed | requested / completed | covered / pass / fail / skip / throw | texts identical / differ / no text | crit probes pass / fail | parse / stat mismatches | exit |
|---|---|---|---|---|---|---|
| 777 | 2000 / 2000 | 2000 / 2000 / 0 / 0 / 0 | 2000 / 0 / 0 | 2000 / 0 | 0 / 0 | 0 |
| 9001 | 2000 / 2000 | 2000 / 2000 / 0 / 0 / 0 | 2000 / 0 / 0 | 2000 / 0 | 0 / 0 | 0 |
| 4242 | 2000 / 2000 | 2000 / 2000 / 0 / 0 / 0 | 2000 / 0 / 0 | 2000 / 0 | 0 / 0 | 0 |
| 31337 | 2000 / 2000 | 2000 / 2000 / 0 / 0 / 0 | 2000 / 0 / 0 | 2000 / 0 | 0 / 0 | 0 |

Source branch counts per seed (producer line): `CopyStatsAttr.apply` reached and applied 1344,
refused by its gate 0, not reached 656 (states 16–23: target Protect, user Taunted or asleep); states
84×16 + 82×8; sides 1000/1000; critical modes none/target/user/both 500 each; all 42
stat/side/{-6,0,+6} cells non-empty (≈280–380 each). Every covered turn ran `MoveEffectPhase`;
`PokemonAnimPhase` (Substitute) 336 per seed.

### Malformed traces (`psych-up-malformed.sh`, seed 777 trace)

Intact trace exit 0; all 26 variants refused with exit 2 (parse failure, not a mismatch):
missing header, header count too high / too low, zero cases, fractional count, other contract
header, missing footer, footer count mismatch, record after footer, second footer, last case
dropped, truncated last case, duplicate / out-of-order / malformed case id, missing / second
RUN, missing / second CRITPROBE, CRITPROBE move out of range, CRITPROBE attacker as defender, no
Psych Up command, stray comment, modifier record, unknown battler tag, END outside a case.
Script exit 0.

### Mutations of the real leaf (`psych-up-mutations.sh`, seed 777 trace)

Each mutant edits the `PR_AC_CopyStatsAttr` case of `src/move_effects.c` in a scratch copy,
rebuilds `psych-up-test` and replays the unchanged trace; the working tree is never modified.

| build | exit | covered pass / fail | texts differ | crit probes fail |
|---|---|---|---|---|
| intact | 0 | 2000 / 0 | 0 | 0 |
| omit accuracy/evasion copy (`s < 5`) | 1 | 656 / 1344 | 0 | 0 |
| stale critical tag (no `pr_remove_tag` branch) | 1 | 1673 / 327 | 327 | 224 |

The working tree is untouched by mutants; the 4×2000 suite was re-run on the final head after the mutation run (same results, see table above).

### Regressions: base (`f118549`) vs head on identical source traces

Source traces were produced once with the unchanged established producers (N=2000) and replayed
by the base build and the head build (`make -C battle-core`, exit 0). Hashes below. The only later change to `src/` is a comment edit in the new case.
Coverage gains are exactly the cases previously reported uncovered as `CopyStatsAttr`.

| suite (flags) | seed | base covered/fail | head covered/fail | notes |
|---|---|---|---|---|
| B plain | 777 | 1944 / 0 | 1947 / 0 | |
| B `TURN_RICH` | 777 | 1836 / 0 | 1839 / 0 | |
| B `TURN_SWITCH` | 777 | 1890 / 0 | 1890 / 0 | |
| B `TURN_SUMMON` | 777 | 1867 / 0 | 1867 / 0 | |
| items | 777/9001/4242/31337 | 1923/1934/1930/1943, 0 | 1928/1935/1932/1946, 0 | strict text no-text 1/1/2/1 retained: **FAILED** text completeness |
| `TURN_COND` | 777/9001/4242/31337 | 1768/1757/1771/1766, 0 | 1768/1757/1772/1767, 0 | |
| `TURN_WEATHER` | 777/9001/4242/31337 | 1957/1960/1956/1964, 0 | 1957/1960/1956/1966, 0 | |
| `TURN_TRAINER` | 777/9001/4242/31337 | 1937/1936/1932/1936; fail 0/0/1/0 | 1939/1936/1932/1938; fail 0/0/1/0 | seed 4242 CASE 1051 `SwitchSummonPhase:-1` vs `SwitchSummonPhase:` remains **FAILED** |
| `TURN_START TURN_TRAINER` | 777/9001/4242/31337 | 1938/1920/1933/1916; fail 0/0/2/1 | identical | pre-existing on base: 4242 CASE 1028/1625, 31337 CASE 784; unchanged, not credited |
| growth (`GROWTH_*`) | 777 | 2000 / 0 | 2000 / 0 | |

All replays: parse 0, stat mismatches 0, rendered-text differ 0. No established producer,
comparator, count, seed, tolerance or exclusion was changed.

### Contract H (base tree vs head)

```sh
make -j2 -C game-core test
game-core/build/gcplay --auto 1000 --jobs 8
game-core/build/gcplay --auto 1000 --jobs 8 --save-test
```

| | test | runs | ended / cleared / stuck / crashed / timed out | turns | digest normal = save |
|---|---|---|---|---|---|
| base | exit 0 (84 ok + all suites passed) | 1000 | 1000 / 0 / 0 / 0 / 0 | 50410 | `75bb4db1` = `75bb4db1` |
| head | exit 0 (same) | 1000 | 1000 / 0 / 0 / 0 / 0 | 50406 | `66edb535` = `66edb535` |

The digest/turn change is expected: the base runs list `CopyStatsAttr 2 runs 3` among
unported attributes and the head runs no longer do — those two runs now execute Psych Up per
source, which changes later turns. Normal and save runs match on each build. 0 clears remains.

## Boundaries and exclusions

- Bounded to wild singles, one Psych Up turn per case, holder on either side. Not covered:
  doubles, trainer/fusion/boss, Psych Up called by other moves (Metronome etc.), other
  CopyStatsAttr users (Psych Up is the only `.attr(CopyStatsAttr)` in the pinned `move.ts`), held items (none loaded),
  `updateInfo` (UI). The critical probe checks one later `getCriticalHitResult` call per case,
  not all future turns.
- RNG: both generator states compared after the turn and after the probe; Psych Up consumes no
  RNG itself. Queue: phase trace, queue levels and END stop compared. Text: every rendered
  message compared (`copiedStatChanges`, `critBoostOnRemove`, protect/taunt/sleep texts).
- 3DS hardware not run. CI host/3DS jobs on the PR head are the remaining external check.

Trace hashes (sha256):

```
5c3d45e6282fd657663d800f28f2d2c01dca8f048005418bb43300a0fbd8faf4  cond-31337.txt
17dc615e9c09b97d0dcfc6203f73def6570640ad09409a0565266b28e3937b5c  cond-4242.txt
6b85a25bc69592b82f36fbc8e1cbca67abc8297696bbcb484599040624276b5b  cond-777.txt
06d2dfcbf991645fa65e491347812005ea57fc66889c54e3f7328c2c9b907123  cond-9001.txt
516ed3c1e07185f2f52ef8bf3d76ae04e78adea4e4e1a5f8055dc7ab929be7d2  gtrainer-31337.txt
16bfdf3cfb1db538d159b1fb5cd53c2c12aecc201798028f6b93c2296fd495bf  gtrainer-4242.txt
940261946c938e82a406a908865f6cfbc11895d062a891a966f389feef86f13a  gtrainer-777.txt
e87daa5a1090bda2ce3d19c39a67ff2fc5a86a8183f6eef4bc44819f5bda46c9  gtrainer-9001.txt
3fec8e3cea0c802e52d18c0a602b3c8dc164f8d1021a5498b6555035f61bc5b7  items-31337.txt
c9123b4033d51b9f49ebbed54aca54de20aff495a34d2118a03a4753289486d9  items-4242.txt
038de3b4fe6041a78a0a20fd0dab4b553034207b4643791baae4645c0b11232c  items-777.txt
94fb096777784eab3c4995397f4ddd2df1131ef4acb621be9036735bcf288d1f  items-9001.txt
8026b7fc7a3c63ba31fdaf3ea5569956dbf3fd51d42351cd055c4c6c5ef39a7f  plain-777.txt
f960668cbf6ef096095f8d306c4cb4b28a056a6013944320e47e970d296a61ad  rich-777.txt
50bf8ca22fd606df01e5a4aaa7f479a36ad9ccf78c21e74fdb68f7162201f664  summon-777.txt
0d36f9e5e691d4c71309519b6c8ec85f171507c6714f06da76e89ecb82d2c0f2  switch-777.txt
74aa9a421101bc4231bec1447df30985073739652ec0e8ac11234aa954233148  trainer-31337.txt
b02c31ccb2defc97c7568612a4be0b1ee6c3296c3633fb06b493da879b00ed8f  trainer-4242.txt
7d94d918d690d10981f43d8b949487193a2d77dfc62002acc51a719d4210023d  trainer-777.txt
7c97a33afe29ae04d56089628246a22fa6d42f4011a4500ecb8c1ce720bf196e  trainer-9001.txt
0ae21e2e75a2692eba7553ffa13b0736b501a5d032085e62848ead1edbe38c2f  weather-31337.txt
86d99fa9438e6e6b315ed975bee0ddc76bf9e72521b7479860e5ae1b202ab0ec  weather-4242.txt
0fa62a2a5ae56a134b825a73e7b889c60af632f07d6bd9db871f589af54c345b  weather-777.txt
9f0dae85a8a1f22a74f3c420d52d5d296b66f6208b36124b34c132ea1717cfdf  weather-9001.txt
904b79a3590aa59082511c2af9a52bc225aeca3a65517789bb64709f55245097  growth-777.gtxt
c1dfa4632468401c6300c64b268c7628b077e68c6ec098d2e3ad668de827ac08  psych-up-2000-31337.txt
2b8f75ca81a6baec413b26b1c0e07f41bdb196f42cd08a5e8d1794a3bf10fdf2  psych-up-2000-4242.txt
00bb3f5fc70c99d84c1d0cc81369f383d81d667a60a308093e1cb19216d9a8d4  psych-up-2000-777.txt
54bb8bb621686960fda430abd7c5511336dc5b4619bd7fc86c980f46b5d4c9b7  psych-up-2000-9001.txt
```
