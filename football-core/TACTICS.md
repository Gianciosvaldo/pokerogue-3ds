# Contact tactics and loose-ball recovery

Before an offensive snap, Left/Right or the contact row cycles Balanced, Juke and
Power. Before an opponent snap, Left/Right or the tackle row toggles Standard and
Dive. These are deliberate pre-snap policies, not live movement controls. Menu
input changes no match RNG or clock. Choices persist through that match and in
its checkpoint; the next match starts Balanced/Standard. Calls cannot change
during an animation. The opponent chooses its carrier/contact policy with seeded
variation, without revealing an offensive play in the defense menu.

Juke uses AGI against the defender's AGI to reduce tackle probability. Power uses
STR for a stiff-arm advantage. Both consume one extra carrier energy on an actual
contact and add a small fumble-risk cost on successful tackles. Balanced avoids
those costs. Dive uses defender AGI to improve tackle probability and raises
impact; a failed dive gives the carrier two extra escape yards. Actual dives cost
two extra defender energy. All probabilities, energy and gains remain clamped.
These concepts adapt the supplied `physics.js` juke/stiff-arm/dive/contact logic
to the existing snap resolver, without runtime cooldown or velocity simulation.

A contact fumble now runs one recovery contest. Each side's best available
speed/agility/catching candidate, after difficulty/fatigue, supplies the contest;
the chance of an offensive recovery is clamped to 15–85%. This is a bounded
recovery estimate, not a bouncing-ball/proximity simulation. Offensive recovery
retains possession at the fumble spot with normal down/first-down rules; a failed
fourth down can still turn the ball over. Recovery adds no further advance.
Defensive recovery uses the existing lost-fumble rules. Own-end-zone offensive
recovery produces a safety; opponent-end-zone recovery uses normal scoring.
The snap report distinguishes offensive/defensive recovery. The appended
FUMBLE_RECOVERED result preserves the original result IDs.

The contact report records the fumble even when recovered. The final resolved
play and existing rules remain authoritative for possession, score and clock.
Fatigue uses the snap's original offense, so a turnover cannot charge exertion
to the wrong team. There is one carrier contact and at most one recovery contest
per accepted snap, bounded six-player scans, explicit rolls and no allocations.

Checkpoint version 2 stores runner/dive policies. Version 1 active-match saves
load with Balanced/Standard defaults while preserving RNG and match state. Tests
cover this migration, policy restoration, atomic choices, both recovery outcomes,
down/first-down/safety rules and juke/dive exertion over 8,000 initial snaps.
Existing full-match replay, passing, defense, mega, checkpoint/file recovery and
seven-round flows remain enabled. CI adds tactics tests to ASan/UBSan and runs
the 3DS build and stack checks. Visual testing remains deferred to the next
Windows builder; no live collision or visual validation is claimed.
