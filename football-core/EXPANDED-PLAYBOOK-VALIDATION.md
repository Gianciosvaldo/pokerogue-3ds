# Expanded football playbook

The live and legacy catalogs now expose 20 offensive calls (eight runs and twelve
passes) and eight defensive schemes. Existing offensive IDs 0–7 and defensive IDs
0–2 retain their values. This work builds on the receiver-read/zone-spacing fix
in PR #529; it does not replace that regression coverage.

New offense: HB Power, HB Counter, Outside Zone, Jet Sweep, Stick, Mesh, Flood,
Curls, Smash, HB Screen, WR Bubble and Shallow Cross. New passing concepts assign
separate WR1/WR2/WR3/TE/RB paths. Momentum anticipation handles the turns, and
hitches/curls brake and settle. Screen reads can target their intended receiver
behind the line; Jet Sweep uses WR3, including its scripted steering. Eligible
teammates block on runs and after a catch, including actual RB contact blocking.

Defense: 2-Man, Cover 2 Zone, Man Blitz, Cover 1 Robber, Cover 3, Cover 4 Quarters,
Zone Blitz and Cover 0 Blitz. The live engine shares explicit rusher/deep masks
with protection planning and preview diagrams. Schemes have four, five or six
rushers and zero to four deep defenders; man schemes retain five unique eligible
receiver assignments. One-high help reads both sides. The CPU weights schemes
by down, distance and field position. Offensive weights normalize within the
run/pass categories before applying existing situation and recent-call weights.

The live card screen has five offensive pages followed by special teams, and two
defensive pages. Two-point attempts include all five ordinary offensive pages;
the extra-point choice stays on its own screen. L/R/footer touch changes page;
D-pad/A and card touch select. The legacy formation/type menu uses two columns
with up to four rows. Card diagrams stay inside their windows, including
negative-depth run paths and short screen routes.

## Checks

- `expanded-playbook-test`: 240 isolated trajectories (eight new pass concepts,
  five eligible receivers, both field directions, three speed/agility profiles),
  using real steering/acceleration. Every trajectory stays within one yard of its
  assigned polyline. Settling routes finish within 0.3 yards and below 0.3 yd/s.
  All eight actual defensive setups are checked in both directions and near the
  goal line, with independent expected rush/deep counts, unique man assignments,
  legal positions and outside-corner responsibilities. Real Jet Sweep snap,
  carrier steering, RB block contact and pre-/post-catch eligibility are checked.
- `live-screen-test`: selects every offensive/defensive call through the actual
  screen using alternating buttons and touch; visits every card and preserves
  existing special-teams/try, draft, pause, shop and save checks.
- `career-test`: all 160 play/scheme combinations produce byte-identical safe
  version-2 checkpoints; reload preserves the saved RNG and permits the new call.
  Calls themselves are deliberately not serialized: resume opens play selection.
- `pass-test`: all twelve pass concepts against all eight schemes, 10,000
  probability cases each (960,000 total), retaining atomic guards and exact
  sack/interception counts. Legacy resolver full-game tests remain in place.
- `ai-test`: all 20 calls sampled and played, retaining situation, scoring,
  run/pass, completion, sack, interception and repeat-call bounds. Original-eight
  per-call variety floors are measured within the original-eight cohort; twenty
  absolute shares each above five percent would be impossible. Each catalog call
  additionally exceeds 1.5% of neutral samples. Original QB uncertainty and
  outside-zone coverage regression cases remain.
- Original `movement-regression-test`: all 1,343 checks retained unchanged.
- `make -C football-core test`: full normal football suite. CI also runs normal
  battle/game/custom/save checks, app host tests and the actual 3DS/ARM stack
  gates. Expanded-playbook checks are included in the sanitizer job.

These are deterministic bounded tests, not a calibrated football balance study
or device acceptance. Concepts are adaptations: there are no new personnel
formations, pulling-guard assignments, pre-snap motion or full professional-game
blocking rules. On-device visual/performance testing is still needed. This
football change does not close the separate pinned-upstream battle-core ledger.
