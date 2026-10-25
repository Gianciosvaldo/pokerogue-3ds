# Bounded monetary reward adapter (Task 96)

Nugget, Big Nugget and Relic Gold now transport the run balance through
`pr_reward_apply_money_item` (reward-core #156). Supported applications consume
the reward without storing a ghost item, preserve party/profile/inventory/RNG,
and use ordered Amulet Coin objects and the MAX_SAFE_INTEGER clamp. Success
includes zero actual credit at the cap. A shop choice applies the credit before
deducting its price and keeps the screen open; a free pick ends the phase.
The API distinguishes nominal amount and actual credit; neither is truncated
into a newly emitted int32 event. Existing shop payment events remain unchanged.

Challenges, unknown item keys, malformed Amulet ownership/stock, invalid
wave/balance/party/capacity and primary or fused Gimmighoul remain unsupported
and retain the existing counted storage path. Gholdengo is supported.
This is balance and modeled reward-boundary parity, not full Scene parity:
MoneyAchv/ModifierAchv validation, money text/animation, purchase sound and
Gimmighoul EvoTracker/updateModifiers are excluded. Upstream addMoney does not
increment a lifetime-money stat; this adapter leaves profile data unchanged.

## Reproduce

Node 22.23.3, pinned external upstream
`8555c08c823b856cbec4eb99ca84ea52a955836d`, prepared frozen dependencies/locales/assets:

```sh
make -j2 -C game-core test
for task_seed in 777 9001 4242 31337; do
  game-core/tools/moneydiff.sh /external/pinned-upstream 2000 "$task_seed"
done
game-core/build/gcplay --auto 1000 --jobs 8
game-core/build/gcplay --auto 1000 --jobs 8 --save-test
```

`BUILD` and `GC_MONEY_OUT` select durable build/trace paths. The additive producer
uses real factories, Scene.addModifier, MoneyRewardModifier, Scene.addMoney and
SelectModifierPhase.applyModifier. UI methods/achievement validation are observed
or intercepted headless boundaries; unexpected queued phases throw. It records
all 54 modeled party fields, entire fixture inventory, global RNG, nominal/actual
credit and paid/free phase outcome. The comparator rotates storage and replays
both direct adapter and public shop/free choices, preserves the complete C profile,
and rejects incomplete records, malformed transport and missing completion footer.
No established producer/comparator/seed/count/exclusion was changed.

## Results

Base main `1c984b7`. Evidence is outside the repository under
`/workspace/validation/lane-c/eight/money`.

- Four seeds x 2,000 source cases: 16,000 direct/public comparisons, zero
  mismatches or skipped/thrown cases. Per seed family counts 667/667/666,
  1,332 purchases and 668 free picks. Capped balances: 1,422 / 1,431 / 1,427 /
  1,418. Fixtures include fractional balances, price-zero shops, wave 1/10000,
  multiple ordered Coin objects, forms/fusions and prior custom natures.
- 110 host checks: repeated/free/paid applications, affordability, zero-price
  shops, cap success, no ghost inventory, legal save/load, primary/fused
  Gimmighoul fallback, Gholdengo, unknown keys, invalid Amulet owners/stock,
  negative and overflow inventory/party counts and wave guards.
- Actual temporary adapter-source mutant subtracting credited money after the
  API call: 3,202 failures / 4,000 comparisons, exit 1. Missing footer and invalid
  family transport: exit 2. Mutants are external files, never committed.
- Existing four-seed healing/candy/ball/options traces replay byte-identically
  before/after; existing exclusions and constructor-stat diagnostics remain
  uncredited. Four-seed Mint/vitamin traces (16,000 each) and Orb full-turn/text
  traces (8,000) still pass unchanged.
- H passes: 32,657 resume checks, 39 item-bridge checks and 641,335 existing
  application checks. The paired 200-run digest changes `803ad8cf` to `39213240`.
  Save/no-save pairs agree. The 1,000-run baseline has 49,284 turns,
  digest `403b442f`; afterward 49,100 turns, digest `2473e509`, 14,271 save round
  trips, maximum save 11,373 bytes. Every run ends; no hangs/crashes/timeouts.
  Chooser/digest code is unchanged. The same-soak external adapter observation
  counts real Nugget/Big/Relic applications 253/57/1, versus taken counts
  294/66/2; 41/9/1 still use unsupported stand-ins. A taken item is not thereby
  credited as a real effect.

Required host/3DS CI must pass on the actual PR head. No console, whole-run
upstream, full Task 96 or full achievement/presentation parity is claimed.

## Gimmighoul follow-up

The historic Gimmighoul refusal above is superseded by the
[EvoTracker owner implementation and proof](EVO-TRACKER-VALIDATION.md).
Other stated limitations remain scoped to their individual evidence.
