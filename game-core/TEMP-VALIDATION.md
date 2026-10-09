# Canonical temporary booster caller — 2026-10-08

Pinned upstream `8555c08c823b856cbec4eb99ca84ea52a955836d`, Node 22.
Game-core now calls reward-core's canonical temporary add/refresh/lapse API for
X Attack/Defense/Sp. Atk/Sp. Def/Speed/Accuracy and Dire Hit. Real stacks 1–2,
side-wide owner, stat 1–6 (Dire arg 0), duration 1–5 are the bounded domain.
Named `battle_count` is transported in reward context and saved per item;
missing fields in older saves default 0 and stay opaque, without invented expiry.
Virtual, held/detached, invalid duration/stat/stack, unknown keys, challenge or
unrepresentable inventory contexts refuse atomically. Other records/order remain
intact. A no-temporary list is a no-op even with unrelated opaque stock.

Scene.addModifier's first-match refresh retains its real stack; full duration 5
substitutes the PLAYER pool's default ball for the offer tier and emits the
itemStackFull event. Its persistent branch mutates inventory but returns false:
a paid selection therefore retains the wallet and reports an error, while free
selection completes. A full-stack ball replacement returns true and the paid
caller deducts. The adapter follows that pinned behavior. Translations/sound/
UI timing are not compared; the focused source checks message presence and the
caller result/money, not complete rendered message parity.

The actual run BattleEnd handler calls the bounded lapse after enemy held cleanup
and the existing post-battle money/ability boundary, before reward selection.
Queued duplicate BattleEnds are culled and only one lapse happens, including
non-victory/flee. Trainer-member knockouts are not separate BattleEnd events.
Lure and other lapsing families, full post-battle abilities/Pay Day/cleanup,
challenge and unknown inventories remain outside this slice.

**Temporary modifiers still report unsupported on full battle turns.** Their
existing raw stat/accuracy/critical helpers receive bounded projections; the
battle-core full-turn whitelist has not been expanded or its flags stripped.
This task proves raw helper inputs and the bounded caller/timer lifecycle,
not full-turn or whole-run upstream parity. No battle-core files changed.

## Source checks and guards

```
# Activate the environment's Node 22, then for each seed 777/9001/4242/31337:
game-core/tools/tempdiff.sh /workspace/shared/upstream-lane-c 2000 SEED
make -j2 -C game-core test
game-core/build/gcplay --auto 1000 --jobs 8
game-core/build/gcplay --auto 1000 --jobs 8 --save-test
```

The owning reward temporary producer stays unchanged. Four established 2k traces
replay through new game transport/projection: 8,000 exact inventory/effect/RNG
cases and all ticks, including first matching duplicate entries and raw stack 2.
The independent game raw fixture adapts its own capacity guards to the 96-entry
projection boundary; the original reward harness/guards remain unchanged.

The separate new actual Scene.addModifier / SelectModifierPhase.applyModifier /
BattleEndPhase.start producer executes 4×2,000 cases. Presentation refresh,
phase completion, enemy graphical destruction and shop UI/audio are headless
boundaries; the add, purchase result/wallet, tier replacement, queue culling,
lapse/removal and real ModifierData reconstructions execute upstream code.
Each seed includes 667 paid cases and 497 full-stack cases. Actual BattleEnd ticks
8,062/7,886/7,901/8,075 compare through the real C handler (normal and loaded
copies). All 8,000 callers match stock/duration, ball deltas, result/money/error,
queue culling and unchanged RNG. Source ModifierData preserves each remaining
1–5 duration/stack; C named-field saves load at the legal encounter boundary.
Saving immediately after BattleEnd remains forbidden, as the existing contract
requires. Full-turn unsupported flags are explicitly asserted unchanged.

Actual C source mutants omitting the BattleEnd lapse caller, timer save assignment
and temp projection class each fail (exit 1); missing completion footer rejects
(exit 2). Additive host guards cover negative/overflow counts, timerless/malformed
stock, atomic refusal, unrelated opaque no-op, and each timer 1–5 save/load/lapse.
Original Mint/Seed/Lens saves still load at their original 113/130/199 checks.
New host: **90 checks**; raw transport guards: **25**. All 32 established Mint/vitamin/Money/Orb/Seed/Lens/Golden/Multi source replays
retain coverage. Four seeds' apply/candy/ball/options outputs are byte-identical.

## Host consistency and limits

Before/after 1,000 normal +1,000 saved runs all reach terminal screens, with zero
stuck/crash/timeout: turns 49,206→50,394, paired digest `15d6d01d`→`9e0f75df`;
round trips 14,291→14,679, maximum save 11,400→11,472 bytes. The paired 200-run
host digest changes `2f376abe`→`74adeffb`. Canonical temporary effects, refresh,
finite expiry and full-stack replacement change later battles/reward trajectories;
the named save field adds storage. The digest hashes outcomes/counters, not
all source state, so equality remains host consistency rather than parity proof.
[All changed soak rows](TEMP-SOAK-DELTA.md) retain exact before/after counts.
Post-battle counted refusals rise 3 runs/4 occurrences→167/1,380 because unsupported
mixed contexts/timerless stock now explicitly report refused lapse. Reward
stand-ins rise 139/169→145/178 with changed trajectories. No zero-fallback claim.

Full Tasks 96/104 and 64/65/66 remain incomplete; broader stat/challenge/form/
virtual/held lifecycle, full upstream run, hardware and presentation are separate.
No console/emulator result is claimed; exact-head host and 3DS CI gate merging.
Durable evidence: `/workspace/validation/lane-c/ten/temp/`.
