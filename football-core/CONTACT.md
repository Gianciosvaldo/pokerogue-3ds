# Blocking and carrier contact

`football_contact.c` adapts concepts from the supplied `physics.js` engagement,
hit-power and tackle functions to the native six-player snap resolver. It uses
bounded integer contests, rather than a per-frame collision simulation. Estimated
impact is derived from effective ratings; it is not measured relative velocity.
The supplied file remains reference material, not executable code or instructions.

## Blocking

Each snap selects the best available blocker using STR/TAK and OL/DL type affinity,
excluding the QB and intended runner/receiver. The defense selects a rusher using
the same strength/anchor model. A one- or two-player passing squad can have no
separate blocker; that produces a neutral block report, not a duplicate actor.

The contest compares 55% TAK / 45% STR anchor with 65% STR / 35% AGI shedding.
A single supplied random roll varies the engagement estimate (200–1,600 ms).
A sufficiently stronger blocker can earn a pancake. Anchor, estimated duration
and pancake result produce an edge capped at -12..12, feeding running lanes and
pass protection before their existing outcome clamps. Duration is an abstract
contest report, not a live timer or a promise that a sprite stays engaged.

## Contact

After a run or completed pass, exactly one carrier/tackler contest occurs using
the actual selected players' difficulty/fatigue-adjusted ratings. Estimated impact
uses defender STR and the two players' speed; weight is already represented in
STR/TAK. Tackling compares TAK against carrier STR/AGI. A receiver just catching
the ball is more vulnerable, following the reference's catch-window concept.

Failed tackles grant 1–7 extra yards, with total snap gain capped at 45. A failed
tackle cannot also cause a fumble. Successful tackles roll fumble risk against
carrier STA/STR toughness; high impact raises that risk. A post-catch fumble uses
the actual caught-ball spot in the existing turnover rules. Airborne incomplete
or intercepted passes do not run this carrier-contact contest. Sacks keep the
existing resolver and QB-carrier behavior; sack fumbles are outside this change.

The bottom screen reports broken tackle, tackle/big hit, contact fumble and
pancake outcomes during the snap presentation. The existing route view still
presents the resolved yardage; contact reports never drive render-time RNG.
Tactical juke/power/dive choices and loose-ball recovery are documented in
`TACTICS.md`. Multi-defender gang tackles, real-time collision integration and
persistent individual player statistics remain outside this resolver.

## Determinism, limits and validation

Pure contact functions receive explicit uniform 0..99 rolls, validate inputs and
leave output unchanged on failure. The match owns RNG, consuming rolls only on
accepted snaps in a defined order. Role selection scans at most six teammates.
Each snap has at most one block contest and one carrier contact, no allocation,
substep loop, texture load or filesystem work. Mega attributes flow through the
same effective-rating path before these contests.

`make -C football-core test` covers all 9,801 equal-rating pairs, every pair of
tackle/fumble rolls for strong and weak carriers, atomic invalid-input guards,
catch vulnerability, pancakes, short-roster role exclusions, post-catch fumbles,
and the existing 1,440-game frame-independent replay and seven-round flows.
CI runs contact tests under ASan/UBSan too, plus native regressions, the 3DS build
and ARM stack checks. Balance coefficients are initial arcade defaults.

Visual testing remains deferred to the next Windows builder: verify contact
labels fit and agree with outcomes, route endpoints agree with final spots, and
mega/non-mega rosters render correctly. No visual/performance result is claimed.
