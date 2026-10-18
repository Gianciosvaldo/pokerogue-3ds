# Tera Shift

Source pin `8555c08c823b856cbec4eb99ca84ea52a955836d`, locales
`270ed0a2938b2c3e2e34e7de940e84aecadf548b`, Node 22. A port of the pin, not a
deviation: no `PR_DEV_*` bit is involved.

Tera Shift (307) is `PostSummonFormChangeAbAttr(p => p.getFormKey() ? 0 : 1)`
for Terapagos, whose form changes are quiet and have no conditions:

- `SpeciesFormChangeAbilityTrigger`: "" -> "terastal". Its formFunc is the same
  closure (`55c07decbbff`) as Hunger Switch's, so the port reuses `hunger_toggle`.
- `SpeciesFormChangeLapseTeraTrigger`: "stellar" -> "terastal", when Tera ends.
- "terastal" -> "stellar" belongs to `SpeciesFormChangeTeraTrigger` (TeraPhase),
  which is still unported in C and stays so here.

The port adds the closure row (`closures_abilities.c`) and a Terapagos branch in
`form_change_with_delay` (`phases.c`). With no ability index known
(`ability_lookup_known` unset) a Terapagos whose ability came from a form slot
reports `PR_FEAT_FORM_CHANGE` unported, since the new form's slot (Tera Shell, 308)
cannot be looked up; Terapagos with an index or an override take it.

Harness addition (`tests/turntest.c`, additive): when a MON record carries
`aidx=` and `cab=` (written by the Tera Shift overlay for player Terapagos only,
non-fused), the replay sets `ability_index`, `custom_ability` and
`ability_lookup_known`, checks that the lookup reproduces `abil`/`babil`, and
dumps the two fields back. Records without them replay exactly as before.

Turns (`tools/build-tera-shift-turn-source.py` over the turn producer, replayed by
`build/turntest`, checked by `tools/check-tera-shift-turns.py`): 2,000 turns per
seed with Terapagos (Normal, Terastal, Stellar) as player lead, bench and reserve
and as enemy (summonData ability or passive), some Terastallized, some at low HP,
with U-turn, Baton Pass or Tackle switching and knockouts.

| Seed | Covered | Pass | Fail | Normal->Terastal | Player takes Tera Shell | Enemy changes form | Shown, no change | Stellar->Terastal lapse | Knocked out |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 777 | 1,999 | 1,999 | 0 | 497 | 305 | 54 | 24 | 73 | 115 |
| 9001 | 1,998 | 1,998 | 0 | 507 | 309 | 55 | 23 | 80 | 120 |
| 4242 | 1,998 | 1,998 | 0 | 451 | 298 | 40 | 31 | 77 | 110 |
| 31337 | 1,996 | 1,996 | 0 | 481 | 296 | 61 | 24 | 91 | 114 |

Texts are identical on every covered turn. Main covers 1,151, 1,110, 1,168 and
1,107 of the same turns (replaying them without the `aidx`/`cab` fields).
Normalised field by field: the trainer-pick Levitate (26) records and final-wave
CLEAR battle types, as in Zero to Hero's checker; anything else fails.

Retained fixtures (plain, RICH, SWITCH, SUMMON, TRAINER, Zero to Hero turns and
Aftermath turns, all four seeds): covered/pass/fail lines identical between main
and this branch.

Seven compiled mutants are caught on all four seeds
(`tools/tera-shift-mutations.py`): closure missing, formFunc inverted, route
removed, lapse dropped, ability trigger from any form, lapse from any form, and
changing to Stellar.

`make -C game-core tera-shift-host-test` (9 checks, in `make -C game-core test`):
a real run whose Terapagos lead enters as Terastal, quietly, with Tera Shell; a
Bulbasaur lead keeps its form. The test fails with the route removed.

Reproduce: `tools/tera-shift-diff.sh UPSTREAM_CHECKOUT SEED`.
