# Adapted football AI behaviours

The user-supplied `ai.js` (SHA-256
`e9708330d2488b6dbc7b95378c614a36547f6f34403dc84193467ff4ad1e1e2e`)
is a browser simulation that imports separate configuration, playbook and physics
modules. It is a reference for these two changes, not a drop-in implementation.

* `routeThink` brakes and settles before the rear of the end zone. The C field
  runs from 0 to 120 with goal lines at 10 and 110. Receiver route intent now
  brakes toward 3.5/116.5, after obstruction-release steering. The four-yard
  braking region covers high-speed, low-agility actors. Existing polyline route
  projection and release behaviour remain in use. No new curl/screen plays are
  claimed; those JavaScript playbook definitions were not supplied.
* `passPro` prioritizes rushers by distance to the quarterback. The C shared
  assignment retains scarce-edge coverage first and uses quarterback distance
  to resolve ties, replacing defender array order. Engaged pairs remain stable.

## Validation

Added cases to the existing movement regression target (already in normal and
sanitized CI). Before the changes, 98 checks failed: 48 route cases reached the
rear boundary, 48 failed to settle, and both mirrored protection cases chose the
farther rusher. Afterward, all 1,343 checks pass, including retained path-following,
Electrode release, cross-field legs, edge assignment and next-snap state checks.
The new route cases cover all four pass plays, both directions, four attribute
profiles (including 99 speed / 1 agility), and obstruction-release state. They
use the real movement integrator. ASan, UBSan and LeakSanitizer pass.

Retained AI tests play 40 full CPU games twice for determinism and 10 different
seed games. The primary set has 2,252 snaps, 23.1 points/game, 43% runs, 58%
completions, 2.6% sacks and 1.9% interceptions, passing all existing floors.
The retained live tests pass 20 full matches; live-system coverage/contact/
facing tests pass. No thresholds were changed. Logs are saved locally under
`work/football-ai/upload-*`.

These tests establish the specified fixes and retained behaviours; they do not
establish parity with the entire JavaScript AI or eliminate all football AI bugs.
No builder is included.
