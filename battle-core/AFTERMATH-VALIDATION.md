# Aftermath and Innards Out

Source pin `8555c08c823b856cbec4eb99ca84ea52a955836d`, locales
`270ed0a2938b2c3e2e34e7de940e84aecadf548b`, Node 22. This is a port of the pin,
not a deviation: no `PR_DEV_*` bit is involved.

Both are PostFaintAbAttrs that bypass faint. FaintPhase passes them the attacker
of the fainted Pokemon's last received attack (`getPokemonById(sourceId)`, maybe
gone) and that attack's move. A faint from indirect damage passes neither.

- Aftermath (`PostFaintContactDamageAbAttr(4)`): canApply needs a move and an
  attacker, the move's contact flag to apply (`doesFlagEffectApply`, so Long
  Reach counts), no Damp on the field (Damp is applied to
  every Pokemon in speed order, not simulated, so it shows its ability), and no
  Magic Guard on the attacker. Then the attacker takes `toDmgValue(maxHp / 4)`
  as indirect damage, added to its `damageTaken`.
- Innards Out (`PostFaintHPDamageAbAttr`): canApply is the base `true`, so the
  ability and its text always show, even after an indirect faint or against
  Magic Guard (the source's own comment). Apply does nothing without a move or an
  attacker, when the attacker is the holder's ally, or with Magic Guard; else the
  attacker takes the damage of `attacksReceived[0]` as indirect damage.

The port fills the FaintPhase params (`phases.c`) and adds the two cases
(`ab_attrs.c`). No generated data change. Before this, both abilities reported
unported and their turns were left out.

Knockout turns (`tools/build-aftermath-turn-source.py` over the unchanged turn
producer, replayed by the unchanged `build/turntest`): 2,000 turns per seed with
holders on both sides and in reserves (ability or passive, most at low HP),
attackers with Tackle, Quick Attack, U-turn, Body Slam, Water Gun or Seismic
Toss, some with Magic Guard, Long Reach or Damp, some at low HP.

| Seed | Covered | Pass | Fail | Texts identical | Aftermath recoil | Innards Out recoil | Attacker knocked out | Magic Guard | Long Reach | Damp | Non-contact | Main covered |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 777 | 1,993 | 1,993 | 0 | 1,993 | 208 | 174 | 125 | 5 | 7 | 5 | 10 | 1,551 |
| 9001 | 1,989 | 1,989 | 0 | 1,989 | 185 | 206 | 132 | 4 | 5 | 5 | 13 | 1,536 |
| 4242 | 1,988 | 1,988 | 0 | 1,988 | 189 | 202 | 121 | 5 | 5 | 8 | 8 | 1,550 |
| 31337 | 1,989 | 1,989 | 0 | 1,989 | 193 | 186 | 117 | 5 | 2 | 9 | 16 | 1,530 |

The last six columns before "Main covered" count covered turns: recoil texts,
turns where the recoil knocks the attacker out, and a holder knocked out with no
recoil text by a Magic Guard or Long Reach attacker's contact move, with Damp on
the field, or by a non-contact move. Main replays the same fixtures with 445 to
468 turns not covered. Four turns over the four seeds are normalised field by
field by the same checker rules as Zero to Hero's (two trainer-pick Levitate
records, two final-wave CLEAR battle types); any other difference fails.

Retained turn fixtures (plain, RICH, SWITCH, SUMMON, TRAINER at 777 and 31337),
main versus this branch: identical counts. The Zero to Hero turn fixture gains
one covered, passing turn per seed checked.

Ten compiled mutants are caught on all four seeds
(`tools/aftermath-mutations.py`): contact ignored, Damp ignored, Damp simulated
(no ability shown), Magic Guard ignored, an eighth instead of a quarter,
`damageTaken` not added, Innards Out shown only after a move, Innards Out
ignoring Magic Guard, half damage, and no attacker passed from FaintPhase.

`make -C game-core aftermath-host-test` (33 checks, in `make -C game-core test`):
real wild battles where the lead's Tackle or Vine Whip knocks out an Aftermath
foe and loses a quarter of its max HP with one text, Magical Leaf, Magic Guard
and Damp leave it unhurt and silent, and Innards Out returns the last hit's
damage. Normal game-core digests are unchanged (`361c1d0e` with and without
saves).

Reproduce:

```
battle-core/tools/aftermath-diff.sh /path/to/upstream 777   # then 9001, 4242, 31337
TURNTEST_LOCALES=/path/to/upstream/locales/en python3 battle-core/tools/aftermath-mutations.py \
  battle-core/build/aftermath-turns-777.txt battle-core/build/aftermath-turns-9001.txt \
  battle-core/build/aftermath-turns-4242.txt battle-core/build/aftermath-turns-31337.txt
make -C game-core aftermath-host-test
```

Not claimed: doubles (Innards Out's ally rule is ported but has no covered
turn), Protective Pads, emulator or hardware runs.
