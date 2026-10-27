# Live football (default entry)

The title's Football option now opens `football_live_screen.c`, a dedicated
22-player, 55-point draft and live playoff. The older six-player mode is retained
as `football_legacy_screen_open` and is explicitly accessible through START;
its `football.sav` file is preserved. New checkpoints use `football_live.sav`.
Classic parties, cost limits, saves and battle APIs remain six-wide.

## Roster and lineup

`football_roster.c` owns 22 `gc_mon` records, normalized football attributes,
boosts and a permutation assigning each member to one of 22 slots. There are
11 offense slots (QB, RB, three WR, TE, five OL) and 11 defense slots (four DL,
three LB, two CB, FS, SS). A swap always preserves one distinct member per slot.
The draft fills the slots in order, so the lineup is the picks; auto-assignment and swapping remain for changing it later.

The draft uses classic species costs and profile cost reductions with a 55-point
aggregate limit. Only base-form starters available in the current Classic/Endless
profile are listed and accepted. The same availability API enforces species,
form, nature, ability and shiny unlocks. Football chooses a valid unlocked starter
configuration, including shiny-only or non-default ability/nature catches.
A profile is required; a missing profile never opens the entire Pokédex.
Duplicate species and evolved starting species are rejected. The catalog starts
with cheaper unlocked species. The draft is one pick for each position, in slot order (Quarterback, Running back, Wide receiver 1
... Strong safety, `football_slot_titles`): the header names the slot being filled, the pick goes into it (`lineup` starts as the
identity and the draft never assigns), every row is tagged with the position its species is best at (`FootballPlayer.position`,
`football_position_label`) and the top screen shows the highlighted species' eight attributes. Left/Right pages, A picks for the
slot, B takes the last pick back. After the 22nd pick the round hub opens (no auto-assignment); its B opens the lineup screen.
All 22 slots must be filled. A pool smaller than 22 or too expensive even with
its cheapest 22 members shows a progression/cost-reduction notice. Existing saves
are also checked against the current profile's original starter unlocks and remain
preserved if they cannot be used with that profile.
On the lineup screen A toggles editing and Left/Right swaps the selected slot's
member. Touch rows select slots and the bottom buttons cycle members while
editing. B finishes editing, then returns to the round hub. Auto-assignment and
Ready also have touch buttons. Lineups can be revisited between rounds.

One heap scratch `gc_run` creates members and runs the existing EXP/evolution
queue for one member at a time. Each winning match awards all 22 members exactly ten levels
(`GC_FOOTBALL_LEVELS_PER_WIN`, `gc_football_award_levels`): the EXP that member's growth rate needs, so after the seven games every member
is level 75 whatever it is, and the highest level evolution there is (Zweilous to Hydreigon at 64) happens in game 6. Levels do not
touch a player's attributes, which come from base stats, so this only decides when evolutions happen. The six-player season keeps its
own EXP schedule (`gc_football_award_exp`, 2000 * round squared). Evolution targets are explicitly selectable,
including Nincada's mutually exclusive Ninjask/Shedinja choice. Level-up moves are learned
without a prompt; with four known the weakest (lowest power, earliest slot) is replaced, but never a move the member's
evolution asks for (Tangela keeps Ancient Power for Tangrowth, an Eevee its only Fairy move for Sylveon) unless every move is one.
Every level evolution happens by its level. A member has no way to meet weather, a party type, a treasure count or a gender it was not
drafted with, nor a move its species only relearns (Piloswine's Ancient Power) or learns at a level a ten-level jump passes while it is
still the earlier form (Primeape's Rage Fist at 35, as a Mankey), so when a species has no evolution it can take by the rules, it takes
the ones that ask for nothing else (`gc_mon_football_evolutions`: the second pass waives those conditions, and a move only once its
learn level is behind the member). That is how Annihilape, Mamoswine, Vespiquen, Salazzle, Pangoro, Goodra and Gholdengo happen. It is a
fallback, never a replacement: a species with an evolution it can take (a Burmy of either gender, Tangela before it has learned Ancient
Power) keeps the rules and the round it always had. The rule belongs to football runs only (`gc_football_new` marks the run; classic and
custom runs queue evolutions by the upstream rules, unchanged). The evolution prompt is as before.
`tests/pace_test.c` takes every draftable species through the seven games and finds none with a level evolution left; `tests/evolve_all_test.c` pins the seven species, the unchanged rounds of the others, the moves and the classic runs. The shop retains Carbos, Protein and rare compatible
mega stones, using existing classic item icons, and adds the evolution items (next section). A mega holder is explicitly
selected between rounds; the form's stats and artwork apply for that match.

## Evolution items in the shop

Levels do not evolve everything (a Growlithe needs a Fire Stone, an Eevee one of five stones), so after a win the shop lists the
classic game's evolution items (`EVOLUTION_ITEM`, and the `RARE_EVOLUTION_ITEM` tier above id 50: Black Augurite, Metal Alloy, the
scrolls) that would evolve a member of the roster **now**. Which items fit comes from game-core (`gc_football_item_evolutions`: the
species' evolution table, the level, the form and the conditions, the same test the classic reward screen uses); an offer is
bound to its member, at most eight are shown (`FB_SHOP_EVO_MAX`), the first item of every member before anyone's second, from a
member that moves with the round so nobody is left out for good, and a member who holds a mega stone or is about to be offered one is
skipped (evolving would lose the stone). Price: `400 + 50 * round` for a common item, `800 + 75 * round` for a rare one.

Buying (`football_career_buy`, offer `FB_SHOP_FIXED + n`) runs the real evolution on the scratch run (`gc_football_item_evolve`: the
evolution phase, then any move the new form brings, a full set replacing the weakest move as after a level-up), copies the member back
and refreshes its player; the member's other items are voided, as it has evolved. The shop shows each offer as "Growlithe: Fire Stone"
with the item's icon, and the top screen the member beside what it would become with each attribute as "today>evolved"
(`football_career_evo_preview` runs the evolution without keeping it).

Not covered: fusion (football has none).

### Friendship

Every member gains `GC_FOOTBALL_FRIENDSHIP_PER_WIN` (15) friendship for each won round, in `gc_football_award_exp` before the round's
EXP is queued, so the level-up it brings can already evolve on it. A lost match gives nothing; friendship stops at 255. It is
the member's own friendship only: the classic game also feeds the starter's candy from it, which here is the candy rules' job
(see Species candy), so no profile is passed. The six-player team gets it from the same call.

Friendship evolutions are decided the way the classic game does, at a level-up with the friendship the species asks for (and
its time of day or move type where it asks for one): the EVOLVE screen offers them with the level-based ones. Starting
from each species' base friendship, a run of wins evolves Pichu, Cleffa, Igglybuff, Togepi, Azurill and Budew after round 2,
Chingling, Woobat, Cosmog and Snom after round 3, Buneary after round 4, Munchlax and Eevee after round 5, Riolu after round 6
and Type: Null after round 7 (the ones with a time of day or a move type wait for a round where it fits).
`tests/friendship_test.c` sweeps every draftable species with a friendship evolution through seven won rounds and checks
none evolves in round 1, each plain one evolves on the round its friendship allows and all have by round 7; the gain is a
design choice, and that test is what fails if it is made too fast or too slow. Mid-stages with a friendship evolution (Golbat,
Chansey, Swadloon) are not draftable, so they matter only once levels have brought a member there; the sweep does not cover them.

## Live engine and controls

`football_live.c` is a fixed-size 60 Hz simulation in yard coordinates, with
22 simultaneous actors and no per-step allocation. Rendering does not consume
its RNG. Each frame catches up at most 100 ms; action edges are retained until
a simulation step consumes them, then used once. Menus and overlays pause play.

| Situation | Controls |
| --- | --- |
| Move controlled actor | Circle Pad or held D-pad |
| Human offense before snap | A or touch SNAP; no automatic human snap |
| Pass targets | X, Y, L, R, SELECT for receivers 1–5, or numbered touch buttons |
| After completion / run handoff | Control transfers to the receiver / RB |
| Carrier evasion | B or touch Juke, with a 1.6-second cooldown |
| Human defense | B switches through all eleven defenders; A attempts a dive tackle |
| Pause / checkpoint / legacy access | START |

Plays initialize formations and AI routes, rather than precomputing results.
Actors accelerate, blockers hold nearby rushers for strength-dependent intervals,
defenders keep coverage responsibilities, and a throw travels to a predicted receiver position.
Accuracy and arm range affect error; actors near the landing point determine
catches/interceptions. Passing beyond the line of scrimmage is disabled. Contact,
sidelines, scoring and a 25-second play bound end downs under the shared rules.
CPU offense chooses independently of the selected Man/Zone/Blitz scheme (see "CPU play" below).
The clock display advances during live play; the shared rules commit its elapsed
time and runoff once at the dead ball. The seven-round playoff, shop and EXP loop
remains. This is arcade football: punts, field goals, kickoffs, the try after a touchdown and both kinds of return exist (see "Special teams" and "Touchdowns, tries and kickoffs"), but no opening kickoff, onside kicks, penalties
or interception return phase are added here. A fumble uses a nearest-actor recovery
contest at its contact spot, without a bouncing-ball phase.

## CPU play

`football_live_ai.c` holds everything the CPU decides; it reads only the live state and the match's own RNG,
so a seed still replays identically.

- **Play calling** (`football_live_ai_call`): a weight per play from down and distance (short yardage runs, long
  yardage throws), field position (no deep routes near the goal line, no pitches from the shadow of the own goal),
  and score/clock in the fourth quarter (trailing teams throw, leading teams run the clock). A play it ran on one of
  the last three snaps is much less likely, so drives are not one play repeated.
- **Defense** (`football_live_ai_defense`): blitz, zone or man by situation. Obvious passing downs bring more
  pressure and more zone; short yardage stays tight.
- **Ball carrier** (`football_live_ai_carrier`): follows the called run's route (the side it bends toward is picked
  per snap in `run_side`), leans away from defenders in the lane, slows when one stands square in front of it and
  stays off the sideline. Receivers and runners no longer share one straight line through the middle.
- **QB** (`football_live_ai_qb`): waits for the routes to break (quick routes early, deep and play-action routes
  late), throws to the most open receiver (the QB misjudges openness more the lower his `tha`), takes a smaller
  window as the clock in the pocket runs, reacts to pressure a beat late (a rusher can arrive first: a sack),
  throws the ball away or, with the agility, scrambles when nothing opens.
- **Receivers and blockers**: the targeted receiver runs to the landing spot; the others keep their routes and
  nudge away from nearby defenders. Run plays also use the tight end and receivers as blockers. Each block is a
  contest: strength tilts it, then a random edge sets how long it lasts and how fast a beaten blocker is shed.
- **The catch**: the ball comes down among the closest receiver and the closest defender. Alone under it a defender
  usually intercepts; with a receiver there the defender intercepts rarely or breaks the pass up; otherwise the
  receiver's hands decide.

`tests/ai_test.c` checks the call mix per situation, the anti-repeat rule, the QB timing, and plays 40 mirror
games between two CPU sides (`FootballLive.autoplay`, host tests only; the human is always HOME in the app) to hold
run/pass mix, completion, sack and interception rates, scoring and lateral variety of runs inside bands, and that
the same seeds give identical games.

## Special teams

`football_live_special.c` plays punts and field goals. The playcall has a third page, SPECIAL TEAMS (Left/Right
or tap the page hint); the CPU decides by itself on fourth down (`football_live_ai_special`: kicks inside its
kicker's range, goes for it on short yardage past midfield, never punts in overtime or when only a touchdown
helps late). A punt can be called on any down; a field goal only from inside 62 yards of kick length (line of
scrimmage to goal line, plus 17 for the end zone and the holder).

The kicker is the QB slot of the offense unit, so `thp` is leg strength and `tha` accuracy. The kick happens a fixed
time after the snap (1.45 s punt, 1.05 s field goal); until then the five-man line holds the closest rushers, and a
rusher who reaches the kicker blocks the kick (a human rusher can dive with A to try). Punt length is
`34 + thp/4` yards from the line of scrimmage with a few yards of variance; near the goal line the punter pooches
it short or aims for the corner rather than kick into the end zone. A human punter aims left/right with the
Circle Pad. Field goal chance is `98% - 1.55% per yard past 20` plus the kicker's `thp`/`tha` over 50.

Stages (`FootballLive.stage`): SNAP (line holds, rushers rush), FLIGHT (the ball flies, the landing spot is
`to_x/to_y` and is drawn as a marker), ROLL (nobody under it: it bounces on and is downed) and RETURN. The returner
(FS slot) runs to the landing spot; a catch inside 2.4 yards is a fair catch (the human presses A; the CPU does it when a
gunner is near or the catch would start in its own shadow), a muff (worse hands, a gunner on top of him) that either
team can recover, or a return. A return **flips `rules.offense` to the returning team**, so its carrier, blockers
and the kicking team's pursuit are the ordinary run-play code; `finish()` hands the end of the play to
`football_live_special_finish`. A kick into the end zone is a touchback to the 20, out of bounds is spotted where it
crossed the line, and a return can score a touchdown.

All rules changes go through `football_rules_possession` (punt/miss/block/muff/touchback: first and ten for the
receiver), `football_rules_field_goal` (+3, other team at its 25) and `football_rules_touchdown` (a return), which
share the clock, quarter, half and overtime logic with `football_rules_advance`. The dead-ball banner (`FootballLive.event`:
PUNT n YDS, FAIR CATCH, TOUCHBACK, BLOCKED, FIELD GOAL, NO GOOD, plus TOUCHDOWN, INTERCEPTION, SACK, FUMBLE, SAFETY after
ordinary plays) is drawn by `football_live_art_top`. `tests/special_test.c` covers the formation, snap timing, blocks,
fair catch with a steering human, returns, the field goal curve, the CPU's fourth-down choices and 30 whole CPU games.

## Touchdowns, tries and kickoffs

A touchdown is six points (`rules.td_points`, set by `football_live_begin`; the older six-player mode keeps its flat seven).
When the ball is dead after the score, `football_live_after_play` records what comes next in `FootballLive.after`:
`FB_AFTER_CONVERSION` after a touchdown (including a return touchdown and a defensive one), `FB_AFTER_KICKOFF` after a
field goal, a safety or the half. Nothing follows in overtime, or on the play that ends the game: those touchdowns are
worth the full seven, which is how `football_rules.c` scores a touchdown whose clock runs out the game or an overtime
possession. The next `CALL` (`begin_next` in `football_live.c`) then sets the state up.

**The try (`conversion` 1).** The scorer is offense on the 4-yard line and `football_live_call` is refused until it
chooses: `football_live_special(FB_SPECIAL_EXTRA_POINT)` (a kick from the 15 through the field-goal formation, 32 yards
of flight, chance `98% + the kicker's thp/tha over 50`, no distance penalty: about 93% with average legs; a rusher who
reaches the kicker blocks it) or `football_live_go_for_two` (`conversion` 2, then an ordinary playcall; two points if the
carrier or the catch crosses the goal line, nothing otherwise). A straight run from the 4 scored 62% in testing (93%
from the 2, the NFL spot, which was too easy here); the CPU's own plays score about 18%. `football_live_ai_goes_for_two`
decides for the CPU: only from the fourth quarter down by 2, 5, 8, 10 or 13 after the touchdown, and 4% of the time
(12% when 9+ behind) earlier. A try never runs the clock. `football_rules_conversion` books the points and gives the other
team the ball at its 25 until the kickoff decides the real spot.

**The kickoff (`FB_SPECIAL_KICKOFF`).** The team that scored (or conceded the safety, or the home team at the half)
kicks from its 35; rules.offense is the kicker until the catch like any kick. Ten coverage men line up abreast with the
kicker four yards behind the ball, the receiving team in a front line ten yards away, a second line, two corners and two
returners on its 5 and 10. The kick comes 1.1 s after the human presses A (the CPU 1.2 s after the set); the run-up is
scripted, and the line starts to run 0.6 s in. Length is `49 + 0.18 thp +/- 7` yards and a human aims it left/right with
the Circle Pad (a full push runs to the sideline and can go out of bounds, which gives the receiver the ball at the 40).
In the end zone it is a touchback to the 25. The deep man nearer the landing spot runs to it while the rest of the
return team falls back into a wall in front of the catch, and the nearest man within 2.8 yards catches it (there is no
fair catch on a kickoff; a muff is rarer than on a punt). A human receiver steers any man, and B switches. The return is
the ordinary run-play code again, so juke, dive, fumbles, a tackle and a return touchdown all work. The clock runs from the
catch only (touchbacks cost one second).

`FootballLive.event`, `kick_yards` and `return_yards` drive the banners (KICKOFF 58 YDS RETURN 21, EXTRA POINT IS GOOD,
TWO-POINT CONVERSION GOOD / TRY STOPPED). A match is only saved at an ordinary playcall (`football_career_save_ready` refuses a
try), and the match always resumes with `td_points` at six. The opening kickoff is not played: a match still starts with
the home team at its 25. `tests/kickoff_test.c` covers the try (kick, block, two points, both outcomes of a real play),
touchdowns that are worth seven, the second-half and field-goal and safety kickoffs, human kicking and returning, a return
touchdown, 60 whole CPU games where every score is one of the known kinds, and replay determinism.

## Species candy

The human team's Pokemon earn species candy (the candy of the species each member started as, in the profile's starter
data, so it spends on passives and cost reductions like candy from a run) while playing. The engine only names the player
behind a play; the career pays.

- `FootballLive.credits` lists the plays of the last dead ball (at most four: an interception returned for a touchdown is
  two): the defender who intercepted (`FB_CREDIT_INTERCEPTION`), the rusher who sacked (`FB_CREDIT_SACK`) and whoever
  carried the ball in (`FB_CREDIT_TOUCHDOWN`, a return or a defender's touchdown too). A sack in the end zone is a safety and
  is still a sack. `stats[side][member][kind]` is the tally of the whole match (saturates at 255); both teams are tallied,
  only the human team is paid.
- `football_credit_candy`: interception 4, sack 2, touchdown 5 (`FB_CANDY_*`). Every one of the 22 also gets 1 candy for
  playing the match and 1 more for winning it, so a team that does nothing special still earns something.
- Timing: a career earns the play candy on the first tick after the dead ball (`seen_snaps` tells a new dead ball from the one
  already looked at; loading a save sets it, so a resumed match earns nothing twice) and the match candy once, when the match
  is over. Earned candy waits per member in `candy_pending`; `football_career_flush_candy` moves it to the profile and the
  screen calls it when it writes a checkpoint, just before it saves the profile. So the profile never has candy that no
  checkpoint knows about: quit mid-match and resume, and the plays after the last checkpoint are played again and earn
  again, and nothing is paid twice. The match candy is flushed by the shop checkpoint after a win and the end checkpoint after
  a loss. With no profile (`football_career_new(..., NULL, ...)`) nothing is earned.
- `candy_plays` and `candy_team` are the totals shown on the result and end screens (earned, flushed or not); they are not
  saved, so they count from the last resume. The banner of a credited play shows the species and the candy under the headline.
`tests/candy_test.c` covers the credit list and tally bounds, a touchdown, a sack by a human defender, a return touchdown, 30
whole games where every credit matches one play and the tallies match the credits, and 8 whole careers (candy earned equals
the tallies' worth, nothing reaches the profile between checkpoints, the team candy and the rest arrive with the shop or end
checkpoint, a resume earning nothing twice, a career without a profile); `live_screen_test` checks that a failed checkpoint write
leaves the candy pending and the saved one hands it over.

## Motion

The engine records the moments worth animating in `FootballLive.fx` (a ring of eight, `FootballFx`: kind, who, who it
happened to, which way, where, and the `steps` it happened at). It is not serialized, and the next play lining up clears it.
`football_live_fx` is called where the play is decided: a juke (a human's press or a CPU carrier's), a dive (every press of a
human defender, whether or not it reaches), a tackle, a sack, a fumble, an interception (at the ball), a catch and a
touchdown (the scorer: the runner, the receiver, a returner, or the defender of a pick-six). Nothing else in the engine reads
them, so they change no play.

`game/source/football_live_fx.c` turns them into motion from the recorded moment and `steps`, never from frame times, so a
moment looks the same at any speed. A player is lifted, shifted, stretched and turned (`football_live_fx_pose`, drawn with
`football_render_draw_sprite_pose`, the ordinary sprite turned about its middle: no new art): a juke shimmies and hops, a
dive launches, lies flat and gets up, a tackled runner falls the way he was hit, a sack throws the quarterback further and
shakes the field, an interception takes two jumps with rings and a PICK! popup, a touchdown is three jumps with the players
around him joining in a beat apart, a white flash, confetti and a banner that pops in. Dust, speed lines, outline ghosts,
impact stars and rings are plain rectangles. The banner now sits at the top of the field so the players stay in view.

The dead ball waits longer when there is something to see: `FB_LIVE_HOLD_TOUCHDOWN` (2.4 s) for a touchdown,
`FB_LIVE_HOLD_TURNOVER` (1.4 s) for an interception, a sack or a fumble, 1 s for everything else. `tests/fx_test.c` checks the
record (ring, guards, juke and dive, and over 30 whole games that every touchdown, sack, interception and fumble has exactly
its moment naming the player the credits name, that the hold is as long as the event says and that the next play starts
clean); `live_art_test` keeps every pose, shake and draw inside bounds for every kind and age.
`FB_FX_TURN` (+1) says a positive angle tips a sprite clockwise on screen, which is how citro2d takes it as far as I know; it
has not been seen on a console, and flipping that one number reverses every lean.

## Menus

The menus are built from the classic game's own widgets (`game/source/football_menu.c`): the grey bevelled windows (`ui_window`),
the emerald font (`text_draw`), the option list's cursor arrow and scroll bar, and the save-slot list's yellow frame for the button
that is chosen. Nothing in them keeps state; `football_live_screen.c` decides what to show.

- **Bottom screen** (320 x 240): a header window, one list window of nine rows (18 px apart, the same rows on every list so a tap lands
  where the cursor would), a hint bar or two buttons along the bottom. Draft, lineup, evolution and shop are that list; the lists page
  with Left/Right and show a scroll bar when there is more than a page. Text that does not fit steps down in size (never below 6 logical
  px) instead of running over.
- **Top screen** outside a match (draft, lineup, the hub, experience, shop, the end): the arena of the round behind a header, the
  Pokemon's sprite in one window and its name, cost or eight attribute bars in another, and a one-line tip. The match field itself
  (scoreboard, banner, effects) is unchanged.
- **Playcall**: offense, defense and special-teams cards are windows with the name inside and the route drawn over them, the chosen one
  framed in yellow. During a play the bottom screen shows the controls, the numbered receivers (X, Y, L, R, Select) with who they are,
  or a map of the field to tap a defender on.
- Host tests (`menu_test`, `live_screen_test`, `live_art_test`) run the real drawing code against fakes that record every window and
  text (`tests/menu_fakes.h`): nothing leaves its screen, every text sits inside a window and clear of every other, every card's
  diagram stays inside the card, and the screen draws every menu it has. They use a generous 8 logical px per character, so they
  cannot say how the real font looks. **Not seen on a console or an emulator.**

## Coverage, body contact and facing

`football_live_defense.c` owns snap-local coverage: four DL rush, LB/corners
match the RB, TE and three receivers, and two safeties protect deep halves. Man
assignments persist for the entire held pass; there is no three-second collapse.
Zone defenders match threats inside their area and break on throws inside that
responsibility. A five-man blitz rotates a safety onto the RB while keeping one
high safety. Confirmed handoffs, catches and a QB crossing the line release the
shell into run pursuit. A thrown pass only pulls its covering defender and nearby
zone/safety help, rather than sending all eleven players to the ball. CPU defensive
schemes depend on the situation/seed, not the selected offensive play.

`football_live_physics.c` resolves solid circular bodies for all 22 players,
including the controlled defender. Five OL engage nearby opponents with a
strength/stamina-dependent hold, expiry and re-engagement cooldown. Strength
controls push and inverse mass; four bounded contact passes remove overlap and
closing velocity. A defender can go around a blocker or push the line, but does
not phase through a body merely because the user controls it. Normal tackle
reach includes body radius so contact remains possible after separation.

Horizontal heading follows actual velocity and persists when moving vertically
or stopping. The QB looks toward receivers even while retreating. The direction
uses the original snap offense during a dead-ball turnover display. Existing
adversary sheets natively face left: live rendering mirrors them for rightward
facing, the inverse of the earlier mistaken convention. The pinned Charizard
sheet (`pokerogue-assets` revision `87426a7`, `images/pokemon/6.png`) was inspected
to confirm the orientation. Front art supports horizontal mirroring only; no new
up/down sprite sheets are introduced.

## Art, UI and camera

`football_live_art.c` adapts the supplied JavaScript's pixel palette, oblique
field, numbered targets and diagram cards into bounded Citro2D primitives.
Alternating turf, yard numbers, hash marks, end zones, sidelines and repeated crowd
pixels replace the flat full-field presentation. Both the line of scrimmage and
first-down line are visible. Team markers and a gold selection frame distinguish
control. Play cards have route diagrams; defense cards show coverage/rush patterns.
The browser's DOM/CSS assets, full stadium skyline and weather system are not
executed or claimed as ported. These source files are mechanics/art references.

`football_camera.c` shows 44 horizontal yards, with a 0.62 vertical squash,
exponential easing at 3.2/s and a maximum 18-yard/s horizontal follow speed.
The camera follows the controlled actor or airborne ball, with capped velocity
lead. A far-away new formation waits for framing before it can snap. It does
not reset zoom or jump to the new focus each play.

Field actors fit a 38-pixel box, compared with the previous 24-pixel cap. Existing
front/adversary assets are loaded once on formation/possession changes. The renderer
snapshots their first frame into independent textures of at most 64x64 pixels,
then releases the full animation atlas. `rr_tex_thumbnail` preserves trim offsets,
aspect and native RGBA8/RGBA5551 pixels with nearest-neighbor sampling. Twenty-two
retained cutouts use at most 352 KiB of GPU storage (176 KiB with binary alpha),
within the existing 4 MiB sprite budget. They are static cutouts moving in the live
world; full source animation sheets are not retained. Draft previews can retain
one source atlas. A missing asset or allocation failure still uses a team marker.
Transient loading, fonts and other application assets are outside that figure.

## Persistence and validation

The versioned `PRFBLIVE` codec serializes named existing mon packets plus explicit
little-endian lineup, boost, shop (including the evolution offers, from format version 2; a version 1 checkpoint loads with none),
round and match-call fields, with CRC validation. An offer that no longer holds against the roster (its member cannot use that item,
or it brings another species) refuses the checkpoint, as does one outside the shop.
It never serializes pointers or raw C structs. Size is capped before file reading.
Safe checkpoints are lineup/round hub, shop, final results and between snaps.
Live movement and pending EXP prompts are not checkpointed. START may return to
the last checkpoint, or explicitly leave without saving. Unreadable saves remain
untouched until explicit deletion. Profile and session are two separate files.

Host checks cover 55-point limits, locked/default-profile restrictions, valid
unlocked variants, cost reductions, saved-profile restrictions, assignment permutations, manual snapping,
movement, throws, deterministic replay, all-defender switching, bounded camera
movement, 20 full live matches, shared progression, codec rejection, shop currency,
and actual screen input/pause/save flows. Coverage through a four-second held pocket, mirrored matchups, zone discipline,
blitz matchups, sustained user-vs-OL body contact, passing around a body, native
sprite UV direction and the QB exception also have regressions. Live/career/screen
and coverage/physics/art tests are included
in sanitizer CI. The old balance audit describes the legacy snap engine; it is
not a win-rate claim for this new live engine. Visual and on-device performance
testing remain deferred to the next Windows build as requested.
