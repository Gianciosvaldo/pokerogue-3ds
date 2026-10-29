# Manual Eviolite cloning, full-add fallback and saved classes

Pinned upstream: `8555c08c823b856cbec4eb99ca84ea52a955836d`; locales
`270ed0a2938b2c3e2e34e7de940e84aecadf548b`; Node 22.

The reward-screen transfer callback calls `tryTransferHeldItemModifier` with
`itemLost = false`. That owner **clones** the original modifier. Eviolite's
`EvolutionStatBoosterModifier` inherits `StatBoosterModifier.clone`, so the
recipient gets a base stat booster whose effect does not require an unevolved
holder. C previously copied the original evolution-only class marker instead.
The retained counterexample transfers to an empty recipient and observes
`eviolite_base == 0` in C versus the native base-clone class.

The source also uses two directional matches: it finds a recipient stack by
asking the existing modifier to match the **original** donor, then adds the new
clone using the clone's match against existing stock. An original Eviolite does
not match a base clone, while the base clone matches either class. Thus a base
clone transferred to a recipient holding an original full Eviolite consumes the
donor, fails the later add, and grants the cached tier's default ball reward.
It leaves the recipient's original modifier in place. This source behavior is
retained, including its successful callback return and full-stack message.

A common held-clone helper now resets constructor/transient fields and retains
Eviolite's inherited clone class for both extra-party creation and transfer.
The manual transfer owner preserves directional matching and performs the
source's full-add fallback. Fallbacks use the five pinned player-pool defaults;
an absent tier uses Common. Existing direct cap refusals still refuse without
consuming the donor. The forced DNA Splicers transport shares this helper;
its separate retained replay remains required.

## Native evidence

The producer captures native battle RNG before GameManager construction,
restores it after startup, and invokes actual scene transfer functions with
presentation bypassed. Seventy-two cases cover Bulbasaur/Venusaur recipients,
original/base-clone donors, empty/original/base-clone recipient stock, absent
cache plus all five explicit tiers. All cases are retained. Native
`canTransferHeldItemModifier` results are diagnostic metadata: this proof
compares the transfer callback, not complete party-menu eligibility. In
particular, native eligibility can say yes when the later callback refuses.

The C replay invokes the actual `RP_SELECT_MODIFIER`/`GC_CH_TRANSFER` owner.
Its 65,544 checks compare callback success, party fields/IVs/stats/HP/PP, stock,
class markers, tier caches, ball counts, profile fields and both RNG streams.
Full-stack messages render against the three captured, pinned English locale
namespaces and must match native text exactly. Native `PokemonData` and
`ModifierData` JSON reconstruction is compared with legal C command saves:
class markers survive, cached tiers reset, and no fields are reported dropped.
This is an owner replay with a legal command scaffold, not whole encounter or
all held-item interaction parity.

The checker catches eight compiled implementation faults, eleven malformed
captures and four forged observations. A fresh capture reproduces both JSON
and generated fixture byte-for-byte. Scoped ASan/UBSan passes on the edited
item owner, saves and replay; supporting archives are not instrumented.
Retained Shedinja (2,400 cases), shared-IV rewards (16 cases), and DNA Splicers
(600 cases, 463 fused/137 refused, zero mismatches) pass after the shared helper
change. Normal app/custom/save tests and the ARM build remain required in CI.

```sh
make -C game-core manual-eviolite-host-test shedinja-host-test iv-alias-host-test
make -C game-core fusion-host-test fusion-replay
python3 game-core/tools/check-manual-eviolite-proof.py
game-core/tools/reproduce-manual-eviolite.sh /path/to/pinned/upstream /tmp/manual-clone
```

The basis contains legal one-stack Eviolites, distinct nonzero holder IDs and
no virtual stock. Arbitrary constructor parameters, zero-ID wildcard behavior,
other class-matching families, all mixed item histories, complete menu selection,
other languages and hardware acceptance remain separate obligations. No
Windows builder is produced.
