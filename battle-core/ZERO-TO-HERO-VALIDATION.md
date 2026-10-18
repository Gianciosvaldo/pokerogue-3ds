# Zero to Hero (Palafin)

Source pin `8555c08c823b856cbec4eb99ca84ea52a955836d`, locales
`270ed0a2938b2c3e2e34e7de940e84aecadf548b`, Node 22.

The pin gives Zero to Hero four callbacks: PostBattleInit requests form 0,
PreSwitchOut requests form 1 unless the holder has fainted (then its own form),
and the PostSummon message is a conditional attribute that needs form other than
0, the holder on the field, and `zeroToHeroFormChangeMessage.get(p) !==
p.battleData`; its message function sets that WeakMap entry. The ability is
uncopiable, unreplaceable, unsuppressable, and refuses Transform and fusion
(existing flags). Palafin's two form changes are quiet ability triggers with no
conditions.

The port registers the three closures (`ability:278`), dispatches
PreSwitchOutFormChangeAbAttr (shown, not simulated), routes Palafin's ability
trigger to the other form, and keeps the WeakMap as `zero_to_hero_shown`,
cleared with the rest of battleData. The flag is not saved: a loaded Pokemon gets
a new battleData object at the pin, so the mark is lost there too. The form is
saved as before. No generated data change.

At the pin an enemy Palafin never becomes Hero by switching out:
QuietFormChangePhase ends early for an off-field enemy without changing form
(the source's own TODO asks whether that is intended). The port reproduces this;
49 to 65 enemy switch-outs per seed show the ability and keep form 0.

Boundary matrix (`tests/zero-to-hero.producer.test.ts`, `tests/zero-to-hero-test.c`):
2,016 cases per seed, 24 holder states (both forms, fainted, passive, suppressed,
fused, transformed, other species, Battle Bond, overridden ability, switching
out, off field, FAINT status, primary and passive, level 1, 1 HP) by seven
operations (switch-out, simulated switch-out, PostBattleInit, PostSummon once,
twice, across a battleData reset, simulated then real) on both sides. Seeds 777,
9001, 4242 and 31337: 2,016 exact each, 0 failures. Main before this change:
504 of 2,016 exact at seed 777.

Switch-out turns (`tools/build-zero-to-hero-turn-source.py` over the unchanged
turn producer, replayed by the unchanged `build/turntest`): 2,000 turns per seed
with Palafin on both sides and in reserves, natural, passive or overridden
ability, command switches, U-turn, Baton Pass and knockouts.

| Seed | Covered | Pass | Fail | Texts identical | Zero to Hero switch-outs | U-turn/Baton Pass | Enemy switch-outs | Entry messages | Palafin KO |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 777 | 1,996 | 1,996 | 0 | 1,996 | 496 | 40 | 49 | 502 | 233 |
| 9001 | 1,997 | 1,997 | 0 | 1,997 | 527 | 63 | 65 | 541 | 253 |
| 4242 | 1,999 | 1,999 | 0 | 1,999 | 523 | 58 | 55 | 508 | 210 |
| 31337 | 1,994 | 1,994 | 0 | 1,994 | 510 | 45 | 50 | 485 | 222 |

Seven turns differ in one field outside Zero to Hero; the checker rewrites that
field only, replays again and fails on anything else. Five are Levitate entries
in a player Pokemon's abilitiesApplied written by the source trainer's
getMatchupScore when it picks its next Pokemon (the replay receives the pick
through EPICK); two are BattleType.CLEAR set by a final-wave VictoryPhase, which
game-core sets in the port. The overlay also restores the battle type each case,
because the reused source battle keeps CLEAR after such a case and the pinned
Roar guard reads `currentBattle.trainer` where the port reads the battle type.

Retained turn fixtures (plain, RICH, SWITCH, SUMMON, TRAINER, 2,000 each, four
seeds), main versus this branch: no covered case lost, seven more covered (SUMMON
777 +2, 4242 +2, 31337 +1; TRAINER unchanged), all pass; the single SWITCH
failure at 9001, 4242 and 31337 is the same case on main.

Ten compiled mutants are caught (`tools/zero-to-hero-mutations.py`): faint
ignored, switch-out gate dropped, simulated form change, Palafin route removed,
Hero at PostBattleInit, mark not set, mark not cleared, message ignoring the
mark, ignoring the field, and shown in Zero form. The mark is checked by the
boundary matrix and the host test; turn fixtures cannot carry it.

`make -C game-core zero-to-hero-host-test` (22 checks, in `make -C game-core test`):
a real switch-out turns Palafin Hero with higher Attack and no message, the next
entry shows one message, a second entry none; the mark survives to the next
legal save point, a saved and loaded run keeps Hero without the mark, and the new
biome's arena reset returns Zero form and clears the mark. Normal game-core
digests are unchanged (`361c1d0e` with and without saves).

Reproduce:

```
battle-core/tools/zero-to-hero-diff.sh /path/to/upstream 777   # then 9001, 4242, 31337
TURNTEST_LOCALES=/path/to/upstream/locales/en python3 battle-core/tools/zero-to-hero-mutations.py \
  battle-core/build/zero-to-hero-777.txt battle-core/build/zero-to-hero-turns-777.txt
make -C game-core zero-to-hero-host-test
```

Not claimed: doubles, Imposter/Transform copying a Hero Palafin beyond the
refusal flags, emulator or hardware runs.
