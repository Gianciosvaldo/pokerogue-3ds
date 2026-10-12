# Battle-core continuation audit

Starting point: main `efd64988c4a1ab3755f999b29c8eac0c48ad8d49`.
Target remains source `8555c08c823b856cbec4eb99ca84ea52a955836d`, Node 22.
This is a work inventory, not a completion percentage or a passing test record.

Card 103 trainer routing is already merged in #185. Ordinary reserve ownership,
callbacks, switching moves and the pinned Roar guard should not be reimplemented.
Its historical optional-index regression was subsequently repaired in #218.
The leave-field primal weather dispatcher is merged in #230:
Primordial Sea/Desolate Land/Delta Stream weather cleanup. Neutralizing Gas's
source-leave tag lifecycle is a separate slice, not covered by that repair.

Next independent source-backed slices, in likely implementation order:

| Slice | Current concrete gap | Required proof |
| --- | --- | --- |
| Wonder Skin (merged #232) | Real accuracy dispatcher and leaf now implemented | Status versus damage, accuracy below/at/above 50, bypass/suppression, stage/item ordering, whole turns/RNG/text |
| Priority brackets and Quick Draw (this slice) | Real bracket, chance and refusal callbacks now implemented | Real source conditions and seeded order, refusals, passives, priority ties; preserve Quick Claw |
| Protean/Libero | PokemonTypeChangeAbAttr move callers record unsupported | Source type-change timing, existing type and Tera exclusions, repeated type changes (the pin has no Gen 9 once-per-summon gate), switch reset |
| Receiver/Power of Alchemy | CopyFaintedAlly needs victim context in the knockout dispatcher | Ally versus enemy victim, eligibility, copied primary/passive and source order, loaded doubles |
| Parental Bond / Beat Up | AddSecondStrike and variable multi-hit dispatch remain | Move exclusions, hit powers/order, targets, KO interruption, exact RNG consumption |
| Cud Chew / Harvest | Berry recording, repeat consumption and restoration callbacks remain | Actual named held-item history and delays, ownership, duplication/refusal and RNG |
| Transform / Sketch / ability copying | Remaining move effects and identity overrides | Real source identity/stat/moveset copying, bounds, reset and state serialization |
| Forms / Battle Bond | Remaining form callbacks and one-use battle flags | Actual conditions and new required state; do not invent absent lifecycle inputs |
| Redirect / Commander / broader doubles | Singles proofs do not close all partner behavior | Natural field/partner state, target redirect and phase ordering with exact source fixtures |
| Final-boss and interlude boundaries | Explicit PR_FEAT_FINAL_BOSS / interlude caller flags | Actual source phase transitions; coordinate run/game prerequisites with their owners |

Other missing moves and conditions must be prioritized using current replay
failure/unsupported reasons, not just class names. A static comparison finds
40 instantiated ability classes without an apply_one switch case, but several
are handled elsewhere or are AI/encounter metadata; **40 is not an unsupported
ability count**. Current retained rich seed 777 has 1,871 exact covered turns
and 129 explicitly unsupported turns before this continuation. Its leading
reasons include priority brackets, field priority immunity, ability bypass,
Pokemon type change, form changes and berry restoration. Fresh before/after
replays are required for each implementation.

Each slice keeps its own branch/PR, actual unchanged source methods, strict
complete-case guards and mutation checks. Source expectations, retained seeds,
counts and tolerances remain unchanged. Host save/load consistency and 3DS CI
are required separately. Custom-core CI is excluded from these battle tasks by
the user's current instruction. No full battle-core, run or hardware parity is
claimed until the remaining source paths and their prerequisites are proven.

Priority continuation on base main `fa1cb29`: rich seed777 now has 1,896 exact
covered turns, up from 1,872; summon 1,869, up from 1,868. Other retained coverage
is unchanged. The dedicated source matrix matches 8,000 complete turns and
16,000 direct bracket probes. See PRIORITY-BRACKETS-VALIDATION.md for the
boundaries; these counts do not establish a completion percentage.
