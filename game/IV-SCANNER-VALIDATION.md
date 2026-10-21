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
including loaded entries. Trainer battles are excluded. Mystery encounters have
an additional native scanner callback that remains separate work. The native
HUD's small improved-IV icons require a separate HUD feature; this fix supplies
the missing prompt and assessment window. It does not establish full battle-core
completion.
