# Additive endgame diagnostic chooser (Card 109)

`gcstress` is a separate host binary and policy. The legacy `gcplay.c`, its
chooser, `soak%08d` seeds and digest definition are byte-unchanged. The diagnostic
is coverage tooling, not source whole-run parity or a completed Classic-clear
claim. Run-owner Tasks 64–66 remain gated.

## Fixture and execution

Every diagnostic starts at wave 1 with `endgame%08d` seeds and the ordinary run
initialization. A single explicit fixture replaces starter selection: six level
200, IV31, neutral-nature, form-zero Pokemon with ordinary first abilities, full
normal move PP, no held stock, 10,000 money, five Poke Balls, 50 Ultra Balls and
25 Master Balls. This bypasses normal starter cost/availability and level caps;
it is a stress fixture, not a legal starting player profile.

| Species | Fixed moves |
| --- | --- |
| Mewtwo | Psychic, Aura Sphere, Ice Beam, Recover |
| Kyogre | Surf, Thunderbolt, Ice Beam, Earthquake |
| Groudon | Earthquake, Flamethrower, Dragon Claw, Thunderbolt |
| Rayquaza | Dragon Claw, Earthquake, Flamethrower, Ice Beam |
| Metagross | Meteor Mash, Zen Headbutt, Earthquake, Bullet Punch |
| Blissey | Flamethrower, Ice Beam, Thunderbolt, Soft-Boiled |

The fixture pushes the real initial `RP_NEW_BATTLE` once. From then on every
choice goes through `gc_run_choose`, actual screens and the phase queue. There
are no later direct HP/PP changes, healing, revival, teleports or wave increments.
Normal game rewards, shops, moves and biome healing remain actual game effects.
The deterministic policy scores damaging moves with stats/STAB/type matchups,
switches for unusable attacks or a badly hurt lead, uses real recovery moves,
and prioritizes offered healing/revive/PP items. It keeps fixture moves and
species, declines catches that would replace the full party, selects the first
legal mystery option and takes legal targetless rewards otherwise. Rejected
reward calls preserve their side effects and use the real skip; repeated shop
attempts have a 24-choice bound.

Each worker has a timeout and step limit. Every run reports seed, final wave,
turns/steps, actual screen, last enemy level/HP/boss segments/moves, surviving
party/remaining PP, action/refusal counts, reason and a gameplay digest. Crash,
timeout, step-limit, choice and transport failures are explicit. A timeout uses
the last completed wave snapshot, not a claim of exact final signal-time state.
The driver returns 1 for reported stuck/crashed/timed-out cases, even if all runs
reached the target; it does not count refusals as successful completions.

## What the 100-run diagnostic finds

Seeds 0–99, normal and save/load, both reach **wave 100+ in all 100 runs**. Every
run reaches wave 200. Results are **94 party defeats, six actual command
refusals, zero clear screens, zero crashes and zero timeouts**, with 25,825 turns.
The six refusals are seeds 9, 43, 62, 67, 90 and 91:
`battle:noPokeballForceFinalBoss`. The current COMMAND screen advertises
`can_ball`, but actual command validation rejects the final-boss ball. The
policy reports that boundary rather than bypassing it.

The final observed opponent is Eternatus (890), level 200, HP 1/832, boss segment
0 of four, with Eternabeam, Sludge Bomb, Flamethrower and Cosmic Power. At Classic
wave 200/form zero, battle-core's damage path keeps a minimum segment and clamps
damage to leave one HP. The required final-boss transition remains gated. These
are concrete late-game blockers; no clear is claimed. Remaining party PP is
reported independently, so this does not mislabel the general defeats as PP
exhaustion.

The report exposes 20,030 enemy-moveset stand-ins, 1,978 doubles played as singles,
2,791 weather-roll stand-ins, 2,744 ungenerated-enemy stand-ins, 15,850 post-battle
fallbacks, 1,588 trainer-title message fallbacks, 216 unapplied reward fallbacks,
2,100 fixed-classic and 644 trainer-config run-core flags, and 829 final-boss
unported observations. Their presence limits conclusions about gameplay parity.

## Persistence bug found by the diagnostic

The first 100-run pair found one full-state mismatch, seed 78 at wave 136:
`enemy_items` record zero was BERRY/arg10/stack1/holder2662926169, with `detached`
1 in the normal run versus 0 after reload. Every other named field matched at
that boundary, and final choices/outcomes matched. Newly generated enemy stock
rewrote key/arg/stack/holder but reused prior stock's detached/virtual/timer fields.
Reload zeroed unused slots, exposing the difference.

A fresh generated item now initializes the entire record before populating the
source-generated fields. Source persistent modifier constructors initialize
virtual stock to zero; old holder/timer state belongs to the previous object.
This does not change existing detached-stock transport or wildcard contracts.
A dirty-unused-stock versus clean fixture executes the real queue to wave 100
and verifies fresh generation and equal choices. Removing the initialization
fails this regression. Seed 78's named boundary and complete paired trace now
match. Before/after evidence is retained locally.

## Validation and digest

The diagnostic digest is new and separate. It hashes canonical named run and
system saves at each encounter save boundary (including party/profile/inventory
and RNG), every accepted choice and resulting party HP/status/levels/moves/PP,
wallet, both RNGs, and terminal profile/stub/unported state. It excludes UI events
re-emitted by reload. The comparator checks every per-seed line and all aggregate
stub/unported rows, not just an XOR aggregate; it cannot cancel two changed runs.

Final 100-run pairs match exactly: **digest `72f0120f`**. Saved mode performs
**18,820 round trips**, maximum **13,693 bytes**. The focused host suite has
842 checks; real two-run pairing, step-limit reporting and ten malformed/bounded
CLI requests run under `make test`. Isolated reload-HP, reload-profile, chooser,
short-worker-transport and removed-stock-initialization mutants are detected.

All 56 retained bridge/temp/Wide/Multi replays and four Classic terminal replays
(60 suites) pass, with all 16 application outputs unchanged. Established fixtures,
producers, comparators, counts and tolerances are untouched. True legacy owning
save tests retain 113/130/199 checks, and system/dex/save tests pass.

The final comparison starts from main `a6d175705206930c92e50bf975fcb3d1cdcb4cbd`,
which includes the independently merged Growth proof. Legacy H before/after is
byte-identical: **1,000 runs, digest `e789ef81`, 50,356 turns**, and saved mode
**14,686 round trips, maximum 11,472 bytes**. The paired 200-run digest is
`6741e0a8`. The earlier pre-Growth baseline was `5b6c9ee2`; that change belongs to
the Growth merge, not this diagnostic or item initialization.

```sh
make -j2 -C game-core all
make -j2 -C game-core test
# Exit 1 truthfully reports the six observed final-boss command refusals.
game-core/build/gcstress --runs 100 --jobs 8
game-core/build/gcstress --runs 100 --jobs 8 --save-test
python3 game-core/tests/endgame_diagnostic_checks.py game-core/build/gcstress 100
game-core/build/gcplay --auto 1000 --jobs 8
game-core/build/gcplay --auto 1000 --jobs 8 --save-test
```

Environment-local evidence and retained replay mapping/commands:
`/workspace/validation/lane-c/eight/endgame/`. Committed fixtures provide portable
focused checks and replay pairing; source clear/boss correctness is not inferred
from diagnostic reach alone.
