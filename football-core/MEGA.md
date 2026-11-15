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

## Kyurem and Calyrex (lorenzo 2026-11-15)

Dark Stone and Light Stone (Kyurem), Icy and Shadow Reins of Unity (Calyrex) are sold in the live career's two stone slots at a Mega Stone's price
(1000) and held, selected and played exactly like a Mega Stone: the selected holder plays the match as Black or White Kyurem, Ice or Shadow Rider
Calyrex (the form's base stats, weight and types), the roster member stays in its regular form, and the item is kept for later matches. The ready
screen's line reads "Form change" instead of "Mega evolution" once one of them is selected.

- Offered only for a member of that species in its base form who holds no stone or form item, and only when the classic game's own condition holds:
  the profile's dex has caught Zekrom (Dark Stone), Reshiram (Light Stone), Glastrier (Icy Reins) or Spectrier (Shadow Reins). The dex gates the
  offer, not a held item: a checkpoint with the item stays valid whatever the dex says (`gc_football_form_items` vs `gc_football_mega_form`).
- A member who can use one comes first, every round, so it is always there to buy. One takes the first stone slot and the Mega Stones keep rotating
  through the second; two (Kyurem and Calyrex on one roster) take both slots, and the Mega Stones come back as soon as one is bought; with more than
  two the pair moves on like the Mega Stones do. A species with two form items (both Zekrom and Reshiram caught) is offered one a round, by the same
  hash as the split Mega Stones, so over a playoff it is offered each.
- They are never in `gc_football_mega_items` / `gc_football_mega_item`, which stay the Mega Stones (the legacy six-player season is unchanged).
- Classic and Custom runs are a separate matter: there a Kyurem item is a rare reward (Rogue tier), not for sale. Its rate was measured with the
  port's own roll (`game-core/tests/kyurem_items_host_test.c` proves the wiring end to end).

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
