# Match-only mega activation

On the round introduction screen, Left/Right or the Mega touch row cycles through
None and teammates holding an applicable mega stone. A/Continue starts the match
with that selection. Exactly one slot can be selected. Selection is optional and
cannot change during a match; it resets to None when the match ends.

In the live 22-player career, every shop offers up to two Mega stones (offers 2
and 3), each for a different teammate who can use one and does not hold one yet.
Each costs 1000; the first win pays 625, so an offer can be saved for. The pair
moves two places along the list of eligible teammates each round, so with more
than two eligible teammates the offers cycle from round to round and every
holder comes up; with exactly one eligible teammate there is one offer. The shop
names each stone by its real name (Rayquazite, Mawilite...) with its fixed holder,
previews that holder, and explains that Mega must be selected before the next
match. Vitamin target controls do not change a stone's holder. The legacy six-player season retains its rare offer.

Both modes use the existing form-change catalog and held-item inventory.
A species with two stones (Mewtwonite X and Y, Charizardite X and Y, Raichunite X and Y, Absolite and Absolite Z, Garchompite and
Garchompite Z, Lucarionite and Lucarionite Z) is offered each of them in turn: `gc_football_mega_items` lists every stone the member
can use, and the shop picks one with a hash of the seed, the round and the member (lorenzo 2026-11-08: only Mewtwonite X ever came up,
because the shop took the first stone of the catalog every round). The pick is deliberately not the round's parity: the pair of
holders moves by two a round, which would pin a holder to one parity. A holder buys one stone and keeps it; the other is not offered
for it afterwards. The legacy six-player season still offers the first stone (`gc_football_mega_item`). `tests/mega_split_test.c`
sweeps all six species.

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
