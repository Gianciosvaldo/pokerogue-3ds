# Macho Brace battle ownership

Pinned upstream: `8555c08c823b856cbec4eb99ca84ea52a955836d`; locales:
`270ed0a2938b2c3e2e34e7de940e84aecadf548b`.

`PokemonIncrementingStatModifier` already had C stat arithmetic. This change
connects canonical attached `MYSTERY_ENCOUNTER_MACHO_BRACE` stock to that class,
admits its validated real/virtual counts in the battle path, and implements the
`BattleScene.applyPartyExp` growth owner in game-core's Victory phase.

The source finds the first held Brace for each surviving participant, increases
its real stock if below 50, then calls `updateModifiers(true, true)`. That clears
virtual stock and recalculates the entire party. These actions precede the EXP
level-cap check. At real stock 50 the stat calculator grants the extra 10% HP and
5% other-stat bonus; virtual stock does not trigger that bonus. Duplicate Braces
retain first-match behavior. Captures also queue `VictoryPhase`, which supplies
`pokemonDefeated=true`; `PartyExpPhase` supplies false and does not grow the item.

Battle friendship also needs the first Soothe Bell before Brace growth. The
previous conservative projection refused a virtual Brace or any fused party
member, producing a counted fallback with the wrong Bell gain. The native owner
matrix proves those two combinations. Acquisition retains its separate guards.

## Native observations and replay

Both producers capture native `BattleScene.prototype.randBattleSeedInt` before
constructing `GameManager`, and restore it after natural battle startup. Each
observation requires unchanged global and battle RNG streams. Production source
files are checked clean at the pin by the reproduction script.

The committed transport retains every observation, with exact matrix keys,
counts, record IDs and footer; it computes no expected battle results:

- The original 32 high-level owner audits remain required unchanged in shape.
- 7,500 stat snapshots: Mew, Shedinja and Charizard; unfused/Mew fusion; levels
  1, 5, 100, 1,000 and 65,536; all 25 natures; ten real stocks spanning 1–50 and
  alternating legal virtual stock. C compares native HP and all six stats both
  through the direct class and the actual game inventory bridge.
- 1,152 `applyPartyExp` snapshots: the original 576 arrangements plus the same
  matrix with fused party members. Six arrangements include fainted holders,
  reversed party order, two holders, duplicate Braces and Soothe Bell. Every
  participant mask, real stocks 1/49/50, defeat flag and levels 5/1,000 remains
  required. C compares all three party snapshots, ordered stock, friendship,
  EXP phase order/amounts/visibility and both RNG streams. The extra C
  `RP_CONTINUE` adapter phase is required explicitly after the native EXP queue.
- 12 actual successful-catch callbacks execute upstream `catch()`, the full-party
  No handler, queued real `VictoryPhase.start()`, and real `PartyExpPhase.start()`.
  Presentation completion callbacks are immediate; capture rolls are outside
  this successful-catch boundary. C compares all 12 owner observations and also
  runs six complete Master Ball command/catch-answer routes to the next wave.
- Owner snapshots round-trip inventory and party stats through saves without
  dropped data. Callback-interior catch states must refuse saving. The complete
  command routes save/load at the next actual valid command screen.
- Malformed stock, constructor arguments, detached/wildcard owners, transferability
  and opaque subclass guards remain outside the promoted canonical class.

`check-macho-brace-proof.py` compiles 12 faults against production implementations:
wrong maximum bonus, virtual-as-real arithmetic, level-cap gating, fainted or
nonparticipant growth, overfilling, last-match selection, uncleared virtual stock,
blocked virtual bridge, transferability and both Bell fallback regressions. All
must fail the native replay. Ten malformed/truncated/duplicate/extra transports
must also reject.

The existing Soothe host matrix retains all ten arrangements. Its former fused
party refusal becomes a positive gain/candy/stock assertion, justified by the
new native fused matrix. Other refusal checks and all existing test floors stay.

## Reproduce

```sh
. /workspace/shared/activate-parity.sh
game-core/tools/macho-brace-diff.sh /external/pinned-upstream
python3 game-core/tools/check-macho-brace-proof.py
make -j4 -C game-core test
```

Normal app/custom/save, retained held-item proofs and actual 3DS CI remain required
before merge; the dedicated custom-only Task63 check stays excluded.

Local validation passes: the reproduction script regenerates and compares the
same full matrix (native party IDs may differ between runs). Native replay checks
1,097,365 assertions; all 13 compiled faults and 10 malformed inputs reject.
Full game-core tests pass with identical saved/unsaved digest `5073f991`.
Retained proofs pass: Thief/Covet 32,768; generic held transfer 32,768; Grip Claw
32,768; Baton 256; Scene/Mini Black Hole 3,608; Grip/Baton full doubles turns
2,114 + 2,050 with zero skipped or uncovered turns. Normal app/custom/save and actual 3DS gates passed for PR #398
(CI run `40085338940`). Later changes require their own exact-head CI.
The retained C replay now checks the distinct battle RNG as well as both global
RNG copies, with an extra compiled fault proving that consumption is caught.

This validates the stated battle/stat/capture/save boundaries. It does not
validate the Winstrate encounter's multi-trainer reward-generation chain, raw
orphan/wildcard scene-cleanup histories, arbitrary mixed modifier inventories,
unbounded levels, general form activation, fusion evolution or all Endless
interactions. Those limits cannot be inferred closed from this bounded proof.
