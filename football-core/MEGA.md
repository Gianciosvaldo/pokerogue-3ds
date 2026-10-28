# Match-only mega activation

On the round introduction screen, Left/Right or the Mega touch row cycles through
None and teammates holding an applicable mega stone. A/Continue starts the match
with that selection. Exactly one slot can be selected. Selection is optional and
cannot change during a match; it resets to None when the match ends.

In the live 22-player career, each shop offers a usable first stone while no
teammate owns one. It costs 1000; the first win pays 500, so the offer remains
available in subsequent shops while the player saves. Once a stone is owned,
additional offers use the existing one-in-four-round schedule. The recipient
rotates among compatible teammates; already equipped members are skipped. The
shop names the stone and its fixed holder, previews that holder, and explains
that Mega must be selected before the next match. Vitamin target controls do not
change the stone's holder. The legacy six-player season retains its rare offer.

Both modes use the existing form-change catalog and held-item inventory.
The holder must match the item's species and current pre-form key. The generated
inactive transition identifies the corresponding mega form, including distinct
Charizard X/Y and Mewtwo X/Y targets. Conditional transitions that the football
adapter cannot evaluate, primal/gigantamax forms, and move-only transformations
are excluded. The shop does not offer another stone for an already eligible
holder. Inventory capacity, duplicate and purchase currency guards remain active.

At match setup, the selected player's copied `FootballPlayer` is generated from
the actual mega form's base stats, weight and types using the same Pokédex maxima
as every other player. Existing Carbos/Protein bonuses are added once and clamped
to 99, and positional affinity is recalculated. For the live career, the boosted attributes are copied to both the match's
home-unit pool and any selected member already active in the opening formation,
including defensive players and rearranged lineups. Subsequent snaps copy from
that same pool. The sprite loader already reads
the copied form index, so it loads the transformed front sprite between rounds
within the existing texture budget. Missing art keeps the existing marker fallback.

The `gc_run` Pokemon remains in its regular form throughout. EXP, evolution,
profile updates and subsequent shops therefore never inherit a temporary mega
form. The stone is retained for selection in later matches. No classic battle
callbacks, profile writes, extra roster copies or mid-frame texture reloads are
needed for activation. The implementation follows the supplied JavaScript's
separation of roster/entity data, match simulation and presentation; it does not
add a browser runtime. The live career uses the existing fixed-step collision simulation.

`make -C football-core test` includes a purchase-to-selection-to-match test,
selection cancellation, wrong-holder/item rejection, phase guards, retained
regular form, vitamin boosts, X/Y identities, and generated-catalog coverage.
The live-career regression additionally purchases a real generated shop offer
through EXP/evolution progression, checks affordability and duplicate guards,
verifies the opening actor and unit pool, and saves/resumes the selected Mega.
The existing full-run tests continue to verify EXP/evolution/shop integration.
CI checks the native host regressions, sanitizers, 3DS build and ARM stack reports.

Per the requested release workflow, visual testing is deferred to the next
Windows builder. That pass should check the selector text, transformed front
sprites, regular-form restoration on the next round, missing-art fallback and
play animations. No device visual/performance validation is claimed here.
Football checkpoints preserve stone ownership and mega selection; see `SAVE.md`.
