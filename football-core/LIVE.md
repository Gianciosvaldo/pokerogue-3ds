# Live football (default entry)

The title's Football option now opens `football_live_screen.c`, a dedicated
22-player, 40-point draft and live playoff. The older six-player mode is retained
as `football_legacy_screen_open` and is explicitly accessible through START;
its `football.sav` file is preserved. New checkpoints use `football_live.sav`.
Classic parties, cost limits, saves and battle APIs remain six-wide.

## Roster and lineup

`football_roster.c` owns 22 `gc_mon` records, normalized football attributes,
boosts and a permutation assigning each member to one of 22 slots. There are
11 offense slots (QB, RB, three WR, TE, five OL) and 11 defense slots (four DL,
three LB, two CB, FS, SS). A swap always preserves one distinct member per slot.
Auto-assignment provides a starting point; it is never the only way to assign.

The draft uses classic species costs and profile cost reductions with a 40-point
aggregate limit. Football opens the full base-form pool independently of classic
catch unlocks, allowing a new profile to fill all 22 slots. Duplicate species and
evolved starting species are rejected. The catalog starts with cheaper species;
Left/Right pages, A drafts, B undoes the last draft. All 22 slots must be filled.
On the lineup screen A toggles editing and Left/Right swaps the selected slot's
member. Touch rows select slots and the bottom buttons cycle members while
editing. B finishes editing, then returns to the round hub. Auto-assignment and
Ready also have touch buttons. Lineups can be revisited between rounds.

One heap scratch `gc_run` creates members and runs the existing EXP/evolution
queue for one member at a time. Each winning match awards all 22 members the
existing football EXP schedule. Evolution targets are explicitly selectable,
including Nincada's mutually exclusive Ninjask/Shedinja choice. Move-learning can
replace a slot or be skipped. The shop retains Carbos, Protein and rare compatible
mega stones, using existing classic item icons. A mega holder is explicitly
selected between rounds; the form's stats and artwork apply for that match.

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
defenders pursue or cover, and a throw travels to a predicted receiver position.
Accuracy and arm range affect error; actors near the landing point determine
catches/interceptions. Passing beyond the line of scrimmage is disabled. Contact,
sidelines, scoring and a 25-second play bound end downs under the shared rules.
CPU offense chooses independently of the selected Man/Zone/Blitz scheme.
The clock display advances during live play; the shared rules commit its elapsed
time and runoff once at the dead ball. The seven-round playoff, shop and EXP loop
remains. This is arcade football: no field goals, punts, kickoff returns, penalties
or interception return phase are added here. A fumble uses a nearest-actor recovery
contest at its contact spot, without a bouncing-ball phase.

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
little-endian lineup, boost, shop, round and match-call fields, with CRC validation.
It never serializes pointers or raw C structs. Size is capped before file reading.
Safe checkpoints are lineup/round hub, shop, final results and between snaps.
Live movement and pending EXP prompts are not checkpointed. START may return to
the last checkpoint, or explicitly leave without saving. Unreadable saves remain
untouched until explicit deletion. Profile and session are two separate files.

Host checks cover real 22-member drafts, assignment permutations, manual snapping,
movement, throws, deterministic replay, all-defender switching, bounded camera
movement, 20 full live matches, shared progression, codec rejection, shop currency,
and actual screen input/pause/save flows. New live/career/screen tests are included
in sanitizer CI. The old balance audit describes the legacy snap engine; it is
not a win-rate claim for this new live engine. Visual and on-device performance
testing remain deferred to the next Windows build as requested.
