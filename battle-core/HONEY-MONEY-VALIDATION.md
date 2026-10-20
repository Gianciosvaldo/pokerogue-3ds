# Honey Gather and wide money

Reference: upstream `8555c08c823b856cbec4eb99ca84ea52a955836d`, locales
`270ed0a2938b2c3e2e34e7de940e84aecadf548b`, Node 22. Both source checkouts
remain tracked-clean. Capture native `BattleScene.prototype.randBattleSeedInt`
before `GameManager`, and restore it after natural battle startup.

## Source behavior and implementation

`MoneyAbAttr` (`ab-attrs.ts:5396`) applies only when not simulated and victorious.
It adds `getWaveMoneyAmount(0.2)` to `currentBattle.moneyScattered`. Honey Gather
is registered **unsuppressable** (`init-abilities.ts:823`). The ordinary dispatcher
queues its ability banner and records application history. `BattleEndPhase`
visits allowed players in canonical party order, including reserves, before
picking up scattered money. The new game caller implements this Money callback
subset; Pickup remains explicitly unsupported where an actual transfer is needed.

Pay Day's `MoneyAttr.apply` (`move.ts:8820`) omits its parent's gates, adds the
same wave amount, and queues the coins message. Scattered money is now binary64,
as upstream's Number is. Battle effects and run economy share one implementation
of the exact source operation order, using the existing V8-compatible power.

Pickup and trainer rewards apply Amulet Coin records in order, double Happy Hour,
cap the wallet at `Number.MAX_SAFE_INTEGER`, and display the amount credited
before that cap. Runtime money events retain double change/wallet values; legacy
int fields mirror only representable values, otherwise zero. Money messages
freeze binary64 bits without a borrowed string and require confirmation in the app.
No session save fields change; the existing wallet save is already double.
Consumers must rebuild for the battle/runtime-event layout changes.

The explicit numeric profile is integer `Number.toLocaleString("en-US")`,
independent of translation language. Ryu provides shortest round-trip decimals;
expansion/grouping preserves native formatting of values such as
`1000000000000000128`, displayed as `1,000,000,000,000,000,100`.
Vendor commit and both licenses are retained in `vendor/ryu`. Other browser numeric
locales and fractional-number formatting are outside this profile.

## Native comparisons

All counts below are required capture inventories, not accepted-case filters.

- **77,236 exact** direct cases across seeds 777 and 2034: 32,768 Honey Gather
  cases and 5,850 Pay Day cases per seed. Pay Day visits every legal Endless wave
  1–5850. Compare all eight complete 125-field actor frames, queued regular/dynamic
  phases and frozen text, application history, scattered money, wave-money result,
  and both RNG streams. Include primary/passive/duplicate/passive-only owners,
  fainting, suppression, switching, physical absence, simulation, victory/loss,
  and balances through `Number.MAX_VALUE`.
- **2,048 exact** actual `BattleEndPhase.start` callback prefixes across both seeds.
  Observe with a passthrough at real money pickup, after the real allowed-party
  loop. Include reordered party arrays and reserve owners. Presentation teardown
  and final phase handoff are replaced after the observed boundary. Owner waves
  are 1, 200, 256, 3000 and 5849; this does **not** validate terminal Endless
  queue clearing at 5850 or the remainder of BattleEndPhase.
- **46,800 exact** real `Battle.pickUpScatteredMoney` / `MoneyRewardPhase.start`
  cases, covering every wave 1–5850, zero and wide balances, multiple ordered Coin
  records, Happy Hour, wallet caps and infinite pre-cap credit. Compare wallet,
  reset/preserved scattered balance, credited amount, exact text, confirmation and
  both RNG streams. Game-core tests invoke its actual owners.
- **200,000 exact** native integer/non-finite numeric-format cases: boundary values,
  every binary exponent's power of two, random integer doubles, negative zero,
  NaN and infinities. Check buffer truncation/required length and frozen message
  arguments with a different translation language. This is bounded formatter
  evidence, not a proof of every binary64 value or every locale.
- **902 real game caller/wallet/save checks**: duplicate-victory folding, reserve,
  passive and duplicate abilities, Honey Gather's unsuppressability, fainting,
  switching, presentation events, money event precision and capped-wallet save/load.

The initial raw fixture set the scene to Endless but left the battle's mode at
Classic, producing 80 wave-200 name-affix failures. Both native mode owners are
now initialized consistently before capture; original captures/failures remain
preserved. The owner transport now maps the native canonical field slice after
party reordering; assertions and expected text were retained. A host assertion
that suppression disabled Honey Gather was corrected against its explicit native
unsuppressable registration. All final cases remain mandatory.

## Final local gates

All 23 compiled faults and 25 malformed captures pass their rejection checks.
Actual previous `ab_attrs.c` fails 27,136/38,618 first-seed cases; previous
`move_effects.c` fails 3,300/38,618. Normal game-core passes with identical
saved/unsaved digest `8669f384`. All normal app/custom/save sanitizer runs pass,
including 10,000 normal runs (digest `8b5d6de3`), system saves, Save and Quit,
custom runs, custom resume and custom Save and Quit. The 3DS build passes.
The new 902-check caller/save test also passes as a normal test target.
Forty-two cross matrices, twenty-six supplemental matrices plus entry/context
checks, eighteen latest matrices and seven retained corpora pass. No historical
covered case IDs are lost. No source exception is discarded to make these pass.

## Reproduction and limits

Run `tools/honey-money-diff.sh /absolute/upstream-checkout` from a Node 22 shell.
It verifies both pins, generates fresh native captures, builds/replays all four
inventories and runs game host checks and compiled-fault/malformed-input guards.
The guard suite catches **23 compiled faults** and rejects **25 malformed captures**,
including missing callbacks/history/banners, wrong simulation/victory gates,
integer narrowing, missing Coin/Happy Hour/reset/prompt behavior, lost wide events
and number formatting faults. Normal game-core, app/custom/save and 3DS gates remain required; only the separate
custom-only Task63 job is excluded. Final gate results are recorded with the PR.

This slice does not establish full battle-core completion. Pickup item-object
transfer and its interleaving, the final source inventory/AI audit, terminal
Endless orchestration and complete wide-level acceptance remain open. The existing
M1 double-as-single game projection and explicit Illusion deviation remain visible.
