# D10 positive battle friendship caller

Base d21ef10bdf5507508dcee5b082ec27856d62f4b5; clean start, GitHub main
and open PRs checked (none). D09 merged #201. Source
8555c08c823b856cbec4eb99ca84ea52a955836d; Node22 required.

Evidence recorded before production edits:

|Source symbol|Behavior / intended C boundary|Fixture|
|---|---|---|
|BattleScene.applyPartyExp battle-scene.ts3299–3343|positive +3 only participating survivors when defeated; use real Victory caller|reordered party, empty participant set, fainted/nonparticipant|
|Pokemon.addFriendship pokemon.ts6080–6129|nonpositive short circuit; singular modifier; full boosted amount enters Pokemon and candy before caps|199/200/254/255, exact cost thresholds, refused candy|
|PokemonFriendshipBoosterModifier modifier.ts2596–2621; Scene.applyModifier|first matching wildcard/exact record, floor base*(1+.5*stack)|none, stacks1–3, duplicate/wildcard order/wrong holder|
|GameData.addStarterCandy game-data.ts1807–1819|cap9999, refused gain clamps progress to cap-1|9998/9999 candies|
|TimedEventManager.getClassicFriendshipMultiplier timed-event-manager.ts106|ordinary multiplier3, nonClassic1; event explicitly fixed in fixture|Classic/Endless|
|gc_system_add_friendship gc_system.c277–309 and Rare Candy gc_reward.c766–775|already owns ordinary progress/caps; preserve separate Candy overwrite and loss APIs|unchanged candy and nonpositive controls|

Planned adapter is read-only Bell inventory projection and a +3 battle-only
call. Fused/event/challenge/unrepresentable contexts retain the legacy gain,
with a counted fallback. No changes to shared friendship API, Rare Candy,
reward-core, generated data, ribbons or achievements. The final source and host results below replace the initial pending status.

## Executed proof

`source /workspace/shared/activate-parity.sh` (environment-specific Node22.23.3),
then `game-core/tools/soothe-battle-diff.sh UP 2000 '777 9001 4242 31337'`.
All four sources completed2000 each without throws/skips; final rebuilt replay
is8000/8000 exact,0 differences. Classic/Endless each1000 per seed, party1–6,
reorder/faint/nonparticipant controls; all eight Bell stock groups rotate.
Pokemon54 fields, unchanged complete inventory/wallet/global and battle RNG,
root candy/progress and all other profile fields compare. Source distribution
queues nothing at these level100 fixtures; actual C Victory additionally queues
exactly its existing RP_CONTINUE transport and no EXP/events. This proves the
friendship distribution boundary, not experience, full Victory scheduling,
ribbons/achievements or a complete legal run.

|Seed|Changed Pokemon|Boosted >3 actual delta|Capped starting254/255 survivors|Refused candy cases|Separate nonpositive/capped controls|
|---|---|---|---|---|---|
|777|2474|485|2406|181|1334/666|
|9001|2501|512|2361|178|1334/666|
|4242|2540|530|2420|180|1334/666|
|31337|2464|514|2345|185|1334/666|

Separate actual Pokemon.addFriendship controls with Bell removed execute and
compare -1/0 and capped+6 through the unchanged shared C API. Unchanged raw
Bell2000×4 plus acquisition2000×4, Rare/Rarer Candy2000×4, apply and balls2000×4,
existing Orb/Wide/Multi2000×4 all replay unchanged with0 differences. Retained
source traces are explicitly reused, not newly generated in those regressions.
33 new host guards pass; fused/challenge, detached/virtual/malformed, orphan
holder, duplicate IDs/order refuse before mutation. No-Bell +3 unchanged.

42 malformed traces refuse2 after intact positive control0. Two actual rebuilt
runtime mutations, duplicate gain and candy progress from capped delta, both
exit1 against unchanged source; intact exits0. Exact differences appear in
`d10-mutations-final.log` below; no expectation mutation.

Baseline H and final rebuilt gcplay H:1000 normal and1000 saved ended each,
0 clears/stuck/crashes/timeouts,50458 turns, matchingf98028d9;14710 save/load
round trips, largest11472 bytes. `make -j2 -C game-core test` passes including
new guards; paired host8711f1c7. The first final soak used an already-linked
binary; it is superseded by explicit `make ... build/gcplay` and fresh normal/
save checks recorded in d10-final-normal/save.log. No forced digest equality.

Initial fixture failures were retained: singleton import resolved from actual
`global-event-manager`; undefined passive/pause fields produced NaN and were
fixed at setup (strict parser rejected them); C continuation was explicitly
asserted after inspecting actual Victory wrapper; an unsigned ID range passed
to a signed-int bound overflowed and was corrected. The fourth final wrapper
replay overlapped that parser rebuild and refused2; all four completed source
traces were then replayed against the final immutable binary, each2000/2000.
No case was removed or declared unsupported to hide a mismatch.

Reproduce: wrapper above; `python game-core/tools/soothe-battle-malformed.py EXE TRACE`;
`python game-core/tools/soothe-battle-mutations.py TRACE`; `make -j2 -C game-core test`;
`make -j2 -C game-core build/gcplay`; gcplay --auto1000 --jobs8 with/without
--save-test. GCC14.2.0. Evidence in `/workspace/work/remaining-2026-10-11/d10-*`
and ignored `game-core/build/soothe-battle-evidence.qzIN8h/`.
The known broad trainer4242case1051, ITEMS text1/1/2/1, legacy damage31337
coverage1999 and0 Classic clears remain unresolved. No wider ledger completion.
Exact-head ordinary host/3DS CI is required before merge; source conditional
jobs are not counted as passed.

Runtime mutation evidence:
```text
intact: exit 0
Soothe battle cases2000 exact2000 failures0 changedPokemon2474 Classic1000 Endless1000
double-battle-gain: exit 1
FAIL soothe battle case2 profile42/21 candy3/3
FAIL soothe battle case3 profile8/4 candy62/62
FAIL soothe battle case4 profile54/27 candy41/41
FAIL soothe battle case5 profile6/3 candy10/10
FAIL soothe battle case6 profile60/30 candy25/25
FAIL soothe battle case7 profile14/7 candy9999/9999
FAIL soothe battle case8 profile48/24 candy9998/9998
FAIL soothe battle case9 profile8/4 candy62/62
Soothe battle cases2000 exact668 failures1332 changedPokemon2474 Classic1000 Endless1000
capped-delta-progress: exit 1
FAIL soothe battle case2 profile3/21 candy3/3
FAIL soothe battle case3 profile0/4 candy62/62
FAIL soothe battle case4 profile12/27 candy41/41
FAIL soothe battle case5 profile1/3 candy10/10
FAIL soothe battle case6 profile3/30 candy25/25
FAIL soothe battle case7 profile0/7 candy9999/9999
FAIL soothe battle case8 profile12/24 candy9998/9998
FAIL soothe battle case10 profile21/42 candy84/84
Soothe battle cases2000 exact873 failures1127 changedPokemon2474 Classic1000 Endless1000
```

|Evidence|SHA256|
|---|---|
|soothe-battle.producer.test.ts|`32e306b05e6995a5e92f2fc221b4c634ef124d1fa247386a792314937a4d306e`|
|caller-2000-31337.txt|`4957aa3e5958b1d24010d0c55e2cd18cc5bafe4c849628844c392f27d6c8dd70`|
|caller-2000-4242.txt|`8feaa1630aaf4f4bdb21db3c93b11debe006ff56ab66123f215ff08eb69a3428`|
|caller-2000-777.txt|`344858f707fa1b1605e1e8266e6208f414e98a740ee864ef012041c149d314c4`|
|caller-2000-9001.txt|`b4c236ff382ce2efd830a76111e0a1db2545fcf60562a4f96f5ab9e317c86ab2`|
