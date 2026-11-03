# Football Playoffs: offensive play audit (2026-11-03)

Scope: every offensive play in the live 22-player mode (`football-core/`, files `football_live*.c`, `football_playbook.c`). Defense is not covered yet. Nothing here was changed to produce this list: it is the state of `main` at 8d4a90a.

## How this was measured

- Read the code that runs each play, then ran the real engine headless (no screen, no hardware, no emulator).
- **CPU runs/passes:** 100 seeds x all 8 defenses = 800 snaps per play, both teams identical 70-rated players, 1st and 10 at the offense's own 40, `autoplay` on. Numbers are averages per snap.
- **Human-style runs:** a bot that holds one direction after the handoff (straight, toward the low sideline, toward the high sideline) or "dodge" (steers away from the nearest defender ahead). 60 seeds x 8 defenses per cell. A human steering the runner ignores the play's route table, so this shows what the player can actually feel.
- **Routes:** dumped the route table of every receiver for every pass play, in field coordinates (depth past the line, lateral y of a 53.3-yard field), and compared the five receivers' positions at 25/50/75/100% of their routes.

Naming: roster slots are QB, RB, WR1, WR2, WR3, TE. On the field WR1 is the wide receiver on the low-y sideline (y=4), WR2 the wide receiver on the high-y sideline (y=49), WR3 the slot receiver on the low side (y=13), TE on the high side (y=39), RB behind the QB (y=28). The on-screen target numbers 1 to 5 are WR1, WR2, WR3, TE, RB. The Jet Sweep carrier is the slot receiver WR3 (target 3), the second receiver in from the low sideline, which is probably the "WR2" in your report.

## Problems that affect many plays

| # | Problem | Evidence | Severity |
|---|---|---|---|
| S1 | **Receiver alignment never changes.** `arrange()` in `football_live.c` uses one fixed table (`oy[]`) for all 20 plays. The "formation" (shotgun or under center) only moves the QB (-5 or -1.5 yd) and RB (-7 or -5 yd). Every play starts as the same 2x2 with the same sides. | `football_live.c` `arrange()`; lanes are also hard-coded in `football_live_ai_receiver` (`lane = 28/4/49/13/39`). | High (your report) |
| S2 | **No pre-snap information.** Nothing on screen shows where the receivers, TE or RB go before the snap. | Only the playcall card diagram exists (WR1's path only, `football_live_art_card`). | High (your request) |
| S3 | **Four of the six per-play balance fields are dead in live mode.** `completion`, `sack`, `interception` and `speed_weight` are read only by the old six-player model (`football_pass.c`, `football_match.c`). Live mode reads only `gain`, and only to set how long the QB waits. So "safer" (Slants), "more sacks and picks" (Four Verticals), "speed and agility" (Toss) and "agility beats pursuit" (Draw) are text, not mechanics. | grep of `->completion`, `->sack`, `->interception`, `->speed_weight` in `football-core/src`. | Medium |
| S4 | **The ball teleports at the snap on every run.** The moment the play starts the ball belongs to the runner; there is no handoff. Distance the ball jumps from the QB: RB in shotgun 2.4 yd, RB under center 3.7 yd, Jet Sweep slot receiver **14.2 yd**. | `football_live.c` `step()`: `m->carrier = off + football_playbook_runner(p)` in the SET to PLAY switch. | High |
| S5 | **For a human, run plays are interchangeable.** Only the CPU runner follows the play's route table. Measured with the same steering bot, results are identical to the hundredth within two groups: Inside Zone = HB Draw = Outside Zone (straight 1.52 yd, edge-low -0.79, dodge 2.85) and HB Dive = HB Toss = HB Power = HB Counter (straight 2.00, edge-low 0.48, dodge 2.63). The only difference inside a group is nothing; between groups it is the shotgun/under-center RB depth. | Human-style run harness (table below). | High |
| S6 | **Run blocking is the same for every run.** Each blocker picks the nearest defender within 8 yd; OL with nobody near stand still; TE/WR/RB without a target run straight upfield. There is no zone step, pull, lead block, reach/stretch or kick-out. | `football_live_physics.c` `football_live_block_target`, `football_live.c` blocker branch. | High |
| S7 | **Play action has no fake.** FB_PASS_PLAY_ACTION and FB_PASS_POST contain no run fake; the only effect of "PA" is that an under-center QB waits 0.4 s longer. Nothing makes linebackers or safeties bite. | No reference to a fake in `football_live*.c`; defenders only commit to a run when the carrier is not the QB (`football_live_defense_intent`). | Medium |
| S8 | **The four original passes send all five receivers on one route.** Quick Slants, PA Cross, Four Verticals and PA Post fall to `default:` in `football_playbook_receiver_path`, so the RB and TE run the WR1 route too (mirrored by side). Results: two receivers sent to the **same coordinate** (below), the RB running 50 yd on Four Verticals, no checkdown, no complementary routes. | Route dump below. | High |

## Run plays

All numbers: CPU runner, 800 snaps per play (CPU "side" = which way the run bends), plus the human-style bot. "neg%" = share of snaps that lost yards.

| Play | Intended (card text) | What actually happens | Bugs | CPU avg yd | CPU neg% | Human "dodge" bot avg yd |
|---|---|---|---|---|---|---|
| Inside Zone | Balanced blocking and inside run | Instant handoff, nearest-man blocking, RB path only matters to the CPU | S4, S5, S6 | 1.41 | 22 to 29 | 2.85 |
| HB Dive | Downhill push behind the line | Same as Toss/Power/Counter for a human | S4, S5, S6 | 1.18 | 20 to 22 | 2.63 |
| HB Draw | "Delayed handoff; agility beats pursuit" | **No delay at all** (instant handoff, same as Inside Zone). No pass-set look, so defenders are not drawn upfield. Agility bonus is a dead field (S3). | S3, S4, S5, S6, plus no delay | 1.15 | 23 to 33 | 2.85 |
| HB Toss | Attack the edge with speed and agility | **No pitch**, no outside blockers; the runner is handed the ball at the snap like any run. The speed bonus is a dead field (S3). | S3, S4, S5, S6 | 1.59 | 8 to 12 | 2.63 |
| HB Power | Follow the lead block into the strong-side gap | **There is no lead blocker.** No guard pulls; the "power" in the name has no mechanic. | S4, S5, S6 | 1.79 | 8 to 11 | 2.63 |
| HB Counter | Show one gap, then cut to the opposite edge | The first-gap fake exists only in the CPU route table; a human never sees a counter step and no guard or back moves the other way. | S4, S5, S6 | 1.63 | 8 to 14 | 2.63 |
| Outside Zone | Stretch the edge before turning upfield | No stretch/reach blocking; the CPU path bends outward, the human path does not. Worst run for the CPU. | S4, S5, S6 | 1.07 | 27 to 31 | 2.85 |
| **Jet Sweep** | Hand off to WR3 and race around the edge | See below. | S1, S4, S5, S6, J1 to J5 | 4.21 | **0** | **8.19** (19.6% of runs 10+ yd vs 1 to 2% for the others) |

### Jet Sweep in detail

- **J1: No handoff, no motion.** The slot receiver WR3 starts one yard behind the line on the low side (y=13) and owns the ball from the snap; the QB is 14.2 yd away. Nobody defends a handoff because there is none. Defenders only switch to "pursue the runner" 0.4 s after the snap (`football_live_defense_intent`: `m->carrier != off && elapsed >= .4`).
- **J2: He can never lose yards.** Because he starts at the line with the ball, **0%** of sweeps lose yards, against 8% to 33% for every other run. The CPU average (4.21 yd) is three times the other runs (1.07 to 1.79); the human dodge bot gets 8.19 yd with 19.6% of runs gaining 10+, against 2.6 to 2.9 yd and 1 to 2% for the other runs. This matches "WR gets the ball right away and is free to run".
- **J3: The direction is a coin flip.** `arrange()` sets `m->run_side = random ? 1 : -1` for every run, and the sweep's route (y=18 from center) is mirrored by it. A slot receiver at y=13 therefore sweeps toward the near sideline half the time and **across his own backfield to y=44.7 the other half**. Measured: run_side -1 averages 3.90 yd and ends at y=14.7 (stopped inside the formation); run_side +1 averages 4.57 yd and ends at y=8.7.
- **J4: No edge blockers.** The RB is just another nearest-man blocker; nobody leads the sweep or seals the corner (S6).
- **J5: The HUD/text say WR3 and a sweep; the engine has a slot receiver running a slow diagonal.** The route table gives only 4 points; the CPU path starts as a gentle diagonal that a linebacker in man coverage (LB rank 4 is assigned to WR3) reaches in about 1 s.

## Pass plays

CPU QB, same sample as above, 800 snaps per play: comp% / inc% / int% / sack% = completions, incompletions, interceptions, sacks per snap.

| Play | Intended (card text) | What actually happens | Bugs | avg yd | comp% | int% | sack% |
|---|---|---|---|---|---|---|---|
| Quick Slants | Quick inside pass; safer, shorter gains | All five receivers run the same slant (the back too: a slant that ends 15 yd past the line, from his spot 7 yd behind it). WR3 and TE end 2.0 yd apart on the field. (The dump below is route-relative; RB and WR1 share the shape and end point, but the back starts 7 yd deeper, so on the field they are 6 yd apart.) "Safer" is a dead field (S3). | S3, S8 | 7.25 | 57.5 | 4.0 | 1.1 |
| PA Cross | Fake the run, cross the middle | No fake (S7). All five run the same crossing route toward the middle at 8 yd; **WR1 and WR2 end 1.0 yd apart** (inside body width 1.3), the RB runs across the whole field to the sideline. | S7, S8 | 4.08 | 48.6 | 6.0 | **10.0** |
| Four Verticals | Deep routes; more sacks and picks | Five receivers including the **RB and TE go 50 yd deep**; no checkdown. "More sacks and picks" is a dead field; the sacks come from the QB waiting for 50-yd routes. | S3, S8 | 14.51 | 51.5 | 4.6 | **10.2** |
| PA Post | Long fake and deep break toward midfield | No fake (S7). All five run the post, the back included; WR3 and TE end 2.0 yd apart on the field. | S7, S8 | 11.54 | 47.6 | 4.1 | **11.6** |
| Stick | Quick stick with flat and seam outlets | WR1 sticks at 7 yd, WR2 and WR3 run seams to 35 yd, TE and RB both go to the right flat. Works as described. Balance: 10.2 yd at 1.0% sacks, among the best. | none beyond S1 | 10.17 | 61.0 | 2.8 | 1.0 |
| Mesh | Shallow crossers create traffic underneath | WR1 and WR2 cross at 4 to 5 yd, WR3 runs a 42-yd vertical, TE drags at 12 yd, RB runs the right flat. No rub/pick geometry. Safest deep-ish play: 0.2% sacks. | none beyond S1 | 10.24 | 53.9 | 5.4 | 0.2 |
| Flood | Stretch one sideline at three depths | Concept is built correctly: left sideline gets WR1 deep (27), TE at 18, WR3 and RB in the flat. **Bad payoff:** 7.53 yd with **12% sacks** (under-center plus `gain` 4 makes the QB wait 1.69 s), the highest sack rate of any pass play, with a below-average gain. | none beyond S1; balance | 7.53 | 50.6 | 3.5 | **12.0** |
| Curls | Settle in front of deep coverage | Works. (Route-relative, the back and TE end 1.0 yd apart; on the field the back starts 7 yd deeper and they are 6 yd apart, so this is not a collision. They do run the same curl shape.) | shared shape | 8.87 | 62.5 | 2.5 | 0.4 |
| Smash | A short hitch under an inside corner route | Hitch by WR1 at 6 yd, corner by WR3, WR2 vertical, TE seam. Works. Balance: 13.66 yd, 36.5% of snaps gain 10+ yd, second best pass. | balance | 13.66 | 58.8 | 2.1 | 7.1 |
| HB Screen | Invite pressure, then find the back in space | **Not a screen:** the offensive line never releases to form a wall, WR1 and WR2 run 45-yd verticals, and the throw is simply a flat pass to the RB. **WR3 also runs to the flat**, so it is the same play as WR Bubble (below). | S6, duplicate | 2.94 | 61.9 | 1.8 | 0.0 |
| WR Bubble | Throw quickly to WR3 outside the formation | **Identical routes to HB Screen** for RB, WR2, WR3 and TE (same `support[5]` table). Only WR1's route (1 yd) and the QB's first read (RB vs WR3) differ. No stalk blocking by the outside receivers. | S6, duplicate | 6.20 | 59.9 | 2.4 | 0.0 |
| Shallow Cross | Find the drag under a clearing deep route | **Nearly identical to Mesh:** same `support[1]` routes for WR2, WR3, TE and RB, WR1 route differs by 1 to 3 yd. Only the under-center set differs. | duplicate | 10.82 | 59.0 | 3.6 | 4.5 |

## Route table dump (field coordinates: depth, lateral y)

Each route is listed from the receiver's own snap spot (route-relative: the back, who stands 7 yd behind the line, only seems to share coordinates with WR1; PR 2 measures absolute field positions instead). For the four plays below, all five receivers use one table (mirrored by side).

```
Quick Slants   RB (2,28)(6,25)(14,20)(22,16)   WR1 (2,4)(6,7)(14,12)(22,16)   WR2 (2,49)(6,46)(14,41)(22,37)   WR3 (2,13)(6,16)(14,21)(22,25)   TE (2,39)(6,36)(14,31)(22,27)
PA Cross       RB (3,27)(5,22)(6,14)(8,5)     WR1 (3,5)(5,10)(6,18)(8,27)    WR2 (3,48)(5,43)(6,35)(8,26)     WR3 (3,14)(5,19)(6,27)(8,36)     TE (3,38)(5,33)(6,25)(8,16)
Four Verticals RB (6,28)...(50,27)             WR1 (6,4)...(50,5)             WR2 (6,49)...(50,48)             WR3 (6,13)...(50,14)             TE (6,39)...(50,38)
PA Post        RB (6,28)(12,28)(25,20)(40,16) WR1 (6,4)(12,4)(25,12)(40,16)  WR2 (6,49)(12,49)(25,41)(40,37)  WR3 (6,13)(12,13)(25,21)(40,25)  TE (6,39)(12,39)(25,31)(40,27)
```

That shared table is problem S8.

## Fix plan (one PR per item, bug-test mode, no builder rebuild)

1. **Run game** (S4, S5, S6, J1 to J5, part of S1): a real handoff with the ball on the QB until the runner reaches him; a sweep that has a tight alignment, a fixed direction toward the strong side, a lead blocker and edge blockers; a pulling guard for Power and Counter; per-play alignments (shotgun/under center plus mirrored left/right).
2. **Pass concepts** (S7, S8, duplicates, collisions, rest of S1): separate routes for the RB and TE on the four old concepts (RB checkdown, no 50-yd RB), a play-action fake that makes linebackers step up, different routes for HB Screen and WR Bubble (OL wall for the screen), Mesh and Shallow Cross made distinct, no two receivers on the same coordinate, per-play pass alignments, Flood/Smash tuned.
3. **Pre-snap route lines** (S2): colored lines for WR1, WR2, WR3, TE and the RB (or the runner on a run) during the set phase, gone at the snap; only drawn when you are on offense.
4. **Dead balance fields** (S3): not planned as a mechanic. The card texts that promise a bonus ("safer", "more sacks and picks", "agility beats pursuit") will be rewritten to say what the play really does.

## Status

**PR 1 (run game) fixes:** S4 (real handoff, ball stays with the QB until the runner reaches the mesh point), S5 (the plays now differ for a human: strong side, wing/RB lead, toss and outside-zone mesh farther out, draw handoff delayed and the rush keeps coming), S6 for the runs that have a lead blocker (Power, Counter, Toss, Outside Zone, Jet Sweep; plain zone blocking is still the nearest-man rule), S1 for runs (alignments Spread/Trips/Tight/Jet, mirrored half the time), J1 to J5.

Before and after, same sample (800 snaps per play, mirror teams, CPU runner; "dodge" = steered runner):

| Run | CPU yd before | CPU yd after | Steered yd before | Steered yd after |
|---|---|---|---|---|
| Inside Zone | 1.41 | 1.15 | 2.85 | 3.32 |
| HB Dive | 1.18 | 1.65 | 2.63 | 3.17 |
| HB Draw | 1.15 | 0.53 | 2.85 | 2.30 |
| HB Toss | 1.59 | 1.15 | 2.63 | 2.59 |
| HB Power | 1.79 | 1.45 | 2.63 | 3.38 |
| HB Counter | 1.63 | 1.54 | 2.63 | 3.30 |
| Outside Zone | 1.07 | 0.53 | 2.85 | 3.02 |
| **Jet Sweep** | **4.21** (0% losses) | **0.51** (36 to 42% losses) | **8.19** (19.6% of runs 10+) | **2.58** (3.8% of runs 10+) |

Known gaps left for later: the CPU's plain handoff costs about 0.3 yd a carry against the instant one; plain Inside/Outside Zone share one blocking rule; Counter has no weak-side counter step (the delay and the wing lead are its only differences); the CPU runs the lateral sweep leg poorly.

**PR 2 (pass concepts) fixes:** S8 (the four old concepts now send the back, tight end and slot on their own routes; the back is an outlet that never runs past 8 yd; no two receivers on one coordinate on the field), the duplicates (Mesh vs Shallow Cross, HB Screen vs WR Bubble), S7 (play action: the back's route runs through the mesh point, the zone linebackers and safeties step up for 0.8 s, the CPU QB waits 0.8 s longer), the screen (the line releases to a wall, the QB waits for it), part of S1 for passes (Spread, Trips or Tight, mirrored half the time), and S3 for the texts only (seven card descriptions that promised a bonus the engine never applied now say what the play does). The `completion`, `sack`, `interception` and `speed_weight` fields stay dead in live mode.

Corrections to this audit's own findings: the S8 and Curls "same coordinate" claims were route-relative (see the note under the route dump). On the field the real overlaps were PA Cross (WR1 and WR2 ended 1.0 yd apart, 0.0 in a Trips set), Slants and PA Post (WR3 and TE 2.0 yd apart), and the back running a slant to 15 yd past the line, a post to 35 and a vertical to 43.

Same sample as before (100 seeds x 8 defenses, CPU QB, 1st and 10 at the 40; alignments vary as in a game):

| Pass | yd before | yd after | sacks % before | after | 10+ yd % before | after |
|---|---|---|---|---|---|---|
| Quick Slants | 6.96 | 9.80 | 0.6 | 3.4 | 22.0 | 38.0 |
| PA Cross | 4.70 | 11.12 | 8.9 | 7.8 | 11.1 | 42.1 |
| Four Verticals | 14.81 | 15.81 | 7.9 | 6.0 | 47.9 | 53.4 |
| PA Post | 12.47 | 10.78 | 12.5 | 8.6 | 43.4 | 43.8 |
| Stick | 10.19 | 11.78 | 0.8 | 0.4 | 27.8 | 31.6 |
| Mesh | 9.03 | 10.06 | 0.2 | 0.2 | 22.4 | 24.9 |
| Flood | 7.10 | 8.46 | 8.9 | 12.2 | 33.0 | 32.2 |
| Curls | 9.34 | 11.86 | 0.0 | 0.0 | 30.6 | 39.4 |
| Smash | 13.88 | 12.90 | 5.6 | 6.0 | 38.2 | 41.1 |
| HB Screen | 2.97 | 7.18 | 0.0 | 0.0 | 9.2 | 20.2 |
| WR Bubble | 5.64 | 5.92 | 0.0 | 0.0 | 15.6 | 17.1 |
| Shallow Cross | 9.73 | 11.85 | 3.8 | 4.2 | 24.1 | 37.2 |

The mean over the twelve passes went from 8.9 to 10.5 yd a snap (the runs average about 1.2), so the gap between passing and running, already large, is a little larger. Nothing here tunes it; it is a balance call for later. Flood is unchanged and still carries the highest sack rate (12%).

Known gaps left: the CPU QB's wait for every concept except the screens still comes from the old `gain` field; Bunch and Empty sets do not exist; WR1 and WR2 still run the same route on Slants, Verticals and Post (mirrored, on opposite sides); the screen's wall is three linemen walking to three fixed points, not a blocking scheme.
**PR 3 (pre-snap route lines) fixes:** S2. In the set phase the top screen draws a dotted line for each receiver, tight end and back of the human's pass (gold for the primary read), or the runner's line on a run (his way to the handoff, then the called path); they vanish at the snap and are not drawn for the CPU's offense or for kicks. The engine answers with `football_live_route_line`, so the lines come from the same route tables, alignment spots (`snap_x`/`snap_y`), inside sign and mesh point the players use. Not shown: the lead blocker's path on a run, and the defenders' assignments (offense first; defense is a later audit).

**Still open:** S3 (the four balance fields stay dead).
