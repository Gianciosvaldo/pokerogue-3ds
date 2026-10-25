# Repeated faint history and its owners

Repeated revivals could overflow the twelve-entry faint log. The old path kept
the first eleven entries plus the newest, reported an engine limit, and could
forget an identity needed by trainer instant-Tera eligibility.

Keep the newest twelve entries in order, and maintain encounter membership for
both sides under the existing six-member party contract. Retaliate still reads
the newest turn. Trainer Tera now queries full membership, including identities
that have aged out of the retained tail. Raw missing prefixes remain unknown;
known positive entries remain usable. More unique identities than the party
contract and exhausted numeric counters remain explicitly reported.

## Pinned source owners

Source `8555c08c823b856cbec4eb99ca84ea52a955836d`, locales
`270ed0a2938b2c3e2e34e7de940e84aecadf548b`:

- `FaintPhase.start/doFaint` appends `{pokemon, turn}` after the instant-revive
  check; the initial Classic final-boss transition is a separate path.
- Retaliate's registered `MovePowerMultiplierAttr` reads each side's newest
  faint turn, qualifying only the user's side and a difference of one turn.
- `Trainer.shouldTera` rejects an instant-Tera candidate if any enemy faint log
  references its identity, even if it has since revived.

The actual native phase, registered multiplier and actual Trainer method supply
all expected results. Capture native battle RNG before GameManager and restore
it after natural doubles startup. Presentation callbacks finish immediately;
phase progression is stopped. Repeated native phase cases use explicit revival
inputs between callbacks; they are not claimed to be full native command turns.

## Evidence and limits

- 72 native cases: both sides, four identity patterns, lengths
  0/1/11/12/13/14/32/100/512. Actual source logs are retained losslessly. Compare
  the ordered newest twelve entries, counters and both consumers across four
  turn offsets and every one of six trainer candidates. **7,864 phase/caller
  observations**, with unchanged observed global and battle RNG states.
- Matching pre-fix ABI/source baseline: **5,348 mismatches/uncovered**. Final
  replay: zero. An earlier setup lacked wave seed and EXP cap and reported
  missing context on every faint; those runs are not capacity evidence.
- **279 raw/missing-prefix/identity/overflow guards**, including ID zero after
  aging out, malformed summary lengths and bounded counter saturation without
  signed overflow. These numeric boundaries remain unsupported, not silently
  treated as parity with JavaScript's larger integers.
- A configured six-member party runs **thirteen actual sacrificial/revival
  command cycles** using finite ordinary PP and PP Ups. No HP revival or PP
  refund is injected during the loop. Real forced replacements and Revival
  Blessing choices execute. After victory, the next encounter clears per-battle
  history, preserves the arena faint counter and passes save/load: **268 checks**.
  Initial party/stat configuration is controlled test input, not a proof of
  campaign generation or party acquisition.
- Eleven compiled controls cover tail truncation, lost old membership, zero
  identity, missing prefixes, identity overflow, both RNG streams, side ownership,
  missing enemy logs, the actual AI consumer and next-encounter reset. Twelve
  malformed native captures reject.
- Fresh pinned reproduction regenerates the fixture byte for byte. Standard CI
  includes the native replay, guards, real command owner and fault controls.
- Targeted ASan/UBSan/LeakSanitizer instruments history/phases, actual AI in the
  native-test translation unit, the game battle owner and test translation
  units. Other archived modules are ordinary builds, not fully sanitized core.

The game saves only at its existing legal command checkpoints. The real loop
rejects saving during these battle/choice states. New encounter setup clears the
whole battle state before the next legal save; no new save extension or relaxed
save permission is needed for ephemeral encounter membership.

Run `make -C battle-core faint-history-host-test`,
`make -C game-core faint-history-host-test`, and
`python3 battle-core/tools/check-faint-history-proof.py`.
With Node 22 and pinned dependencies, reproduce using
`battle-core/tools/reproduce-faint-history.sh /path/to/upstream /tmp/faint-proof`.

This closes repeated-faint storage for these consumers within the stated party
contract. It does not claim unbounded counters, broader identity/cardinality
changes, all revival interactions, complete source inventory or full wide-level
Endless acceptance. No Windows builder or release accompanies this change.
