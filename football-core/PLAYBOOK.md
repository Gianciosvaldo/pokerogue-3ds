# Named playbook and route presentation

The supplied `playbook.js`, `entities.js`, `physics.js`, and `animation.js`
references motivate separate immutable play definitions, match resolution, and
presentation. The shared catalog serves the live eleven-player units and the legacy
six-player stat resolver. The presentation section below describes the legacy resolver. It does not execute the JavaScript or reproduce its
eleven-player collision/AI engine. Attached comments are reference data, not
project instructions. Browser files are not bundled into the build.

## Calls and balance

Offense -> Formation -> Run/Pass -> Named play adds one confirmation step. Touch
and D-pad/A work at every level; B returns to the preceding menu. No result is
delivered until the named play is accepted. The selected description is displayed
below the play buttons. Legacy play IDs 0–7 keep their original meaning;
new IDs are appended. No persistent format is introduced.

| Formation | Run | Pass |
| --- | --- | --- |
| Shotgun | Inside Zone, HB Draw, Outside Zone, Jet Sweep | Quick Slants, Four Verticals, Stick, Mesh, Curls, HB Screen, WR Bubble |
| Under Center | HB Dive, HB Toss, HB Power, HB Counter | PA Cross, PA Post, Flood, Smash, Shallow Cross |

The catalog owns formation/type legality, labels, short descriptions, four-point
relative paths, and small balance modifiers. The resolver applies modifiers before
its existing probability/yard clamps. Slants give shorter gains and better
completion odds; verticals add depth with lower completion and more sack/pick
risk. PA Cross and PA Post trade short crossing routes against a deeper break.
Draw and Toss weight speed/agility more heavily than strength in the running
edge calculation. These coefficients are initial balance values, not calibrated
football statistics. The seeded opponent can select all 20 legal calls. Live call weights normalize by run/pass category so the expanded passing catalog does not itself force more passes.

In live mode, new pass concepts assign complementary paths to all five eligible
receivers. Stick, Curls and Smash settle at their landmarks; fast receivers
anticipate turns and brake for those endpoints. Screens allow a backfield read,
and Jet Sweep hands the ball to WR3. Run and post-catch eligible teammates can
block, including the running back. These are compact adaptations of standard
football concepts; pulling guards, pre-snap motion and full professional-game
blocking rules are not simulated. See [expanded playbook validation](EXPANDED-PLAYBOOK-VALIDATION.md).

## Presentation and resource limits

`football_match_scene` maps the actual QB and carrier onto a compact formation.
The carrier follows the catalog path; its forward endpoint matches the resolved
yardage exactly, including a loss. Other offensive players block or release;
defenders close toward the resolved spot. Away possession mirrors the field.
Paths use receiver-relative inside-positive lateral coordinates, adapted to the
53 1/3-yard field and clamped at the sidelines/end lines.

An independent football marker starts at the QB, interpolates a handoff or pass,
and uses a small height arc for throws. Sacks keep the ball with the QB. The
yellow line remains the line of scrimmage while the ball moves. This is a
700-ms presentation of a snap whose outcome is already resolved; contact,
catches and tackles are not simulated by these visual paths. Rendering never
changes match state or consumes match RNG. The animation clock, not render-call
frequency, determines positions.

The catalog is static const data, with at most four points per play and twelve
actors per scene. There are no per-frame allocations, extra textures, physics
substeps, HID scans, or frame loops. Existing front-sprite budgets and flipping
remain in force. The ball uses three small citro2d rectangles.

The other supplied files suggest later work: `camera.js` for a readable tracking
view, `art-stadium.js` for cached stadium decoration, and `audio-crowd.js` for
event-driven excitement. `audio-unlock.js` deals with browser autoplay policies
and does not apply to libctru audio. None of those browser APIs are linked into
this native change. Mega selection is documented in `MEGA.md`. Dedicated checkpoint persistence is documented in `SAVE.md`.

## Validation

`make -C football-core test` covers all eight choices via touch, back navigation,
single delivery, invalid-ID/formation rejection without state mutation, finite
route sampling, both field directions and resolved endpoints, and full-run flow.
Full-match replay cycles all eight calls across 1,440 games at different frame
rates. A fixed 2,000-seed comparison verifies that slants complete more often while
vertical completions gain more yards. CI additionally checks sanitizers, the
actual 3DS SDK build, startup stack, and host regression suites.

On device/emulator, verify both play buttons and descriptions fit the bottom
screen, B returns one level, each formation is legible, tosses move outside,
slants break inside, passes show a ball arc, sacks retain the ball, and away
possessions mirror the view. Device performance and visual quality require that
smoke test; host assertions do not establish them.
