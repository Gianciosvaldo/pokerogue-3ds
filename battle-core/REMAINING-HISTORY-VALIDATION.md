# Complete Conversion 2 and accuracy history searches

The eight-entry retained move tail could hide earlier entries from Conversion 2
and from Lock-On/Mind Reader accuracy checks. Add semantic summaries of whether
any non-NONE move occurred (all use modes), and the latest Lock-On/Mind Reader
entry (all modes/results, exact ordered targets). Append/reset and shared target
updates maintain these summaries, using the existing serial and summon epoch.
Short raw histories are exact; long raw/legacy tails without their older prefix
remain unknown until a matching new entry supplies the answer.

An optional named `*_all_moves` save extension preserves this state without
changing older field widths. Validate field schemas, knowledge/presence masks,
move identity/mode/result/targets and serial. Missing extensions rebuild only
what the retained tail establishes. No unsupported flag is removed for genuinely
missing input history.

## Direct source owners

Pinned upstream `8555c08c823b856cbec4eb99ca84ea52a955836d`, locales
`270ed0a2938b2c3e2e34e7de940e84aecadf548b`:

- `ResistLastMoveTypeAttr.getCondition`, move.ts:9121: not Terastallized and
  `target.getLastXMoves(-1).some(move != NONE)`. The condition is sequence 4.
  The move effect still reads the newest entry, even when that entry is NONE;
  do not substitute the latest nonvirtual or successful move for this behavior.
- `MoveEffectPhase.checkBypassAccAndInvuln`, move-effect-phase.ts:467: with an
  IGNORE_ACCURACY tag, the newest Lock-On/Mind Reader entry must include the
  target. The source filters neither use mode nor result. A native undefined
  return when no bypass applies is compared as false at its boolean C caller;
  the retained raw capture keeps the distinction.

The source producer calls both actual owners with native battle RNG captured
before GameManager and restored after natural doubles startup. No query, phase
accuracy method, condition or RNG is replaced.

## Evidence

- 192 native histories: lengths 0/1/7/8/9/64/65535/65536, first entry
  NONE/Tackle/Lock-On/Mind Reader, all six use modes. Ten stages include absent
  and present tags, shared target edits, failed Lock-On, nine newer NONE entries,
  missed Mind Reader, summon reset and old/current shared arrays after reset.
  **9,600 caller observations**, exact outcomes and unchanged RNG.
- The original seven-stage baseline had 2,016 mismatches/uncovered out of 6,720
  observations; the final ten-stage implementation has zero out of 9,600.
- **1,920 real codec roundtrips, 30,227 checks**, including live and restored
  callers, 13 malformed/duplicate extensions and legacy missing-prefix behavior
  followed by a new match and resave. Existing save-point eligibility is retained.
- **126 raw/legacy/shared-array/reset guards**. A target list from an old summon
  cannot rewrite the current aim entry, even when serials coincide.
- Ten compiled history/alias/save fault controls and eight malformed capture
  controls are retained in the standard CI proof script.
- Retained chain-wave (256 histories/569 checks), interrupted charge (44
  victories/7,235 checks), nonvirtual (5,832 saves/161,856 checks) and Protect
  (244,800 native observations plus 6,120 saves/511,661 checks) pass locally.

Run `make -C battle-core remaining-history-host-test`,
`make -C game-core remaining-history-host-test`, then
`python3 battle-core/tools/check-remaining-history-proof.py`.
The script regenerates the checked-in C fixture from the lossless compressed
native capture and checks it byte for byte. With Node 22 and dependencies:
`battle-core/tools/reproduce-remaining-history.sh /path/to/pinned-upstream /tmp/history-proof`.

An earlier sequence-3 producer failed its required true/false coverage assertion;
it is not used as condition evidence. A proposed control omitting an otherwise
unobservable reset cache rebuild was equivalent because empty-history getters
ignore it and the next append rebuilds it; replaced with a meaningful old-epoch
binding control. No check or producer case was dropped to force passing results.

This closes these two whole-history searches, not battle-core completion.
Faint-history capacity and its Retaliate/trainer-Tera callers, the remaining
source inventory and broad wide-level Endless acceptance remain separate gates.

Targeted ASan/UBSan/LeakSanitizer also passes: history/engine/condition/codec
objects and the test translation unit containing actual phases are instrumented.
Other archived modules are ordinary builds; this is not fully sanitized core.
