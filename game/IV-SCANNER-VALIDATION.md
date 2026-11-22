# Wild encounter IV Scanner

Owning an IV Scanner now prompts at the start of ordinary wild encounters. Yes
opens the six-stat IV assessment; No continues. Both live foes in doubles are
processed once, and A, B or touch closes an assessment. This is presentation
only: it does not submit a battle command, change an IV, or grant a dex unlock.

The pinned ScanIvsPhase and BattleMessageUiHandler.promptIvs/getIvDescriptor at
upstream 8555c08c823b856cbec4eb99ca84ea52a955836d use these ratings: Best (31),
Fantastic (30), Very Good (21–29), Pretty Good (11–20), Decent (1–10), No Good (0).
The port uses the same English locale keys. Ratings above the baby/root species'
saved IVs are green, or the native perfect-IV style for 31. The existing level-up
stat window carries the assessment, with no battle-core or save schema changes.

The app's real run driver is exercised with an injected canonical IV Scanner:
100 runs include 749 accepted scans, 356 declines and 749 assessment windows.
Both surrounding battle RNG streams stay unchanged. Window payloads are checked,
commands resume and the existing model reports no stuck runs or violations.
The scanner fixture is added to normal app tests; the original 10,000-run and
all custom/save tests pass without changing coverage floors. Their digests change
because the test player now makes additional Yes/No choices when it owns a
scanner; scanning itself preserves the game RNG streams. The actual 3DS app builds and passes the
strong 524,288-byte stack/frame check (2,259 reports, largest 45,056 bytes).

```sh
make -C game/tests runtest
RUNTEST_SCANNER=1 game/tests/build/runtest 100 0
JOBS=2 game/tests/run_host_tests.sh
```

Scope: canonical positive IV Scanner stock on ordinary wild encounter entry,
including loaded entries. Trainer battles are excluded. A mystery encounter's
wild or boss fight is a wild battle in game-core (`run->me_battle`), so it is
scanned like one, and a trainer fight inside an encounter is not (upstream's
`MysteryEncounterBattlePhase.endBattleSetup` skips TRAINER_BATTLE); the Safari
Zone is the one encounter that scans without a battle, see below. The native
HUD's small improved-IV icons require a separate HUD feature; this fix supplies
the missing prompt and assessment window. It does not establish full battle-core
completion.

## The Safari Zone

`summonSafariPokemon` pushes a `ScanIvsPhase` for each Pokemon it summons when the player has an IV Scanner, before the four actions are offered. A Safari
is a MYSTERY screen with nobody on the battle's field, so the driver (`scan_safari_ivs` in `run_screen.c`) asks for `run.enemy[0]` when the actions
screen first comes up for a Pokemon (keyed by wave and the `remainingCount` the screen carries, so bait, mud and a ball that broke free do not ask again),
with the same question, the same assessment window and the same ratings as above. It answers nothing to game-core: no command, no RNG, no dex unlock.

```sh
make -C game/tests runtest
RUNTEST_SCANNER=1 RUNTEST_SAFARI=1 game/tests/build/runtest 60 0
RUNTEST_SAFARI=1 game/tests/build/runtest 60 0
python3 game/tests/check_safari_scan_faults.py
```

`RUNTEST_SAFARI` forces the Safari Zone from wave 5 (the playtest hook of `gc_debug_force_mystery`). With a scanner the test checks that each Pokemon is
asked about once, that the window shows that Pokemon's own IVs and that no RNG stream moves; without one it checks that nothing is asked.
Not run on a console or in Azahar. Upstream's own TODO in `summonSafariPokemon` says the scan is placed apart from the encounter text because awaiting that text shows
it without the Pokemon on screen; the port asks once the Pokemon and the actions screen are up, which is not a replay of that ordering.
