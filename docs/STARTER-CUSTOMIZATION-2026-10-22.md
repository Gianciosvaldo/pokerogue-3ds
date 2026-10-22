# Starter customization validation — 2026-10-22

The starter screen now offers Customize for moves, unlocked natures, shiny tiers
and abilities, species candy purchases and passive enable/disable. Preferences
are optional saved profile fields; older profiles remain readable. Failed profile
writes restore the previous choice and candy balance. Caught and minted nature
unlocks and existing catch/hatch/friendship candy acquisition remain in use.
The starter and command screens show team luck, with the native 15 grades.

## Source comparison

Pinned game source: `8555c08c823b856cbec4eb99ca84ea52a955836d`.
The committed native producer captures battle RNG before GameManager construction,
starts a battle naturally, then restores that RNG. Its fixture checks all 572
starter species, 791 forms and all 16 egg-move masks: 12,672 move pools, plus
passive candy prices and all 15 luck grades. The C comparator passes 181,889
checks, including purchase guards and save/reload. Native registry concatenation
order matters for form moves; battle level-up ordering remains separate.
Indeedee's native default adds Encore, changing the custom-run digest as expected.

Direct hidden-ability purchasing is the user's requested extension, at the same
species' upstream passive-unlock candy cost. Upstream instead sells species eggs.
Neither purchase consumes candy again when already unlocked.

## Validation

- Native producer regenerated the committed fixture byte for byte.
- Full game-core suite passes, including retained theft, Grip Claw, Baton and
  Mini Black Hole proofs, the mint/nature tests and normal save tests.
- Starter data: 51,280 checks; starter UI: 11,857 checks, 200 sessions and
  120,000 button presses. New flows cover each selector, both purchases, ability
  switching, saved reload, failed-write rollback and displayed luck.
- Full app suite passes: normal 10,000 runs, Scanner, system/save, quit,
  custom 200, custom resume and custom quit. No coverage floors changed.
- Actual devkitARM 3DS build passes. Strong startup stack is 524,288 bytes;
  2,271 frame reports checked, largest frame 45,056 bytes.
- Only the separate custom-only Task63 job remains intentionally skipped.

This validates the requested starter features within these checks, not completion
of all battle-core mechanics. Real-console interaction remains to be tested.
