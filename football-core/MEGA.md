# Match-only mega activation

On the round introduction screen, Left/Right or the Mega touch row cycles through
None and teammates holding an applicable mega stone. A/Continue starts the match
with that selection. Exactly one slot can be selected. Selection is optional and
cannot change during a match; it resets to None when the match ends.

The rare shop offer uses the existing form-change catalog and held-item inventory.
The holder must match the item's species and current pre-form key. The generated
inactive transition identifies the corresponding mega form, including distinct
Charizard X/Y and Mewtwo X/Y targets. Conditional transitions that the football
adapter cannot evaluate, primal/gigantamax forms, and move-only transformations
are excluded. The shop does not offer another stone for an already eligible
holder. Inventory capacity, duplicate and purchase currency guards remain active.

At match setup, the selected player's copied `FootballPlayer` is generated from
the actual mega form's base stats, weight and types using the same Pokédex maxima
as every other player. Existing Carbos/Protein bonuses are added once and clamped
to 99, and positional affinity is recalculated. The sprite loader already reads
the copied form index, so it loads the transformed front sprite between rounds
within the existing texture budget. Missing art keeps the existing marker fallback.

The `gc_run` Pokemon remains in its regular form throughout. EXP, evolution,
profile updates and subsequent shops therefore never inherit a temporary mega
form. The stone is retained for selection in later matches. No classic battle
callbacks, profile writes, extra roster copies or mid-frame texture reloads are
needed for activation. The implementation follows the supplied JavaScript's
separation of roster/entity data, match simulation and presentation; it does not
add a browser runtime or change the snap resolver into a collision simulation.

`make -C football-core test` includes a purchase-to-selection-to-match test,
selection cancellation, wrong-holder/item rejection, phase guards, retained
regular form, vitamin boosts, X/Y identities, and generated-catalog coverage.
The existing full-run tests continue to verify EXP/evolution/shop integration.
CI checks the native host regressions, sanitizers, 3DS build and ARM stack reports.

Per the requested release workflow, visual testing is deferred to the next
Windows builder. That pass should check the selector text, transformed front
sprites, regular-form restoration on the next round, missing-art fallback and
play animations. No device visual/performance validation is claimed here.
Football checkpoints preserve stone ownership and mega selection; see `SAVE.md`.
