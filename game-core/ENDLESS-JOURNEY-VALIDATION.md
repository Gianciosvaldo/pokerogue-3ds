# Wide Endless encounter and save/resume chain

The generated enemy stock now initializes `non_transferable` using the same
constructor policy as player stock. A fresh enemy's base stat boosters previously
stored false, while load repaired the flag to true. Battle conversion already
applied the lock; this fixes the inconsistent owning stock/save representation.
The retained [source constructor-lock proof](HELD-LOCK-VALIDATION.md) establishes
the key policy at upstream `8555c08c823b856cbec4eb99ca84ea52a955836d`.

## Chained regression

`endless_journey_host_test.c` retains eight fixed seeds in both Endless modes.
Prepare a Mewtwo at level 65,535 with EXP one point below 65,536, then invoke the
real new-battle queue at wave 5,701. This is a synthetic player/wave API setup;
it does not represent playing the preceding 5,700 waves or their accumulated
inventory. Opponent selection, levels, fusion, boss segments, movesets and held
stock are generated normally. No enemy HP/stats/moves are overwritten.

At the initial command boundary, save/load must preserve every serialized byte,
with no dropped data. Play the saved and unsaved runs with identical commands;
compare screens, party/enemy identity, form/fusion/ability, levels/EXP, HP/status,
stats, moves/PP, money and both RNG streams after every choice. Refuse all
unsupported/fallback diagnostics. Check transported EXP and level-up event
metadata. Skip the naturally offered reward and compare the next generated
encounter and its complete save bytes after a victory. Preserve losses as well.

All **16 contexts and 1,750 assertions** pass: ordinary/spliced wins 4/1 and
losses 4/7, one double per mode, four initial boss contexts per mode, 104 locked
stock observations, seven EXP and six level-up events. Both modes include the
65,535 → 65,536 transition and a naturally generated next encounter. The input
inventory never drops a losing seed. Removing the constructor flag makes the
initial locked-stock check fail. The retained constructor-lock suite also passes
2,632 native observations and 39,495 bridge/UI/save checks.

This is a joined C owning-caller/save regression, **not a new native full-run
comparison**. Exact native wave, constructor and progression evidence remains in
[the wave sweep](../run-core/ENDLESS-WAVE-SWEEP-VALIDATION.md),
[constructor finish](CONSTRUCTOR-FINISH-VALIDATION.md) and the wide progression
records. It does not prove all generated battles or confirm a win strategy for
bosses. High-level samples do not execute learning/evolution; those integration
contexts, complete natural token history, terminal/display acceptance and the
broader source ledger remain open. The other agent owns abilities, forms,
boss shields, admin/team-boss movesets, fusion niches and trainer Tera.

Run `make -C game-core endless-journey-host-test held-lock-host-test`.
Normal core/app/custom/save/football and actual 3DS/ARM stack CI remain merge
gates; only dedicated Task63 is skipped. No builder or full completion claim.

The later [lifecycle integration](ENDLESS-LIFECYCLE-VALIDATION.md) retains all
16 contexts and adds played learning/evolution and final-wave chains. Its own
synthetic setup and native/full-history/presentation limits remain explicit.
