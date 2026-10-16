# Stuff Cheeks, Teatime, Bug Bite and Pluck

Pinned upstream `8555c08c823b856cbec4eb99ca84ea52a955836d`, locales
`270ed0a2938b2c3e2e34e7de940e84aecadf548b`, Node22.23.3.

## Source behavior

EatBerryAttr calls only its parent HP gate. It selects an owned BerryModifier
using the user's battle RNG, including locked stock. Berry Pouches on the donor
side can preserve it, with short-circuiting battle RNG draws. Consumption loses
the held item and refreshes donor-side modifiers before applying the berry.
The source calls eatBerry(target), even when a raw self-target attribute is
invoked with distinct user and target. PostItemLost and HealFromBerryUse apply
to that consumer; preservation excludes the berry from the Harvest history.

StealEatBerryAttr omits the parent HP gate. It checks BlockItemTheft before
looking for berries, picks target stock with the user's battle RNG, and ignores
transfer locks. Source callback ordering includes three donor PostItemLost calls:
before the message, during consumption, and after the berry effect. The user's
HealFromBerryUse follows. Stolen berries update turn history and hasEatenBerry,
but not Harvest history unless the raw callback aliases user and target.

Stuff Cheeks' selection restriction and use condition search every BerryModifier
on the user's side, including ally/reserve stock. They do not require an owned
berry. Belch's owner-specific predicate remains separate. The normal BerryPhase
implementation is unchanged.

## Evidence

`tools/berry-moves-diff.sh UPSTREAM [16384] [777]` captures native calls and
complete turns, replays them, runs fault controls and the game command/save test.
Native BattleScene battle RNG is captured before GameManager construction and
restored after battle startup. Gameplay callbacks delegate to native functions;
only UI, assets, animations and automatic phase advancement are intercepted.
Queued stat/heal/status/ability phases are executed through native phase start.

- Final protocol v3: 16,896 complete, exact cases (16,384 base plus 512 appended
  virtual-stock controls). Raw overrides and applyMoveAttrs dispatch cover all
  four moves, all eleven berries, four naturally active actors, same/opposite
  sides, primary/passive abilities, suppression, HP/status/stages/PP and pouch
  counts. Selection text, use condition and restriction are observed directly.
- Exact comparisons include four full actor projections, five tags with source
  metadata, status counters, all three berry histories, hasEatenBerry, real and
  virtual stock, inventory order/ownership/locks, typed queues and stat source
  options, callback order/actor, drained phase effects, localized text and both
  RNG states. Native exceptions are not skipped; complete footers are required.
- Earlier 16,384-case v1/v2 and 16,896-case v2 captures remain accepted and exact.
  The first capture used a preliminary PP fixture; its actual native inputs and
  observations are retained. Later captures use each chosen move's own max PP.
- Complete doubles turns: 2,050 source turns, zero exceptions; 2,049 exact and
  one explicitly unsupported Xerneas form-change case (1782). All 2,048 original
  cases remain. Two appended Hyper Voice controls exercise the unchanged
  comparator's both-opponents targeting floor. No coverage requirement was
  reduced. Exact turns include 1,115 ally-target commands, 87 turns hitting both
  opponents and 124 turns with multiple faints. The legacy turn format does not
  contain full berry histories; those are checked by the callback/phase proof.
- 21 compiled runtime faults caught; 27 malformed/truncated captures rejected.
- Game command/inventory/save replay: 4,400 checks, all four moves and eleven
  berries in both directions with zero/three pouches.
- Retained broad doubles: 1,964/1,953/1,967 exact; mixed turns:
  1,976/1,970/1,991/1,995 exact. No previously covered IDs lost. Normal game-core
  tests pass; saved/unsaved 200-run digests both `361c1d0e`.

These bounded proofs do not establish complete battle-core, all modifier
families, all phase combinations, or unrestricted Endless arithmetic support.
The Xerneas case remains explicit pending separate form lifecycle work.
