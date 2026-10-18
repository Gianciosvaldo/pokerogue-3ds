# Final-boss transition: bounded core implementation

Base: merged #280, `cd8eef4151a4f11b7dab2aab3d84925196223645`.
Source: `8555c08c823b856cbec4eb99ca84ea52a955836d`; English locales:
`270ed0a2938b2c3e2e34e7de940e84aecadf548b`; Node 22.

`BattleScene.initFinalBossPhaseTwo` now has a native controller: its enemy/boss/
form/segment guards, non-transferable Mini Black Hole creation, fixed Eternatus
phase-two moves, manual Eternamax form queue, doubles flag and second-player
position/summon/post-summon queue. DamageAnim and PostTurnStatusEffect ownership
uses the Source Classic/Challenge final-wave predicate, rather than wave 200
alone. The form phase recalculates HP/stats and emits the Eternamax message.
The post-summon queue can consult a party slot before Summon puts it on field,
as Source's getPlayerField/getEnemyField do.

PokemonMove.ppUp is signed in permanent, temporary and detached move storage.
Eternatus Recover preserves -4 and one maximum PP through use, moveset copying
and the native constructor/save adapter. Existing 0..3 PP bytes keep their
representation. Save validation rejects unsupported negative or excessive PP
Ups. The game-core constructor/use/save fixture passes 44 checks, including
malformed PP metadata rejection in Classic and Endless.

The form-change callback Set tracks getPassiveAbility().id even when hasPassive
is false. The old code used the enabled passive projection instead; native
Eternatus exposes the difference (Neutralizing Gas recorded, not activated).
This correction does not enable that passive on the Classic boss.

Actual Source evidence:

- 16,384 controller/owning-phase cases compare guards, mode/wave eligibility,
  actual input party/field membership, raw move/PP metadata, new held item,
  hierarchical queue order, summon field/side/party arguments, dynamic activation
  queue and RNG. There are 296 transitions. Eighteen Challenge passive-access
  calls retain their explicit existing `wave` refusal; they are not supported
  coverage. The status-owner slice executes the no-status/early-end boundary;
  damaging poison/burn/toxic at the boss boundary needs its own proof.
- 16,384 actual controller -> QuietFormChangePhase.start -> PokemonMove.usePp
  cases (8192 at seeds 777/2034) compare Eternamax HP, six stats, ability index/
  IDs, disabled passive state, callback Set, exact English text, four moves'
  raw PP adjustments/used/max PP, and RNG. Levels are 1..255, with randomized
  IVs, nature, HP and PP consumption. This is not wide-level Endless coverage.
- UI/audio/assets/timing transport is stubbed; Source gameplay methods and
  expectations are unchanged. Full-capture footers/IDs are required.

Initial fixture repairs carried actual field membership, used a dummy display
child to avoid the test mock's index-zero getIndex bug, and removed duplicate
sprite additions between cases. The form fixture disables the framework's
Ball Fetch override and regenerates the ordinary form's name before each
transition. Otherwise its stale prior Eternamax name and global override were
unrepresented inputs, not native runtime errors.

Reproduction: copy the two `.producer.test.ts` files into a clean pinned
upstream's additive `test/tests/zz-dump` directory and run Vitest with Node 22.
`FINAL_BOSS_N`, `FINAL_BOSS_SEED` (form producer), and `FINAL_BOSS_OUT` control
captures. Encode controller JSONL using `tools/build-final-boss-proof.py`, then
run `build/final-boss-test`; replay form text with `build/final-boss-form-test`
and `TURNTEST_LOCALES` set to the pinned English locale directory. Compile-fault
and malformed-input checks use `tools/final-boss-guards.py`.

This is a prerequisite, not a completed Classic final boss or doubles release.
Mini Black Hole transfer execution and game-core inventory persistence, the
second player command adapter, natural double encounter start, phase-two save/
recovery, broader challenge context, and authentic legal-start clears remain
open. Existing item/doubles/Challenge unsupported accounting stays in place.
Run-core is unchanged. No hardware/emulator readiness follows from this slice.

Nine compiled faults are caught, including missed party-slot lookup and inactive
passive bookkeeping; twelve malformed captures are rejected. The complete
game-core host suite passes (paired 200-run digest `d62eb254`), including the
32,956-check enemy moveset adapter. Its former unsigned workaround assertions
now require actual Source raw -4 metadata and no maximum-PP override.
App/retained and exact-head CI results are recorded after completion.
