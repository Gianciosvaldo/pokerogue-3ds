# Gimmighoul tracker: pinned acquisition and primary evolution

Source: `8555c08c823b856cbec4eb99ca84ea52a955836d`, locales
`270ed0a2938b2c3e2e34e7de940e84aecadf548b`. This closes the canonical
EvoTracker held-class admission, Nugget acquisition and primary evolution
caller gaps. The [fusion level-up follow-up](FUSION-EVOLUTION-VALIDATION.md)
adds secondary selection/mutation; neither record closes battle core.

`EvoTrackerModifier` is nontransferable, has a no-op `apply` returning true,
clones its species/required fields, and caps held stacks at 999. Its canonical
factory uses Gimmighoul and **required=10**. MoneyRewardModifier adds 1/2/3
coins per Nugget/Big Nugget/Relic Gold, in party order, for either original
species component. A rejected increment gets COMMON's default five Poké Balls;
there is no partial fill. Awards continue when the wallet is already capped.

The evolution condition requires an existing tracker. It adds the number of
Golden Punch objects owned by this Pokémon and side-wide Amulet Coin/Golden
Poké Ball objects, counting entries rather than stacks. The primary evolution
owner now evaluates that condition and removes its first tracker on evolution.
Remaining duplicate trackers retain source order. Saves use the existing item
schema and preserve tracker stock/ownership/locks without a format change.

The reward-core money helper remains a monetary primitive: its result now
reports per-party tracker additions. The game-core caller handles stacking,
fallback, party refresh and persistence. Unknown/virtual/malformed money item
inputs retain existing explicit caller guards. At the fixed inventory limit,
a new tracker refuses explicitly before changing money; an existing matching
tracker can still increment. Species-specific pruning is applied when money
awards refresh trackers; this is not a claim about every modifier lifecycle.

## Native observations and retained checks

`tests/evo-tracker.producer.test.ts` starts a natural battle and captures native
BattleScene.randBattleSeedInt **before** constructing GameManager, restoring it
after startup. It calls actual MoneyRewardModifier.apply, Scene.addModifier,
Scene.updateModifiers, getPersistentTreasureCount, getEvolution and evolve.
Money UI/achievement hooks, modifier icons, evolution asset loading and status
UI are intercepted presentation boundaries. Both native RNG streams remain
unchanged across each monetary application. No expected effect is computed in
the transport builder.

- 576 required acquisition cases: all three money rewards, six primary/fused/
  multiple/absent Gimmighoul arrangements, eight tracker counts including
  997/998/999, and four treasure inventories. Full ordered stock, balances and
  ball fallback match; every source case is required, with no exclusions.
- All 3,456 primary/secondary evolution queries match before/after rewards:
  the retained 2,688 primary/no-fusion observations and all 768 secondary
  observations, including 272 ready secondary evolutions after the reward.
  Fused acquisition is fully compared in all 576 cases. No source observation
  is discarded. The level-up queue and mutation owner have separate evidence
  in the fusion follow-up; this query corpus does not prove a whole battle.
- 12 actual source primary evolution cleanups: both forms, one/two/three
  tracker objects, fused/unfused primary holders, preserving another holder's
  tracker and the remaining ordered inventory.
- The native replay includes 58,576 C owner/save checks, all snapshots restored
  with zero dropped entries. A separate 2,124-check C test runs actual doubles
  commands before/after restore and checks legacy lock flags, discard/transfer
  refusal and full inventory boundaries. These C command checks establish
  continuity, not native complete-turn queue equivalence.
- Seven actual compiled faults and eight malformed transports are rejected:
  fusion omission, wrong reward factor, stack-vs-entry treasure, threshold999,
  missing cleanup, overfill and transferable tracker.
- Original four money seeds (777/9001/4242/31337), 2,000 source cases each,
  retain every party field, inventory entry, monetary result and RNG check.
  Replaying them exposed an already-failing synthetic continuation: a fainted
  captured party[0] cannot stop at RP_COMMAND. Both baseline and preliminary
  replay fail the same 64 cases for seed777. The harness now supplies a separate
  living command sentinel at the producer's intercepted shiftPhase boundary;
  the actual source party, including HP zero, remains unchanged and compared.
  No count, source case or expected field was removed. This is not a source
  battle-command continuation proof.

Full local game-core tests pass, including saved/unsaved digest `5073f991`.
Normal app/custom/save and 3DS stack/build gates must pass on the PR head before
merge. The custom-only Task63 gate stays skipped. No downloadable builder.

## Reproduce

```sh
. /workspace/shared/activate-parity.sh
game-core/tools/evo-tracker-diff.sh /external/pinned-upstream
python3 game-core/tools/check-evo-tracker-proof.py
make -j2 -C game-core test
```

Fusion acquisition/item-evolution owners and special extra-party evolutions,
every raw constructor
parameter, virtual/orphan money acquisition, wider money-wave bounds and all
other open battle-core inventory/history/Endless rows remain outside this slice.
