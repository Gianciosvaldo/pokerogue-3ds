# Transferred stat boosters and saved class identity

Pinned upstream `8555c08c823b856cbec4eb99ca84ea52a955836d`, locales
`270ed0a2938b2c3e2e34e7de940e84aecadf548b`, Node 22.

The held-item inventory review after Pickup found three remaining transferable
classes: StatBoosterModifier, EvolutionStatBoosterModifier and
SpeciesStatBoosterModifier. They inherit `isTransferable = true`. The concrete
factories are Eviolite and the six species boosters: Light Ball, Thick Club,
Metal Powder, Quick Powder, Deep Sea Scale and Deep Sea Tooth. Together with
the retained Pickup/Grip Claw captures, the canonical factory identities of all
25 transferable held-item classes now have transfer observations. This does
not establish all possible raw constructor arguments or all battle interactions.

## Source behavior

In `src/modifier/modifier.ts`, EvolutionStatBoosterModifier inherits
StatBoosterModifier.clone. Transferring Eviolite therefore creates a base
StatBoosterModifier: its DEF/SP.DEF multiplier then applies without the original
evolution/Max eligibility guard. SpeciesStatBoosterModifier overrides clone and
preserves its species list, stats and multiplier.

The scene's transfer helpers use different matching inputs. Preflight compares
each existing recipient item against the **clone**. Transfer compares existing
items against the **original** before adding the clone. Eviolite overrides
matchType, so these operations can disagree when the recipient already has
Eviolite. The port keeps that asymmetry. Stat matching includes the selected
stats and multiplier; species matching also includes the species list.

Native ModifierData records className and reconstructs that actual class with
Reflect.construct. An Eviolite clone remains a base stat booster after reload.
The game inventory stores an Eviolite-only class marker for this distinction.
The marker is optional for older port saves, whose Eviolite entries could only
be factory items. A present marker has a strict size and boolean value and is
rejected on another item key. The ordinary dropped-field accounting is unchanged.

## Native comparison

The producer uses naturally started doubles actors, captures native battle RNG
before GameManager construction and restores it after startup. It observes the
actual Pickup leaves, ability dispatcher, BattleEnd callback prefix, scene
transfer helpers, constructors, clone and ModifierData reload. Phase shifting
and subsequent enemy teardown are intercepted; actual item/stat updates run.
Honey Gather observations stop at the native boundary before money collection,
then allow the real money method to continue.

- 704 original observations across the seven stat-item identities and Grip Claw.
  These include raw stack boundaries, cached selection, primary/passive order,
  suppression, bench/fainted holders and both party orders.
- 256 additional observations compare every pair of these identities, with
  factory and cloned recipient stock, through both preflight and raw apply.
  The original observations remain required.
- All 960 compare exact ordered stock class/parameters/ownership/counts/flags,
  loot references and cache, actor HP/base stats/ability histories, every queued
  phase and rendered message, money and both RNG streams. Version 3 additionally
  compares all five effective stats for every actor before and after the call.
  Version 1's 704 and version 2's 960 observations remain accepted separately.
- Sixteen guaranteed native factory/clone save cases are transported without
  computing expected classes or effects. The game controller performs actual
  transfer, inventory synchronization and save/reload against those observations.
  Its retained test also checks the older marker-free schema, zero dropped data,
  invalid booleans, invalid item/marker combinations and wrong field size.
- Eight compiled clone/matching/cap/effective-stat/save-class faults are caught;
  three malformed/truncated captures are rejected.
- Full game-core tests pass, including the historical byte-level save fixture
  with its original zero-dropped-data assertion. Saved and unsaved run digests
  both remain `5073f991`.
- Retained Pickup 4,400, theft 32,768, Grip Claw 32,768, Baton 256 and scene
  transfer/Mini Black Hole 3,608 comparisons pass.

Reproduce with:

```sh
. /workspace/shared/activate-parity.sh
sh battle-core/tools/stat-loot-diff.sh /path/to/pinned-upstream
make -C game-core stat-loot-host-test
TURNTEST_LOCALES=/path/to/pinned-upstream/locales/en \
  python3 battle-core/tools/check-stat-loot-proof.py \
  battle-core/build/stat-loot.native.txt battle-core/build/stat-loot-save.native.txt
```

## Remaining scope

Fusion eligibility for original Eviolite, species boosters and Leek remains an
explicit full-turn refusal. Its existing four admission guards are retained;
they must be replaced only after native callback/turn proofs. Generic constructor
parameter arrays outside the represented canonical factories, orphan cached
stock and the other inventory/capacity boundaries remain separately reported.
Normal app/custom/save and the actual ARM build/stack CI checks remain merge
gates. No SD-card builder is produced by this slice, and battle core is not yet
declared complete.
