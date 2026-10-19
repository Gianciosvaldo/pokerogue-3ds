# Landing the Meloetta / Revival Blessing stack on main

PR #318 merged into the Meloetta branch as 2c20f981586804d6787f2995b69693d06f94790f.
PR #315 then merged that branch into the already-merged Disguise/Ice Face branch
as 26ba05398b9a1aa99c8b666bdc137334a5237217. Neither operation landed those two
features on main. The common ancestor of main06b5eac and the completed stack
f4b7e3b is 4b0fd14, the Disguise/Ice Face head. A merged PR state alone therefore
does not establish that its commits reached main.

Apply the original Meloetta and Revival Blessing commits onto main06b5eac,
including their native producers, strict comparators, mutation guards and game
host tests. Preserve main's Aftermath/Innards Out, Tera Shift, form-event gender
and explicit Illusion deviation. The existing IllusionBreak phase ID remains
stable; RevivalBlessing is appended after it. Keep every host test target and
recipe from both sides of the Makefile merge. Canonical source fixtures keep
all deviation flags clear; game callers retain main's explicit deviation flags.

The original proofs and reproduction commands are in
MELOETTA-FORMS-VALIDATION.md and REVIVAL-BLESSING-VALIDATION.md. Integration checks
replay both seeds of those unchanged raw and complete-turn captures, plus the
other 28 cross-slice captures (36 total), and compare all seven retained suites'
covered IDs against the completed stack. No expected fields, text, RNG or coverage
floors are changed to accommodate the merge. Run both compiled mutation suites
again on the combined code, all normal game-core and app/custom/save tests, and
the CI-equivalent devkitARM 3DS build. Normal exact-head remote CI must pass
before merging this integration directly into main.

Future stacked PRs must be retargeted to main after their parent merges. Verify
main ancestry as well as GitHub's merge status before marking a slice landed.
