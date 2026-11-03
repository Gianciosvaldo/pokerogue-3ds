# Football Playoffs: defense audit

Requested by lorenzo on 2026-11-03, after the offense audit (`OFFENSE-AUDIT.md`: run game #542, pass game #543, pre-snap route lines #544). Same method: read what each call is supposed to do, measure what the eleven defenders actually do, list every difference. **Nothing in the engine was changed for this document.** Fixes are proposed at the end, one PR each, and wait for lorenzo's go.

## How this was measured

Scratch harnesses (not committed) driving the live engine at 60 Hz with mirror teams (identical 70-rated units on both sides, home on offense at its own 40, 1st and 10), the defense forced by hand after the play is called:

- **Look:** every defense against a pass and a run, positions at the set phase.
- **Results:** all 12 pass concepts and 8 runs x 8 defenses x 100 seeds, CPU quarterback and CPU runner (1,200 pass snaps and 800 run snaps per defense).
- **Coverage while the QB holds:** a human offense that never throws (12 passes x 8 defenses x 60 seeds), snapshots at 1.2, 2.0 and 3.0 s: how far each of WR1, WR2, WR3, TE and the back is from the nearest defender ("open" = more than 4 yd from every defender). Only snaps where the QB still holds the ball at that moment count (about 650 per defense at 1.2 s, 130 to 350 at 2.0 s, fewer against the blitzes, which sack him first).
- **Throw outcome by separation:** every CPU throw, bucketed by the target's distance to the nearest defender at release (150 seeds x 12 concepts x 8 defenses).
- **Goal line:** the same runs and passes from the opponent's 3-yard line.

The CPU picks its defense from down, distance and field position only; it never reads the offensive call (`football_live_ai_defense`), so none of the findings come from the AI cheating or guessing.

## What each call is, and what the engine builds

The structure of every call matches its card text (rushers, men, zones, deep players all counted from the set-phase positions). The differences are in behaviour and results, below.

| Call | Card text | Rushers / men / deep | What the set phase shows |
|---|---|---|---|
| 2-Man | five man matchups, two deep safeties | 4 / 5 / S9, S10 at 15 yd | LBs 5 yd off, CBs 2 yd off |
| Cover 2 Zone | two deep safeties, five underneath zones | 4 / 0 / S9, S10 at 18 yd | CBs in the flats at 5 yd, LBs at 7-8 yd |
| Man Blitz | five rushers, man, one deep safety | 5 / 5 / S10 | LB5 rushes, S9 steps up onto the back at 5 yd |
| Cover 1 Robber | one deep safety, a robber reads the middle | 4 / 5 / S9 at 18 yd | S10 sits at 8 yd in the middle |
| Cover 3 | three deep thirds, four underneath | 4 / 0 / CB7, CB8 at 18 yd, S9 at 24 yd | LB4 and S10 in the flats at 7 yd |
| Cover 4 Quarters | four deep quarters, protect the big play | 4 / 0 / CBs at 16 yd, safeties at 22 yd | LBs at 7-8 yd on y 12, 27, 41 |
| Zone Blitz | five rushers, three underneath, three deep | 5 / 0 / CBs 18 yd, S9 24 yd | LB4 rushes, S10 drops to the low flat |
| Cover 0 Blitz | six rushers, five isolated men | 6 / 5 / none | both safeties at 5 yd, on WR3 and the back |

## Results per defense (mirror teams, CPU on both sides)

| Defense | Pass yd/snap | Comp % | INT % | Sack % | 10+ yd % | 20+ yd % | Run yd/snap | Runs that lose yards % |
|---|---|---|---|---|---|---|---|---|
| 2-Man | 10.82 | 58.2 | 3.2 | 4.0 | 35.5 | 13.7 | 0.48 | 33.6 |
| Cover 2 Zone | 7.34 | 58.1 | 3.3 | 4.8 | 23.8 | 6.2 | 1.51 | 21.0 |
| Man Blitz | 14.22 | 58.8 | 3.1 | 6.2 | 37.3 | 20.8 | 0.24 | 36.9 |
| Cover 1 Robber | 14.16 | 59.4 | 2.2 | 5.0 | 38.8 | 22.4 | 0.35 | 35.8 |
| Cover 3 | 7.50 | 58.5 | 1.9 | 2.7 | 29.7 | 8.0 | 2.21 | 15.0 |
| Cover 4 Quarters | 7.68 | 56.0 | 3.7 | 1.2 | 36.0 | 5.2 | 1.80 | 20.1 |
| Zone Blitz | 9.02 | 57.2 | 3.8 | 1.2 | 42.2 | 7.5 | 1.54 | 21.9 |
| Cover 0 Blitz | 15.19 | 54.1 | 3.0 | 7.4 | 36.8 | 23.7 | -0.09 | 43.4 |

When the QB never throws, the time to the sack tracks the rush as it should: Cover 0 1.66 s, Zone Blitz 1.79 s, Man Blitz 1.99 s, the four-man calls 2.12 to 2.21 s.

## Problems found

Severity: **High** = visibly wrong in play or contradicts the call's own card; **Medium** = a real hole or a card that does not match its results; **Low** = design gap.

| # | Sev | Problem | Evidence |
|---|---|---|---|
| D1 | High | **(fixed, see Status)** **Zone depths ignore the goal line.** `football_live_defense_setup` places every zone and deep defender at `los + depth` and only clamps to the back line (x 119). From the 3-yard line Cover 3 puts the deep players 12 yd from the ball (9 yd into the end zone) and the linebackers 7-8 yd deep (4-5 yd past the goal line), so nobody stands in the front of the end zone. | Set-phase positions from ball 97 (LOS at x 107, goal line 110, back line 120): Cover 3 CBs and S9 at depth 12, LBs at 7-8; Cover 2 and 4 safeties at depth 12. A completion or run of 3+ yd from the 3 (the touchdown distance) happens on 50.6-57.3% of passes against all eight calls (a flat answer: the call hardly matters), and on 8.0-34.1% of runs (Cover 0 8.0%, 2-Man 11.6%, Cover 3 34.1%). |
| D2 | High | **Cover 4 and Zone Blitz leave the sideline strips uncovered.** A deep defender only reacts to a receiver deeper than 8 yd (`inside = depth>=8` for `deep`), and neither call has a flat defender on that side (Cover 4's linebackers sit at y 12 and 41, Zone Blitz's LB4 rushes and S10 covers one flat only). No pattern matching: the corners at 16-18 yd never come up for a short route. | At 1.2 s with the QB holding, WR1 / WR2 are more than 4 yd from every defender in **75% / 80% (Cover 4)** and **66% / 76% (Zone Blitz)** of snaps, against 0-3% in Cover 2 and Cover 3. Trace, Curls vs Cover 4: WR1 is 11.7, 9.4, 6.7, 5.6 and 6.1 yd from the nearest defender at 0, 0.4, 0.8, 1.2 and 1.6 s; vs Cover 3 the flat defender is 1.3 yd from him by 0.8 s. Cover 4 is also not safe underneath: 36.0% of its passes gain 10+ yd (Cover 2: 23.8%, Cover 3: 29.7%), though it does protect the deep ball (5.2% gain 20+, the lowest). Zone Blitz sacks 1.2% of snaps, as low as Cover 4, against a card of "+6 sacks"; most likely the QB releases the ball to the open outside man before the rush arrives (not tested). |
| D3 | Medium | **Zone lanes are fixed in absolute field lanes and do not move for the offense's set.** Every zone call uses the same lanes whether the offense is in Spread, Trips or Tight, and mirrored or not, so the slot receiver can sit between two zones. | WR3 (the slot) is more than 4 yd from every defender at 1.2 s in **26% (Cover 2)** and **40% (Cover 3)** of snaps, 0-3% for WR1 and WR2 in the same calls. Share of all five receivers open at 2.0 s by set: Cover 2 19.8% Spread, 26.5% Trips, 19.7% Tight; Cover 3 22.0%, 25.5%, 26.4%. The cause (lanes against the slot's spot) is my reading of the code, not isolated. |
| D4 | Medium | **A back who stays behind the line is never covered, in any defense.** Man coverage aims at a point 1.7 yd downfield of the receiver, which for a back behind the line is behind the offensive line, and no zone owns the backfield flats. The assigned linebacker makes slow progress (most likely the linemen's bodies are in the way; the cause is not isolated). The back is a free outlet. | The back is more than 4 yd from every defender in 77-86% of snaps at 1.2 s and 69-92% at 2.0 s against all eight calls. Trace, 2-Man vs Slants: the assigned LB5 moves from 5 yd to -0.4 yd in 1.6 s (about 3.4 yd/s against a top speed near 7), and the nearest defender to the back is still 6-7 yd away. After #543 the back is a real checkdown, so this now shows. |
| D5 | Medium | **Coverage barely changes whether a pass is caught.** The target's distance to the nearest defender at release moves the completion rate only a few points, and wide-open targets are still intercepted. The call's effect on completion is about +-2 points. | Completion by separation: under 1.5 yd 54.2%, 1.5-3 yd 56.5%, 3-5 yd 61.2%, 5-8 yd 61.9%, over 8 yd 61.8%. Interceptions: 4.7%, 4.5%, 2.5%, 2.0%, 3.2% (over 8 yd: Cover 4 4.9%, Zone Blitz 3.3%). Completion by call: 54.1-59.4 across all eight. This is the catch model, shared with the offense; listed because it is why a defense call cannot reduce completions. |
| D6 | Medium | **The cards' balance fields (`sack`, `completion`, `interception`, `run_gain`) are dead in the live engine** (only `football_match.c` and `football_pass.c`, the old six-player model, read them), and the results often contradict them. The live defense card shows only the name and the rush/shell diagram, so a player never sees the numbers, but the legacy menus still print the descriptions. | Cover 1 Robber (card: completion -5, interception +3) allows 14.16 yd/snap, the second most, and intercepts 2.2%; Zone Blitz (sack +6) sacks 1.2%; Cover 0 (completion +10 to the offense) has the lowest completion rate, 54.1%; Cover 0 (interception -3) intercepts 3.0% against Cover 3's 1.9% (+3). The run fields point the right way (Cover 4 +4: 1.80 yd, Cover 0 -2: -0.09 yd). |
| D7 | Medium | **Run defense is eleven players sprinting at the ball.** At the handoff every defender, deep safeties and man corners included, switches to pursuit of the carrier's predicted spot. No gap, contain or force responsibility, so the result is set by how many bodies are near, not by the call. Man-based calls stop runs almost completely, the zone shells give the most. | Defenders within 4 yd of the carrier 1.5 s after the handoff: Cover 0 5.4, C1 Robber 4.9, Man Blitz 4.9, 2-Man 4.2, Cover 2 3.6, Cover 4 3.5, Zone Blitz 3.8, Cover 3 3.2. Run yards 0.2-0.5 and 34-43% losses against 2-Man, Man Blitz, Cover 1 and Cover 0; all eight run concepts average 0.43 yd or less against Cover 0 (Draw -1.04, Jet Sweep -0.69, Outside Zone -0.22, Toss -0.19). |
| D8 | Low | **No disguise.** Every zone defender starts on his drop spot (Cover 3: CBs 18 yd deep, free safety 24 yd; Cover 4: CBs 16, safeties 22), so the coverage is readable from the set phase, and nobody rotates after the snap. | Set-phase positions (table above). |
| D9 | Low | **Man matchups are by rank, not by position on the field.** WR1 is always CB7's, WR3 always LB4's, the TE LB6's, wherever they line up (Trips, Tight, Jet, mirrored), so a linebacker is always on the slot and on the tight end. | `man[]` in `football_live_defense_setup`. Not wrong, but there is no check or switch. |

## Status

**D1 (goal-line squeeze) fixed.** `football_live_defense_setup` now caps every defender's start and drop spot at the goal line plus 2.5 yd for the deep players and 1 yd for the rest (`FB_LIVE_DEEP_BEHIND`, `FB_LIVE_SHALLOW_BEHIND`), and the deep players take over a receiver once he is past the goal line (`deep_min` = min(8, yards to the goal line + 1)) instead of only at 8 yd. Before this the 2-Man safeties and Man Blitz's deep safety, placed 15 yd off the ball, started *outside the field* from the 3-yard line (x 122). Nothing changes until the ball is about 21 yd from the goal line: the open-field tables above are bit-for-bit identical (100 seeds, every table). Share of snaps that gain the distance to the goal line, CPU on both sides, 100 seeds per play, before to after:

| Defense | From the 15, pass | run | From the 10, pass | run | From the 5, pass | run | From the 3, pass | run |
|---|---|---|---|---|---|---|---|---|
| 2-Man | 19.9 to 18.6 | 0.1 to 0.1 | 35.3 to 31.8 | 0.4 to 0.1 | 49.5 to 44.1 | 4.4 to 2.8 | 55.3 to 48.7 | 11.6 to 9.6 |
| Cover 2 | 11.0 to 11.3 | 0.0 to 0.0 | 23.2 to 18.3 | 0.9 to 0.6 | 45.5 to 35.8 | 12.1 to 4.9 | 51.3 to 43.2 | 27.0 to 12.5 |
| Man Blitz | 24.8 to 24.8 | 0.4 to 0.2 | 34.1 to 34.1 | 0.2 to 0.1 | 47.9 to 45.9 | 2.9 to 2.1 | 54.2 to 49.8 | 10.5 to 8.4 |
| Cover 1 Robber | 27.4 to 27.4 | 0.4 to 0.5 | 37.3 to 36.7 | 0.9 to 0.8 | 48.8 to 49.8 | 3.9 to 2.9 | 53.3 to 54.4 | 11.1 to 6.5 |
| Cover 3 | 14.2 to 13.0 | 0.1 to 0.2 | 27.7 to 22.9 | 2.0 to 1.8 | 50.7 to 46.6 | 14.4 to 9.1 | 57.3 to 52.5 | 34.1 to 23.2 |
| Cover 4 | 13.9 to 11.2 | 0.1 to 0.0 | 33.1 to 22.5 | 1.8 to 0.6 | 49.4 to 35.8 | 12.4 to 7.1 | 53.2 to 48.2 | 28.0 to 16.6 |
| Zone Blitz | 16.6 to 15.8 | 0.1 to 0.4 | 39.8 to 26.8 | 1.6 to 1.1 | 50.8 to 44.8 | 11.1 to 8.0 | 53.1 to 53.3 | 26.1 to 20.5 |
| Cover 0 | 26.4 to 26.8 | 0.1 to 0.1 | 35.3 to 36.5 | 0.1 to 0.1 | 45.3 to 46.2 | 1.4 to 1.4 | 50.6 to 51.8 | 8.0 to 8.2 |

The zone calls now defend the end zone, and the call matters near the goal (from the 5, Cover 4 allows a score-distance pass 35.8% of the time, Cover 1 49.8%; it used to be 45-51% for all). The side effect is a harder red zone for the offense, mostly against zone shells, which were already low for runs; I did not retune it. The man calls barely move (their men were already at the line). One existing test, `kickoff_test`, put its ball carrier down on top of a defender at the goal line (26 of 200 seeds were pushed back out); its touchdown helper now drops him in the emptiest gap over the line, with every assertion unchanged.

**D2 (sideline strips in Cover 4 and Zone Blitz) fixed.** The fix is in the corners, not the linebackers: a corner of these two calls now starts 12 yd off the ball (`FB_LIVE_SQUAT_START`, it was 16 and 18) and drops to his third or quarter as before; while nobody is deep in his strip he keeps a receiver who is still short (from 3 yd behind the line to the 8 yd mark, within 10 yd of his lane) like an underneath defender does, and the ordinary deep rule takes the same receiver once he passes the mark, a deeper receiver in the strip always first. `football_defense_squat_mask` (ranks 7 and 8 of those two calls) says who does it; the other six calls are bit-for-bit what they were (every number below for them is identical). Measured with the QB never throwing (60 seeds x 12 concepts) and with the CPU on both sides (100 seeds x 12 concepts, 1,200 snaps per call), before to after:

| | Cover 4 | Zone Blitz |
|---|---|---|
| WR1 / WR2 more than 4 yd from every defender at 1.2 s (QB holding) | 75 / 80% to 13 / 5% | 66 / 76% to 12 / 6% |
| same at 2.0 s | 59 / 61% to 1 / 0% | 53 / 51% to 1 / 0% |
| Mean separation of WR1 / WR2 at 2.0 s, CPU passes | 3.8 / 5.5 to 1.6 / 1.3 yd | 4.3 / 6.9 to 1.6 / 1.3 yd |
| Pass yards per snap | 7.68 to 6.29 | 9.02 to 8.03 |
| Completion / interception / sack | 56.0 / 3.7 / 1.2% to 54.8 / 3.2 / 4.3% | 57.2 / 3.8 / 1.2% to 54.3 / 3.6 / 5.5% |
| Gain 10+ / gain 20+ | 36.0 / 5.2% to 22.2 / 4.6% | 42.2 / 7.5% to 23.6 / 9.1% |
| Run yards per snap | 1.80 to 1.74 | 1.54 to 1.45 |

Things to know before trusting it. The two calls are now sharper than the audit asked for: Cover 4 gives up the fewest pass yards of the eight calls (6.29, Cover 2 is 7.34) and the lowest 20+ share, and the rate at which the QB is sacked went from 1.2% to 4.3% / 5.5% (the Zone Blitz card says +6 sacks), because the QB's outside reads are now covered and he holds the ball. Zone Blitz gives up more deep balls than before (7.5% to 9.1%, about two standard errors at 1,200 snaps): a corner at 12 yd with one safety deep can be beaten vertically, which the old 18 yd cushion made impossible. The start depth is not the knob for the yards: the rule alone (corners left at 16-18) already gives 6.34 yd in Cover 4, and starting at 16 leaves WR1 / WR2 open 72 / 74% at 1.2 s; 12 is the deepest start that brings both under 20%. The slot (WR3, 0% in these two calls) and the tight end (16-19%) are unchanged; the back is D4.

**D3 (the slot in Trips and Tight) fixed.** The cause was not the lanes drifting but a count: Trips and Tight put three of WR1, WR2, WR3 and the TE on one side, and Cover 2 and Cover 3 have two underneath defenders there, so each defender takes the receiver that scores best for him and the third is alone. WR3 at 1.2 s with the QB holding, more than 4 yd from every defender: **Trips 86% (Cover 2) and 84% (Cover 3), Tight 12% and 59%, Spread 15% and 21%**. `overload_lanes` (`football_live_defense.c`) now, when three of the four stand on one side, puts the underneath defenders nearest that sideline (the flats and hooks, not the deep players) on the three receivers' spots, one each, sideline first, clamped to y 4..49.3; two and two (Spread, Jet) keeps the fixed lanes. `football_defense_matches_overload` limits it to Cover 2 and Cover 3: Cover 4 and Zone Blitz already had the slot at 0% in both sets, and a plain 6 yd shift of their lanes (my first try) made it 34% and 60% in Tight. Before to after (CPU on both sides, 100 seeds x 12 concepts, 1,200 pass snaps and 800 run snaps per call; QB-holding figures 60 seeds x 12 concepts):

| | Cover 2 | Cover 3 |
|---|---|---|
| WR3 open at 1.2 s, QB holding (all sets) | 26% to 11% | 40% to 14% |
| WR3 open at 1.2 s, Trips / Tight / Spread | 86 / 12 / 15% to 0 / 10 / 15% | 84 / 59 / 21% to 0 / 17 / 21% |
| Pass yards per snap | 7.34 to 5.74 | 7.50 to 6.38 |
| Completion / interception / sack | 58.1 / 3.3 / 4.8% to 56.5 / 2.5 / 5.3% | 58.5 / 1.9 / 2.7% to 56.9 / 2.3 / 4.1% |
| Gain 10+ / gain 20+ | 23.8 / 6.2% to 17.7 / 4.0% | 29.7 / 8.0% to 28.2 / 5.2% |
| Run yards per snap | 1.51 to 1.51 | 2.21 to 1.91 |

What is left: the Spread numbers do not move (they come from the play-action and bubble concepts, PA Cross, PA Post, WR Bubble, Flood, and I did not touch them); Cover 3 in Tight is still 17%. The back is D4 (76% / 65% open in these two calls). The other six calls are identical to before in every table. The size of the drop is the thing to look at: Cover 2 now gives up 5.7 yd per pass snap and Cover 3 6.4, with Cover 4 at 6.3 and Zone Blitz at 8.0, against 10.9 to 15.2 for the four man calls. That order may be right (zone gives up the short game, man the big plays), but the gap is wider than it was, and with D5 (completion barely depends on coverage) unchanged it is a balance call for lorenzo, not something I tuned.

**Correction to the method (found while measuring D3).** The scratch harnesses forced a call by setting `m.defense` and calling `football_live_defense_setup` a second time, without re-arranging the formation, so a defender the new call does not place kept the spot the CPU's own first pick had given him. Zone calls place every rank, so Cover 2, Cover 3, Cover 4, Zone Blitz and Cover 1 Robber are not affected; the deep safeties of 2-Man and the free safety of Man Blitz started somewhere else than their 15 yd. Re-measured with the default formation restored: 2-Man 10.82 to 10.88 pass yd/snap and 0.48 to 0.54 run, Man Blitz 14.22 to 13.06 and 0.24 to 0.42; every other row and column is identical. None of the findings changes (Man Blitz and Cover 0 are still the leakiest calls). The D1 red-zone table's 2-Man and Man Blitz rows were measured the same way and I did not redo them. The game itself always re-arranges the formation after the call is set (`football_live_call`, also from `football_live_defend`), so players never see this.

**D4 (the back) evaluated and left as is.** The back is still the one receiver nobody owns: he stands about 2 yd behind the line and is 5.3 to 7.7 yd from the nearest defender when the CPU throws to him, in every call. Two things argue against covering him now. First, the CPU quarterback uses him as the safety valve once the other receivers are covered: share of CPU throws that go to the back, before D2 and D3 to after, Cover 2 33.8% to 43.5%, Cover 3 21.3% to 34.1%, Cover 4 8.6% to 24.0%, Zone Blitz 6.2% to 18.2% (2-Man 28.2%, Man Blitz 26.7%, Cover 1 26.9%, Cover 0 17.1%, unchanged). Second, the back gains less per throw than the other four (5.4 yd against 6.8 in Cover 2, 6.5 against 6.8 in Cover 3, after D3), so he is a fair price for the shells, not a hole. A prototype (an idle zone defender, one with nobody in his window, picks up the back while he is within 3 yd of the line) moved the distance from 6.8 to 5.5 yd and the back's yards per throw from 5.39 to 4.91 in Cover 2, and did nothing in Cover 3 (7.3 yd, no idle defender), at a time when D2 and D3 had already cut zone pass yards by 10 to 22%. I did not ship it. The man calls are different: the linebacker assigned to the back aims 1.7 yd upfield of him, at the line, and is held up by the linemen (sep 5.3 to 5.7); that is physical, not a coverage rule. Revisit if pass yards against zone turn out too low in play.

**D7 (run fits) evaluated and left as is.** I first read the problem as "too few yards", with 3 to 4 yd per carry as a target. That target has no basis in the repo: `OFFENSE-AUDIT.md` measured the other runs at 1.07 to 1.79 yd before the offense work, and the current figures (0.0 to 2.3 yd depending on the call) are the same range. What D7 describes is the spread between calls: man calls and Cover 0 stop runs (0.0 to 0.8 yd), the zone shells give 1.7 to 2.3. The cause I suspected was the secondary running at the ball from the handoff. A prototype that keeps the corners and safeties on their coverage until the carrier is 2, 3, 4 or 6 yd past the line moved the run averages by 0.00 to 0.17 yd (Cover 3 2.04 yd with no delay, 2.16, 2.23, 2.26, 2.39) and the 10+ share from 3.5% to 5.6 to 6.1% in Cover 3; the man calls moved 0.1 yd. So the run is decided by the front seven, not the secondary. Changing that (blocking contact, line pursuit) would move every run number and is not a fit-assignment change, so I did not ship anything; if runs should be stronger or weaker overall, that is a balance decision, and it is lorenzo's to ask for.

## Verified as correct

- Rusher, man, zone and deep counts match the card text for all eight calls (set-phase counts; mask functions in `football_defense.h`).
- More rushers get home sooner (see the sack times above), and play action pulls the second level up (#543).
- Man calls hold their receivers: against 2-Man, Man Blitz, Cover 1 and Cover 0 WR1, WR2 and the TE are never more than 4 yd from a defender at 1.2 s, and WR3 only in Cover 0 (13%).
- The CPU's defensive mix (blitz-heavy on 3rd and long, man-heavy on short yardage, fewer deep shells in the red zone) is plausible and is not influenced by the play.

## Proposed fixes (one PR each, none started)

1. **D1 goal-line compression** (done, see Status): cap each defender's depth at the distance to the goal line plus a few yards, in `football_live_defense_setup`; test positions from the 1, 3, 5 and 10 yard lines for every call.
2. **D2 flats and pattern matching** (done, see Status; the corners were changed, the linebackers were not): a deep corner with no deep threat picks up a shallow outside receiver in his third or quarter; Cover 4's underneath linebackers move out, and Zone Blitz gets its second flat defender back. Target: WR1 / WR2 open under 20% at 1.2 s in both, deep share kept.
3. **D3 seams** (done, see Status; the cause was three receivers against two underneath defenders in Trips and Tight): shift the underneath zones toward the offense's strong side and the slot (Trips, Tight), mirrored with the alignment.
4. **D4 the back** (evaluated, left as is, see Status): let a man defender trail a receiver behind the line from the side, and give the flats a zone owner; check the pass yards do not collapse.
5. **D7 run fits** (evaluated, left as is, see Status): contain and gap responsibilities so the safeties wait for the run to declare; will move every run number, so it needs lorenzo's call on the target yards.
6. **D6 / D8 cards and disguise** (small): either hide or correct the dead numbers and descriptions, and let the safeties start from a two-high look and rotate after the snap.

D5 (catch model) is shared with the offense and is a balance decision for lorenzo, not a bug I would change unprompted.

## Not looked at

Special teams alignments, the human-controlled defender's controls, tackling and blocking contact values, and the legacy six-player model.
