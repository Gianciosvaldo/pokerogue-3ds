# Football match engine

The next layer after the data, season, playcalling and renderer systems is a
stat-driven match resolver. `football_rules.c` owns football rules;
`football_match.c` resolves selected plays and opponent calls;
`football_playoff_match.c` feeds the real result into the season; and
`game/source/football_match_view.c` converts state into the existing renderer's
scene. These modules compile in the normal 3DS build. The main-menu entry and
application screen that drives them are documented in `APP.md`.

## Rules and initial scope

This is an abbreviated six-player arcade rule set, using the port's existing
1–6-member draft capacity, rather than a full NFL simulation. A drive begins at
the offense's own 25-yard line, with four downs to gain ten yards. The goal-to-go
marker stops at 100. Failed fourth downs, interceptions and lost fumbles change
possession and mirror the field spot correctly. A lost ball in the opponent's
end zone is a touchback; one recovered in the offense's own end zone is a defensive
touchdown. Normal touchdowns score seven (six plus an automatic extra point),
and safeties score two. Scoring starts the next drive at the receiving team's 25.
There are no punts, field goals, kick returns, penalties, manual extra points or
collision/route physics in this first engine.

Regulation has four simulated two-minute quarters. Play resolution consumes game
time; deliberating in playcalling does not. The final play of a quarter completes
before the clock/winner is checked, so a touchdown with one second left counts.
Quarter transitions preserve the drive except halftime, which gives the away
team the ball at its 25. Unequal scores after the fourth quarter end the game.

Ties use paired possessions from the opponent's 25, with both sides receiving an
opportunity, alternating which side starts each pair. A non-tied pair ends the
match. After four tied pairs, a seeded, skill-weighted two-point decider guarantees
a winner. This bounded overtime is an arcade rule, not an NFL overtime claim.
The renderer labels overtime OT1–OT4. All rules can be tested independently of RNG.

## Resolving a snap

`football_match_call` accepts a validated home-team `PlayInfo` only when waiting
for a home call. Formation/type/play ID must agree with the immutable `football_playbook.c`
catalog, including its legacy IDs. See `PLAYBOOK.md` for the eight named calls. Default target hints are used
when they refer to an appropriate position; otherwise the resolver selects a
runner or receiver from the actual roster. A multi-player team cannot throw to
its quarterback. Solo rosters use a single-slot squad abstraction. QB, runner,
receiver and tackler selection also use the existing primary/secondary type
positional-affinity matrix, with no extra attribute inflation.

Run outcomes use speed/agility/strength, blocking strength and opposing tackling;
pass outcomes use throwing power/accuracy, catching, receiver speed, protection
and opposing tackling. Under Center adds run push and longer play-action gains
with extra sack exposure; Shotgun improves passing completion probability.
Blocking matchups and carrier contacts are resolved by `football_contact.c`
(see `CONTACT.md`). Broken tackles can add bounded yards, and fumbles can occur
on runs or after a completed catch. Sacks, interceptions and incompletions remain
explicit outcomes. Balance
coefficients and probabilities are initial defaults, not a calibrated competitive
model. Stamina controls fatigue; energy remains within 0–100, affects subsequent
ratings, and partially recovers at possession breaks/halftime.

Away AI selects legal run/pass calls based on distance, down and seeded variation,
then chooses a formation and one of its matching named plays.
The round difficulty scales its effective ratings by Q8, capped at 99, before
fatigue. Opponent generation chooses six distinct supported, obtainable base-form
species. Their eligible base-stat total ceiling increases from 350 in round one
to 650 in round seven. It reuses stat generation and the season's Pokédex maxima.
Opponent sampling runs once at match load with fixed-size storage.

## Lifecycle and integration

Zero-initialize a `FootballPlayoffMatch` session and keep the existing dedicated
`FootballSeason`/`gc_run` ownership. From READY call
`football_playoff_match_begin(&session, &season)`. The bridge obtains the real
round seed/difficulty, copies the drafted/evolved/shop-boosted team and generates
the opponent. It rejects duplicate starts and crossing sessions between seasons.

While the season is MATCH, pass home playcalls to
`football_match_call(&session.match, play)`. Call
`football_playoff_match_tick(&session, &season, delta_ms, &why)` each frame.
It automatically begins an away AI snap when needed, then advances at most one
animation or commits one result per tick. A snap has a 700-ms presentation phase;
per-tick elapsed time is capped at 250 ms to avoid catch-up bursts. Menu, render
and zero-time polls consume no match RNG. Game outcomes are independent of frame
rate for the same sequence of accepted home calls.

On completion the bridge reads the actual winner and calls the season result API
exactly once: wins enter the existing EXP/evolution phase and losses eliminate
the run. Further ticks cannot grant currency/EXP again. Existing Nincada evolution
choices remain Ninjask / Shedinja / Cancel. The season's shop still gates READY
for the next round. Use this bridge instead of inventing a win/loss flag in the UI.

Load each team's front sprites into the existing renderer cache between matches,
using home slots 0–5 and away slots 6–11. Build each render snapshot with
`football_match_scene(&scene, &session.match, &renderer, animation_ms)`. It borrows
frames without loading textures, maps yards and direction to the field, positions
the actual selected QB for Shotgun, and supplies the real score, down, distance,
quarter and clock. Render the existing playcalling bottom menu only while
`football_match_waiting_for_home` is true. During AI/animation, show a waiting or
play-result screen; while EXP/SHOP is active, use those existing modal systems.

The engine allocates no heap memory and uses no filesystem, graphics or profile
APIs. Its team/state arrays are fixed-size and work per tick is bounded by the
six-player roster. No unbounded simulation or recursive play execution occurs in
a frame. The bridge owns no extra gc_run or profile snapshot.

## Validation

`make -C football-core test` runs the existing suites plus:

- Exact rules tests for field spots, first downs, fourth-down turnover, scoring,
  safety, lost-ball end zones, halftime, final-play scoring and bounded overtime.
- 1,440 complete games across every permitted roster size and seven difficulty
  steps, including replay at different frame rates and invalid-input guards.
- 100 real match-to-season outcomes checking both winners/losers, ownership and
  exactly-once rewards/elimination.
- Render conversion checks for actual QB alignment, home/away coordinates,
  borrowed front-sprite slots, overtime labels and no render-driven RNG draws.

Rules and full-match replay also pass AddressSanitizer/UndefinedBehaviorSanitizer.
The managed local debugger prevents LeakSanitizer, so leak detection was disabled
only for the local run; these modules allocate no memory. CI runs both sanitizers
with default leak settings and also runs the real 3DS build and stack checks.
Dedicated football checkpoint serialization is documented in `SAVE.md`. Match-only mega activation is documented in `MEGA.md`. The
app screen and its controls are documented in `APP.md`. Device visuals and performance still need
hardware or emulator validation.
