# HP-cost stat boosts

Source pin `8555c08c823b856cbec4eb99ca84ea52a955836d`, locales
`270ed0a2938b2c3e2e34e7de940e84aecadf548b`, Node 22.23.3.

Belly Drum, Clangorous Soul and Fillet Away share
`CutHpStatStageBoostAttr.apply`. It first pays
`toDmgValue(user.getMaxHp() / cutRatio)` through `damageAndUpdate` with an
INDIRECT result, then calls the inherited stat-change implementation. Thus a
raw invocation can pay HP and faint before the parent HP gate declines the
boost. Move eligibility is separate: the producer observes actual conditions
at sequence 2, 3 and 4 instead of imposing them on the raw override.

The port retains this order and the existing damage/faint/boss-shield pipeline.
Belly Drum's queued stat phase records its custom message using existing fields
and evaluates it on the current recipient when the phase runs. It uses Attack
+12, Clangorous Soul five stats +1, and Fillet Away Attack/Sp. Atk/Speed +2;
Simple, Contrary and caps continue through the shared stat phase.

The source's `getMaxHp()` calls `getStat(HP)` with its default bypass of temporary
stat overrides. Correct both the shared phase maximum-HP helper and Pain Split's
heal cap to use this getter. Actual public `setStat(HP, ..., false)` controls
expose these defects; combining a boss shield with the override makes the shared
helper's old result observable. The preserved pre-fix library matched 12,677 of
12,800 raw cases and failed 123 of the appended 128 boss/override controls.

Reproduce with `tools/hp-cost-boosts-diff.sh UPSTREAM 777`, then 2034. This checks
the source and locale pins, requires Node 22, runs the registered source
callbacks and actual full turns, compares all cases and compiles real faults.
Native `BattleScene.prototype.randBattleSeedInt` is captured before GameManager
construction and restored after natural double-battle startup. Source gameplay
methods are unchanged; UI transport is stubbed at the raw callback boundary.

Raw protocol 3 has 12,928 cases per seed, retaining all previous protocol 1/2
cases and adding 128 actual Wonder Guard/Shedinja one-HP controls. Both seeds
match exactly: 25,856 complete raw cases, zero unsupported cases. Per seed:
7,764 declined native returns, 8,044 failed move-condition groups, 256 boss
controls, 128 Enduring tags, 256 HP overrides, 5,724 queued faints, 12,928 damage
animations and 5,398 stat phases. C dispatch returns void; native boolean returns
are recorded without claiming C return-value parity.

The comparator retains every actor's 125-field input/output frame, boss and tag
state, all queued phase levels/order and damage/faint options, dynamic stat
payloads, message presence, move conditions and both RNG states. Enduring tags
are instantiated by the actual source tag factory; tag setup presentation is
outside the raw boundary. Native PhaseTree.clear's retained deferredActive flag
is captured explicitly. Source tag enum values are strings and are compared as
such, without rewriting captured observations.

All 16,896 owning turns match exactly, with zero source exceptions or unsupported
cases. Per seed, 128 temporary-HP controls and 128 one-HP Wonder Guard controls
invoke actual cost callbacks. Successful native Belly Drum/Clangorous Soul/
Fillet Away callbacks number 2,460/4,838/3,202 for seed 777 and
2,445/4,840/3,192 for seed 2034. All established ally/spread/faint/switch floors
remain enforced. The earlier 8,320-turn seed-777 capture also replays exactly.
Fresh GameManager startups can assign different actor IDs; captures keep these
native IDs without normalizing them into an older run's observations.

The first added one-HP owning attempt failed its fixture assertion because the
framework's global Ball Fetch override prevented Wonder Guard's native HP
calculation. Those incomplete attempts are excluded. The corrected fixture
clears that override only in the appended controls before calculating actual
stats; no source gameplay method or expected observation is replaced.

All 24 compiled faults are caught, covering absent/early/wrong-recipient costs,
rounding, divisors, minimum-one HP, temporary HP getters, indirect damage,
faints, post-cost HP gating, boost stages/recipient/source, custom message
payload and execution, spurious RNG and owning dispatch. Twenty-two malformed
captures are rejected. The expanded Pain Split proof independently catches
19 compiled faults and rejects 21 malformed captures.

Retained doubles are 1,986/1,986/1,988 and mixed turns are
1,994/1,993/1,994/1,998: twelve additional exact cases, no lost covered IDs.
Earlier HP-cost and Pain Split protocol captures, Pain Split owning turns,
ability-changing callbacks, Spectral Thief, berry moves and wide AI/Substitute
captures replay exactly. Normal battle entry controller and game-core tests
pass; saved/unsaved game digests both equal `361c1d0e`. The full normal app suite
passes with ASAN/UBSAN/LSAN, including custom and save/resume tests.

This evidence is bounded: it does not establish full battle-core completion,
all self-cost ability/item/passive interactions, natural high-wave progression
or hardware acceptance. The remaining source inventory and other maximum-HP
callers still require audit. Normal app/custom/save tests and the exact-head
3DS build remain merge gates; only dedicated Task63 is skipped.
