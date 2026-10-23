# Inventory capacity across battle, rewards and saves

Source pin `8555c08c823b856cbec4eb99ca84ea52a955836d`, locales
`270ed0a2938b2c3e2e34e7de940e84aecadf548b`, Node 22.

PersistentModifier.add in `src/modifier/modifier.ts` appends unmatched entries
to an array. It limits stacks of matching entries, not total inventory length.
BattleScene.tryTransferHeldItemModifier preserves this behavior. Battle's
addPostBattleLoot similarly appends transferable object references. The old port
stored 128 game entries, projected only the first 96 into battle, and used an
8-bit battle count. A preserved old-library diagnostic reports 114 stored and
96 projected entries. These are actual capacity gaps, separate from the prior
fusion producer's independent-case loot leakage.

## Change

Battle and reward stock share a 2,048-entry per-side limit; battle counts are
16-bit. Game storage keeps the existing 32-entry validation headroom, while its
battle bridge rejects out-of-range counts before reading any items. A full
unmatched add reports ENGINE_LIMIT instead of silently discarding stock. Run
save/load rejects counts outside the supported battle limit. Save buffers grow
from 128 to 512 KiB and remain outside the app's stack. The save format and field
schema are unchanged; older fixtures remain required with zero dropped data.

The generated pinned factory catalogue has 109 keys: conservatively 233 held
or form-item identities per holder and 43 side identities. Counting every form
item even when mutually exclusive, six holders and one extra Eviolite clone
class per holder gives 1,447 identities. `audit-inventory-catalogue.py` checks
these counts and the storage bound. It deliberately overcounts generated
species-item aliases. This is a bound on catalogue identities, not on every
transient object history or arbitrary raw constructor argument.

## Native observations and owner checks

The producer naturally starts six player and two enemy Pokémon, captures native
battle RNG before GameManager construction and restores it after startup. It
constructs 57 distinct canonical transferable item identities per holder and
runs the actual PersistentModifier.add and scene update methods. The two Orbs
match as one class, so only Toxic Orb is included in this distinct-identity set.
Factory argument metadata follows actual ModifierType object references,
including clones, without computing an effect oracle.

It observes 342 player and 114 enemy entries, an actual scene transfer from
index 107 into the sixth player, then 113 detached post-battle loot references.
The fixture transports exact ordered keys, arguments, owners, stacks and concrete
classes. Game-core runs the bridge, raw transfer, synchronization, reward-context
transport and save/load against those source lists. It compares all entries,
including positions beyond 255, and preserves zero dropped data after reload.
The native matrix is an inventory/transfer/loot observation, not a full-turn
comparison of every item interaction.

Additional storage stress deliberately repeats entries on both sides up to the
full limit, producing a save larger than 128 KiB and loading all 4,096 entries
with zero dropped data. This exercises serialization capacity; it is separate
from source acquisition parity. Invalid counts on either side are checked before
any null-key entries can be read. The owner test currently runs 10,854 checks.

Five compiled faults are detected: restoring battle truncation at 96, reward
truncation at 256, the old 128-KiB save buffer, silent full adds, and silent
invalid-count truncation. An initial save fault targeted a different save owner
and survived; the control was corrected to constrain the actual run writer.
Both logs are retained.

Historical fixed-count guard cases at 257 are retained as admitted inputs;
additional no-mutation guards now use the expanded limit plus one. Full-capacity
stack checks still require the exact inventory count. Raw bridge harnesses check
both game and reward storage bounds before copying. No native case, comparison
floor or guard count is removed.

The retained 5,120 fused turns, 2,000 ordinary stat turns, 2,000 critical-item
turns, Pickup 4,400, transferred-stat-item 960, theft 32,768, Grip Claw 32,768,
Baton 256 and scene transfer/Mini Black Hole 3,608 comparisons pass against the
expanded battle state. Normal game-core, app, custom/save tests and actual ARM
build/stack checks remain required merge gates. Only custom-only Task63 is
skipped. No SD-card builder is produced.

Host sizes change from 23,960 to 277,720 bytes for pr_battle and from 474,872 to
912,952 bytes for gc_run. App run/save copies are static. The automatic battle
constructor and actual ARM frame reports require the existing 512-KiB startup
stack check; host sizes alone do not establish hardware acceptance.

## Remaining scope

Detached objects, repeated faint/revival/theft cycles and orphan inventory can
exceed a simple catalogue-identity bound and still need a temporal inventory
audit. The explicit 2,048-entry boundary remains reported. This change does not
close history capacities, every acquisition/updateModifiers path, broad raw
constructor parameters, or the remaining wide-Endless acceptance audit. Battle
core is not declared complete.

Reproduce:

```sh
. /workspace/shared/activate-parity.sh
sh battle-core/tools/inventory-capacity-diff.sh /path/to/pinned-upstream
python3 battle-core/tools/check-inventory-capacity-proof.py
make -C game-core test
```
