# Defensive playcalling

The mode now waits for a defensive call on every opponent possession. The bottom
screen offers Man Coverage, Zone Coverage and Blitz, using touch or D-pad/A.
B resets the highlight to Man; START retains the normal save/return controls.
Thinking time consumes neither game clock nor match RNG. Calls are disabled
during the snap animation, and each choice is delivered once.

The three concepts adapt `DEF_PLAYS` from the supplied `playbook.js` to six-player
arcade squads. Man provides balanced run support and coverage; Zone lowers
completion odds and improves interceptions at the expense of run space and rush;
Blitz raises sack pressure but exposes passing gaps. These are initial bounded
probability/gain modifiers, not a simulation of eleven-player coverage zones.
Route/coverage/separation resolution is documented in `PASSING.md`.

When the player calls offense, the away defense is selected using down/distance
and seeded variation. It does not inspect the named offensive play. When the
player calls defense, the opponent's offensive call is generated only after
accepting that choice, using down/distance and the match RNG. Generating that
offensive call does not depend on which defensive option was selected, so the
menu cannot reveal or reroll an opponent play through idle input.

The UI opts the match into `manual_defense`. Portable/headless callers can leave
that flag false to retain automatic away snaps. `football_match_defend` accepts
only a legal defensive ID while awaiting the home defense; wrong-side, repeated
and invalid calls leave the entire match unchanged. Match ticks pause at manual
defense without consuming RNG. The existing match bridge still owns scoring and
exactly-once progression. Checkpoint loads reset menus and restore manual defense
for the app, with the same score, spot, clock, energy and RNG.

All definitions and menu state are fixed-size. There are no new textures, frame
loops, allocations or physics substeps. The bottom renderer uses the same
citro2d target and shared input events; the snap report names the chosen defense.

Validation: all three touch choices, edge rejection, D-pad wrap, back priority,
single delivery, wrong-side/invalid/repeated-call atomicity, and idle clock/RNG
checks. A 12,000-snap matched-seed comparison verifies identical opponent plays
across defensive options and the intended sack/completion/run-space tradeoffs.
Full-run, save/resume, contact, mega and render API tests remain enabled. CI also
runs defense tests under ASan/UBSan and builds the 3DS app with stack checks.
Visual testing remains deferred to the next Windows builder.
