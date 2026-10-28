# Receiver, protection and live Mega regressions

Receivers previously chased a waypoint indexed by elapsed time. A receiver could
outrun that point, reverse, and chase it again. Routes now look ahead from actual
field depth and continue downfield beyond the final route point. Ball targets
still track the landing point. Receivers also steer laterally around a covering
body, continuing the release until clear rather than turning back into contact.
Speed, acceleration, collision size and strength are unchanged.

Pass protection previously selected a nearest rusher independently for each
lineman. Several linemen could double an interior defender while leaving an edge
unassigned. A shared deterministic assignment now prioritizes rushers with fewer
reachable blockers and avoids duplicate assignments. Physical engagement honors
that assignment instead of grabbing an incidental nearby defender. Existing
strength contests, shedding, cooldowns, run blocking and special teams remain.
Protection can still lose a matchup; it does not guarantee a clean pocket.

`movement-regression-test` covers all four passing plays, five eligible slots,
both directions, slow/fast attributes and actual generated Electrode attributes;
slower man-coverage jams in both directions; unique protection of four rushers;
and contact that must not abandon an edge assignment. The current 273 checks
pass. Compiling the same tests against the pre-fix movement/physics reproduces
the failures; logs are retained in the task checkpoint workspace.

The unchanged AI test runs 40 seeded games twice for determinism, another ten
seeds, and situation/mix checks. Before: 2276 snaps, 20.9 points/game, 41% runs,
57% completions, 4.6% sacks, 2.6% interceptions. After: 2247 snaps, 19.2 points/game,
42% runs, 57% completions, 5.1% sacks, 2.4% interceptions. All original balance
bounds remain. These seeded samples establish regression coverage, not complete
AI balance or on-device performance.

See `MEGA.md` for first-stone availability, fixed-holder UI and initial-formation
activation. New movement and live-career Mega tests are included in the normal
football suite and the CI sanitizer step. Host UI recordings check holder/name,
preview, purchase and the unavailable-holder explanation. Hardware visuals and
performance require a later device run; no new builder is part of this fix.

The candy regression retains all eight complete career games and every credit,
profile, save/resume and duplicate-payment assertion. Its former requirement
that six of those weak-roster games happen to earn a play reward was dependent
on AI outcomes (now five do). That sampling condition is replaced with 24
deterministic checkpoint cases: each of the three credit kinds in each of the
eight career fixtures, delivered through the live credit and career owner. Each
checks unflushed profile isolation, duplicate delivery, save isolation, exact
payment and duplicate flush. Natural-game reward counts remain diagnostic.
