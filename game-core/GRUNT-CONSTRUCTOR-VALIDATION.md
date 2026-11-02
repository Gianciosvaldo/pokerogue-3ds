# Native grunt trainer constructors

Pinned upstream `8555c08c823b856cbec4eb99ca84ea52a955836d`, locales
`270ed0a2938b2c3e2e34e7de940e84aecadf548b`, Node 22.

Fresh Classic grunt constructors now use the existing native-weighted trainer
moveset owner instead of the counted last-four-level-moves fallback. Admit only
Rocket, Magma, Aqua, Galactic, Plasma, Flare, Aether, Skull, Macro and Star grunts.
All ten actual source configs have genders/doubles, no egg pools, no boss flag
and no member callbacks. The interleaved admins and bosses remain excluded.
Require a valid default/female/double variant matching the battle context, plus
all existing fresh-actor, no-Tera, no-fusion/passive/form-callback guards.

## Native comparisons

Observe **1,200 first members plus 1,200 complete parties**, or **6,000 actual
constructor sequences**, through native `Trainer.genPartyMember`. Cover all ten
configs, all three variants, waves 35/112, levels 24/25/40/41/55/71/101/121/170/201,
and two samples of each input. Full parties contain two or six members; 400 are
doubles. Levels are controlled API inputs and are not claimed as naturally rolled
levels at those waves. Every original ordinary/Gym capture remains unchanged.

Capture native battle RNG before GameManager and restore it after startup.
Assert real moveset and power methods, exactly one native moveset generation and
no addEnemy boss, data-source or postProcess arguments. Capture source config
facts and reject callbacks, boss/passive, planned Tera and non-fresh inputs.
Compare base metadata, generation-time stats/HP, ordered moves/PP, constructor
finish/IV rerolls, full-party stat refresh and complete member/ambient RNG streams
through the real C party constructor. The runner passes **423,656 assertions**,
including 25 atomic context/error guards. No fallback or unsupported diagnostic
is accepted. Full encounter item generation, trainer dialogue and played battles
are separate from this constructor proof.

Fresh native JSON and its complete generated C transport reproduce byte-for-byte.
Twenty-one compiled wrong owners are caught with three pristine controls;
18 malformed/forged captures are rejected. The original ordinary-trainer
247,371 assertions and Gym 128,928 assertions pass, as do all 22 retained ordinary
faults. Source pools, ordering, weights, caps, slots, callback ownership, decline
rollback and variant admission retain their controls. The ordinary proof's
Young Couple/same-seed controls remain in its unchanged suite; those unrelated
config branches are not claimed by the new grunt matrix.

Scoped ASan/UBSan/LSan passes with the moveset owner, encounter constructor,
run-core party continuation and new reader instrumented; retained libraries are
otherwise unchanged. Existing production stat and RNG ordering is preserved.
No threshold, historical case inventory or coverage floor is relaxed.

## Reproduction and limits

```
make -C game-core grunt-constructor-test grunt-constructor-capture grunt-constructor-faults
# Node 22, both pinned source trees tracked-clean:
game-core/tools/reproduce-grunt-constructors.sh /path/to/upstream /tmp/grunt-proof
```

This closes the fresh grunt moveset owner, not all trainer constructors or
battle-core. Remaining special trainer/postProcess/Tera/form/ability/fusion
integration and the broader source/unsupported-path ledger stay visible. Admin
and team-boss postProcess work has another owner; this does not replace it.
Normal core/app/custom/save/football and actual 3DS/ARM stack checks are merge
gates; only dedicated Task63 is skipped. No builder is produced.
