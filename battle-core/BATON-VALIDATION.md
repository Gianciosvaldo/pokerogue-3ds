# Baton inventory and switch callback

Reference: upstream `8555c08c823b856cbec4eb99ca84ea52a955836d`, locales
`270ed0a2938b2c3e2e34e7de940e84aecadf548b`, Node 22.

Baton Pass now forwards the canonical Baton held item during SwitchSummonPhase,
after opposite-side tags change source and before the party swap. The native
branch transfers one item only when the incoming Pokémon has no Baton object.
Both native findModifier calls omit their player argument: they search player
inventory even for an enemy switch. This pinned behavior is preserved. A locked
donor is still transferred; the shared transfer helper does not check that flag.
The clone resets its lock, normal modifier updates run, and itemLost=false avoids
Unburden. Ordinary, forced and Shed Tail switches do not run this branch.

The canonical stock has cap 1, class-only matching and tier 3 in all three pools.
Game-core projects and reimports its owner, lock, count and cached tier using the
existing named-field save format. Unknown parameters and virtual/noncanonical
stacks retain explicit unsupported reporting.

## Validation

The native producers capture BattleScene.prototype.randBattleSeedInt before
constructing GameManager and restore it after natural battle startup. Gameplay
implementations and expected outcomes are not replaced.

- 256 native SwitchSummonPhase branches cover both sides, both field slots, four
  switch kinds, donor/recipient presence and both locks. Presentation is stopped
  only after the native transfer branch and party swap. The comparison checks
  complete ordered inventory and metadata, six actors' HP/stats/Unburden, both
  RNG streams and unsupported/deferred flags. Version 2 adds actual native
  canTransferHeldItemModifier results and three native tier observations;
  all original version 1 cases remain supported and exact.
- 2,050 complete native doubles turns compare state, phase traces, text and RNG:
  all covered and exact, no native exceptions. They exercise actual Baton Pass
  commands, two available reserves, donor/recipient locks and presence, ordinary
  faint switches, 128 Explosion multi-faints and two spread targeting controls.
  The complete turn matrix uses wild enemies without reserves; the enemy lookup
  quirk is checked by the separate native phase cases above.
- Seven compiled faults are caught: wrong inventory side, wrong recipient/donor
  owner, adding a lock gate, triggering item loss, removing the owner hook and
  running it on other switch kinds. Eighteen malformed captures are rejected.
  Merely deleting the recipient-presence check is equivalent for canonical full
  cap-1 stock and is not counted as a detected fault.
- Real game-core commands, inventory synchronization and saves pass 3,072 checks.
  Baton Pass and ordinary switches replay after an initial legal save; item
  ownership and flags survive the next legal encounter save. The test finishes
  the battle to reach that save point rather than permitting mid-turn saves.
- Retained Grip Claw callback/turn, shared scene, theft and Mini Black Hole
  source corpora remain exact. Their 52 compiled faults and 72 malformed
  controls remain effective. Three established doubles corpora retain covered
  counts 1,994/1,996/1,998 with zero mismatches; no coverage floor changed.
- Full game-core, normal app/custom/save tests and the actual 3DS build pass.
  The saved/unsaved game-core digest stays b7cf2078. The strong 524,288-byte ARM
  stack override is present; 2,252 frame reports pass, largest 45,056 bytes.
  Exact-final-head CI remains required before merge.

Reproduce with Node 22:

```sh
battle-core/tools/baton-diff.sh /path/to/pinned-upstream 777
TURNTEST_LOCALES=/path/to/pinned-upstream/locales/en \
  python3 battle-core/tools/check-baton-proof.py \
  battle-core/build/baton-switch.txt battle-core/build/baton-turns-777.txt
make -C game-core baton-host-test
```

## Remaining scope

These bounded canonical-stock observations do not establish full battle-core
completion. Manual party switching in game-core still chooses an ordinary switch;
this change does not add the optional Baton item action to that UI. Pickup's loot
identity/cache and post-battle owner, remaining stock families, Eviolite cloning,
shared mutable modifier-type identity and broader save/fusion interactions remain
separate work. Existing capacity and unsupported-class guards remain active.
