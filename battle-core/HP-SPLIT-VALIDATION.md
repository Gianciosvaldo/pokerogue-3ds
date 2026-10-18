# Pain Split direct HP changes

Pinned upstream `8555c08c823b856cbec4eb99ca84ea52a955836d`, locales
`270ed0a2938b2c3e2e34e7de940e84aecadf548b`, Node 22.23.3.

`HpSplitAttr.apply` calls its parent HP gate, then snapshots the floor of the two
HP values' average. It processes user first, target second, healing directly up
to maximum HP or calling `Pokemon.damage(amount, true)`. These direct changes
show HP numbers, with no DamageAnimPhase, heal phase or PostDamage ability hook.
Direct damage retains endure/faint logic and bypasses boss shield clamping while
still updating cleared shields and queuing their stat boosts. Pain Split's move
registration separately rejects boss targets; the raw attribute override does
not run this move condition.

Expose the existing damage implementation with its ignoreSegments argument,
retaining false for existing callers. The new callback reuses it with true and
emits the existing HP events for actual healed/damaged amounts. No save fields
or generated tables change.

Reproduce `tools/hp-split-diff.sh UPSTREAM 777`, then 2034. The raw producer invokes
the registered attribute and separately observes actual move.applyConditions.
Both producers capture native battle RNG before constructing GameManager and
restore it after natural double-battle startup.

- All 16,640 raw API cases and move-condition observations match exactly. Four
  field roles, self/ally/opponent aliases, zero/one/full/half/near-full HP, unequal
  maximum HP and levels 1..1,000,000. Each seed includes 128 direct API controls
  with 2/3/5/8 boss segments; the owning move condition rejects those bosses.
- Per seed: 1,636 denied raw calls, 3,916 heal numbers, 5,045 damage numbers and
  307 deferred faints. All actors' 125-field snapshots, boss state, HP event
  identity/amount/order/intermediate four HP values, phase levels/order, faint
  options, dynamic stat payloads and both RNG states are compared. C dispatch is
  void; native boolean returns are recorded without a C return parity claim.
- All 8,192 owning turns (seed 777) match exactly, with 2,147 successful native
  callbacks, zero source exceptions and no unsupported cases. The established
  comparator and ally/spread/faint/switch floors remain intact, including wide
  values and native Substitute blocking.
- Native PhaseTree.clear retains deferredActive. The initial C fixture assumed
  it reset this flag and consequently mismatched cases following the first
  faint. Version 2 captures queue input explicitly; version 3 adds actual move
  conditions. Historical versions 1 and 2 remain accepted and all prior captures
  replay exactly. No source queue observations were normalized away.
- All 18 compiled implementation/caller faults are caught, including missing
  parent gate, extra user gate, rounding, resnapshotting, processing order, HP cap,
  missing/wrong HP changes or numbers, shield clamp, unwanted animation, faint,
  boss boosts, shield index and owning dispatch. Twenty-one malformed captures
  are rejected. A first mutant had invalid C syntax and is excluded; its corrected
  executable is detected by the same native records.
- Retained doubles are 1,976/1,975/1,984, mixed 1,984/1,986/1,994/1,998: 11 new
  exact cases, no lost covered IDs. Spectral Thief's retained raw capture remains
  exact. Normal game-core tests pass, with matching saved/unsaved digests.

These are bounded observations. They do not prove every endure/item/passive or
fusion interaction, natural high-wave encounters or the full callback inventory.
Keep normal app/custom/save tests and exact-head 3DS CI as merge gates; skip only
the dedicated custom-only Task63 workflow. Ability-changing callbacks and the
remaining completion checklist stay open.
