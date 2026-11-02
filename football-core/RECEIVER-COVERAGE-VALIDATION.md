# Receiver reads and outside zone coverage

QB uncertainty now applies after the opening-score cap. Previously all sufficiently
open targets collapsed to the same score and the first read won: a fixed 400-snap
matched-stat zone probe picked WR1 300 times and WR2 zero times on each offense.
No position quota is introduced. Tests require varied choices between equal
windows and preserve selection of the only open receiver at every position.

Zone hook defenders now start inside and the flat corners outside, instead of
placing both groups in the seams and duplicating inside marks. Coverage aims
outside the receiver's physical body; the previous one-yard zone target could
pin the receiver. Both field directions retain outside WR responsibilities.

Validation: all `football-core` tests pass, including 1,343 retained movement
checks, complete CPU games/reproducibility, real touch/render/career/save checks,
blocking/contact and return-camera cases. Added isolated QB-read coverage uses
4,000 seeds for each of three accuracy profiles on both sides, plus 100 seeds
for each uniquely open receiver in those six contexts. Zone responsibility and
body-clearance checks cover both directions and all four existing passing plays.
The original routing, movement and gameplay thresholds are unchanged.

These observations do not establish every roster matchup or force balanced
statistics in genuine coverage mismatches. Actual 3DS/ARM and normal app/core/
custom/save/football checks remain merge requirements; only dedicated Task63
is excluded. No builder is produced.
