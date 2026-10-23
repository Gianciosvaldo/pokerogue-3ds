# Passing resolution

`football_pass.c` adapts pressure, route timing, throwing accuracy and contested
catch concepts from the supplied `physics.js`. It remains a bounded snap-level
estimate, without real-time ball integration, live separation coordinates or
Gaussian aim errors. Presentation and simulation remain independent.

Each named passing concept has an intended depth and release time: Slants
8 yards / 450 ms, PA Cross 12 / 1,000, Verticals 26 / 1,300, PA Post 24 / 1,500.
Depth models the throw; final gain also includes a bounded after-catch component,
catalog gain tuning and the later carrier-contact result. The animation still
uses the existing route points and final resolved spot.

The match selects a coverer using speed, agility, catching and TE/S affinity,
separate from the rusher when the defense has multiple players. Receiver speed
and agility against that coverer, route breaks, and the chosen coverage produce
a separation estimate. Zone protects deep lanes; Blitz exposes space. The rusher's
power/agility, QB mobility, blocker edge and release delay produce pressure.
Long-developing reads and blitzes expose the QB for longer. A throw beyond the
QB's THP-derived arm range gets an accuracy penalty.

THA and receiver CTH, separation, pressure, arm shortfall, and catalog/coverage
modifiers feed completion chances. Pressure/read time feed sacks; the coverer's
catching, QB accuracy, throw depth, separation and pressure feed interceptions.
All probabilities are clamped. Three explicit supplied rolls resolve pressure,
catch and gain, with no owned RNG or allocation. Inputs are validated and failed
calls leave output unchanged. Sacks retain the QB as carrier. Completed catches
hand off to the actual coverer's contact contest, which can add broken-tackle
yards or cause a post-catch fumble. The pass report is the pre-contact outcome;
the match's resolved play is the final authoritative result.

The screen describes pressure, an open receiver or a contested lane during the
snap report. It does not expose internal percentage calculations to the player.
The away offense uses the same model with difficulty/fatigue-adjusted ratings;
mega attributes enter through the same effective-rating path.

Tests cover invalid inputs atomically, arm limits on deep throws, higher THA,
receiver/coverer speed and agility, protection/pressure, release/depth differences,
and 120,000 pressure/catch probability cases across every pass/defense pairing.
The full football suite also covers full-match replay, the defense risk/reward
comparison, contact, mega, checkpoint restoration and seven-round progression.
CI includes pass tests under ASan/UBSan, host regressions, the 3DS build and stack
checks. Coefficients remain initial arcade balance values. Visual testing is
deferred to the next Windows builder.
