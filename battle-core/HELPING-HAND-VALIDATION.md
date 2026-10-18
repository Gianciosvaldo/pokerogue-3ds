# Helping Hand tag lifecycle and doubles continuation

Reference: upstream `8555c08c823b856cbec4eb99ca84ea52a955836d`, locales
`270ed0a2938b2c3e2e34e7de940e84aecadf548b`, Node22.23.3.

The Helping Hand tag now follows its native constructor, add message,
non-stacking overlap, expiry and removal behavior. Its constructor fixes the
source move to Helping Hand and duration to one turn regardless of incoming
arguments. The add message resolves the original source and uses source/target
affixes as upstream does; a missing source leaves the tag without a message.
The existing1.5x power effect is reached once the complete tag lifecycle is
implemented. Other unsupported tag types retain their accounting.

## Actual source API and owning turns

`tests/helping-hand.producer.test.ts` uses the real Pokemon add/lapse/remove/
source-removal/source-transfer APIs and native HelpingHandTag. It captures raw
uint32 actor IDs, including zero, source IDs including missing/orphan/reserve
sources, duplicates and constructor arguments, expiry at turn-end/custom/faint,
non-expiry before a move, source removal/transfer, localized queues and both
RNG streams. No source gameplay method is replaced. The original BattleScene
RNG is captured before GameManager setup and restored after startup.

`tools/build-helping-hand-proof.py` only transports actual observations. The
replay compares API return values, tag ownership/count/duration/source move,
lapse/baton metadata, all queued message levels and exact text, dynamic queue
emptiness and both RNG streams after the sequence.

- 8,192 complete API cases,18,696 operations, zero mismatches.
- Three fresh full doubles-turn captures: plain seed777, rich seed777, plain
  seed2034,2,000 requested/written cases each; zero source exceptions, parse or
  initial-stat mismatches. The established move chooser and exclusions remain
  unchanged. The producer now restores the original RNG method instead of a
  copied body; the copied body was identical to the pinned implementation.
- Covered turns:1,805→1,850;1,812→1,852;1,811→1,859. All5,561 final covered
  turns and localized messages match. Covered-ID sets prove zero lost cases
  and133 gained, rather than relying only on net counts. The samples execute
  ally targets/hits, spread targets/hits, simultaneous faints and replacements.
- Nine compiled runtime faults caught; fourteen malformed captures rejected.
  Removing the power multiplier causes19 covered full-turn failures, so the
  owning-turn comparison proves gameplay damage in addition to tag metadata.
- Augmented game-core command/save fixture:28 checks pass. It inserts a tag
  through the public battle API before a command save, verifies source/move/
  duration after load, replays the real fight command on original/restored runs,
  verifies increased damage and turn-end expiry. It is not a natural doubles
  run or legal-clear proof. Enemy stats/HP are finalized using the encounter's
  existing constructor/calculateStats boundary.
- Full game-core suite passes; paired200-run saved/unsaved digest `d42eb77a`.
- Retained24k mixed source turns pass with no coverage loss. Exact-head host,
  app and3DS CI are required merge gates; final results are in the pull request.

## Remaining requirements

The direct tag fixture uses ordinary non-fused actors, NONE abilities and no
inventory/status. The broader full-turn rich sample supplies additional native
contexts, but explicitly unsupported cases remain excluded:439 of6,000 turns
are not covered by this comparison. That number is not a count of distinct
missing effects or a completion percentage.

Acupressure, Core Enforcer's acted/suppression path, After You, attention
redirection and further callbacks remain. This patch does not integrate playable
app doubles, double save/recovery, all partner behavior, final-boss run
orchestration or wide-level Endless. No run-core edits or emulator/hardware
acceptance are included.
