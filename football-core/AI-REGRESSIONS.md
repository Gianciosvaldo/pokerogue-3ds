# Receiver, protection and live Mega regressions

Receivers follow the called polyline from their actual snap positions. The
route controller projects onto the path and looks ahead by distance along it,
so nearly lateral PA Cross legs are not skipped by advancing on x alone. Inside
means toward midfield from the receiver's starting side, rather than slot
parity. Previously WR1/WR2 slants turned toward the sidelines, routes used
slot-number depth offsets, and the anti-jam release could keep running sideways.
The corrected controller continues downfield after the final route point;
a thrown-ball target still tracks the ball's landing point.

A temporary sidestep holds clearance around an obstructing defender, stays near
the called path, and rejoins it after passing. Its small actor state is local to
the snap and cleared by formation setup; checkpoints rebuild actors, so the save
format is unchanged. Pass man coverage aligns and tracks with lateral leverage
and a lead outside the 1.3-yard body diameter, rather than aiming back into the
receiver's body. Run coverage/pursuit, collision size, strength and acceleration
are unchanged. Eligible blockers on a run release downfield if no defender is
within assignment range; they do not remain stationary with an empty assignment.

Pass protection still uses the shared deterministic assignment from the first
fix: rushers with fewer reachable blockers are prioritized, assignments are
unique, and physical engagement honors them. Existing strength contests,
shedding, cooldowns and special teams remain. Protection can lose a matchup;
it does not guarantee a clean pocket.

`movement-regression-test` retains the original backward-motion, speed/contact
and edge-protection tests. It now has 1015 checks: all four passing routes, all
five eligible slots, slow/fast attributes and actual generated Electrode
attributes, both field directions; full man/zone/blitz formations; no accidental
pre-pass block engagement; downfield run-blocker releases; and release-state
reset at the next snap. Unopposed trajectories must stay within one yard of the
called polyline; covered releases must stay within 3.5 yards while making forward
progress. The previous tests isolated receivers and checked positive x movement,
which did not establish route fidelity in actual formations. Compiling the
expanded test against the previous release produces 285 failures. Only the
receiver helper's const qualification is adapted to compile that old behavior
against the current header; its movement/coverage behavior is unchanged.

The unchanged AI test runs 40 seeded games twice for determinism, another ten
seeds, and situation/mix checks. Previous release: 2247 snaps, 19.2 points/game,
42% runs, 57% completions, 5.1% sacks, 2.4% interceptions. Corrected release:
2234 snaps, 29.1 points/game, 41% runs (31% outside), 59% completions, 2.5% sacks,
1.7% interceptions. All original balance bounds remain. These seeded samples
establish regression coverage, not complete AI balance or device performance.

See `MEGA.md` for first-stone availability, fixed-holder UI and initial-formation
activation. Movement and live-career Mega tests are included in the normal
football suite and CI sanitizers. Host UI recordings check holder/name, preview,
purchase and the unavailable-holder explanation. Hardware visuals/performance
require a later device run; no new Windows builder is part of this fix.

The candy regression retains all eight complete career games and every credit,
profile, save/resume and duplicate-payment assertion. Its former AI-dependent
sampling condition is replaced with 24 deterministic checkpoint cases: each of
the three credit kinds in each of the eight career fixtures, delivered through
the live credit and career owner. Each checks unflushed profile isolation,
duplicate delivery, save isolation, exact payment and duplicate flush.
Natural-game reward counts remain diagnostic.
