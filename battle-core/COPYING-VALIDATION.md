# Sketch and Mimic

Pinned upstream: `8555c08c823b856cbec4eb99ca84ea52a955836d`.
Pinned locales: `270ed0a2938b2c3e2e34e7de940e84aecadf548b`.

Sketch now checks the source ban list and known effective moves, then uses
Pokemon.setMove semantics: replace the permanent slot and, when present, the
temporary slot with the same new move object. Mimic creates a temporary
moveset without changing species, form or sprite. Its copied slot is independent;
the remaining slots retain the source's shallow-copy sharing with permanent
moves. Transform creates independent temporary moves again. Learning changes
only the permanent moveset. Summon cleanup removes temporary moves while keeping
Sketch and PP spent on shared permanent moves.

C represents shared PokemonMove objects with a per-slot alias mask. PP spending,
restoration, PP Up, Spite/Eerie Spell, Grudge and Leppa updates synchronize shared
slots. Queued move phases retain their old move values when Sketch, Mimic or
Transform replaces a referenced object. The existing Transform identity/sprite
bridge is unchanged; Mimic uses its temporary moves without enabling that bridge.

## Source comparisons

The API producer calls the actual source conditions, attribute apply methods,
Pokemon.setMove, finite-PP PokemonMove.usePp and resetSummonData. It compares
permanent/effective moves, PP, sharing, retained references, messages and RNG.
Seeds 777 and 2034 each have 4,000 complete exact cases. The six contexts include
Sketch, Mimic, setMove, permanent learning, Mimic followed by Sketch, and missing
Sketch. Reachability checks require successful copies and the negative control.

The turn producer runs the actual TurnStart/Move/MoveEffect/MoveEnd and end-turn
phases. Commands select existing effective moves; they do not replace fixture
moves. Ordinary copying, duplicate rejection, Transform followed by Sketch or
Mimic, and following-turn copied PP match all 8,000 complete turns across seeds
777, 9001, 4242 and 31337. Another 2,000 complete turns at seed 4242 cover real
Mimic followed by Transform, including subsequent copied-move PP. Every case is
covered, with exact state, phase queues/order, messages and RNG; none are skipped.
Presentation and dump transport stubs preserve the actual gameplay methods and
asynchronous phase order. Sparse permanent holes are dumped as absent moves.

Reproduce with Node 22, the pinned upstream and its installed dependencies:

```sh
tools/copying-diff.sh /path/to/upstream 4000 777 api
tools/copying-diff.sh /path/to/upstream 2000 777 turn
tools/copying-diff.sh /path/to/upstream 2000 4242 transition
TURNTEST_LOCALES=/path/to/upstream/locales/en python3 tools/check-copying-proof.py \
  build/copying-api-777.txt build/copying-turn-777.txt \
  --transition build/copying-transition-4242.txt
```

Eleven compiled mutations are caught, testing ban/duplicate checks, both copying callers, permanent
replacement, copied-slot independence, other-slot sharing, learning, retained
references, shared PP, and Transform clearing Mimic aliases. Malformed input
checks reject 15 malformed API traces and eight malformed turn traces, including
incomplete/versioned API traces and out-of-range numeric, text
and new temporary-move transport fields. Comparisons retain existing tolerances.

Retained before/after coverage increases from 1,950 to 1,951 exact rich turns
and 1,964 to 1,965 plain turns. The other retained singles, switch, summon, item
and trainer traces have no covered mismatch or coverage loss. The retained
72,000 dedicated ability/Beat Up cases and 16,000 Ditto cases also pass.

## Commands and saves

The 70-check game-core host test uses actual commands, knockouts and reward
continuations to legal encounter save points. It covers Mimic, Sketch during
Mimic and Transform, subsequent shared PP, save/load and summon cleanup. New
alias/active metadata is an optional atomic two-field group. Partial groups,
invalid flags/masks, aliases outside both movesets and inconsistent shared
fields are rejected. Historical normal saves can omit both fields; historical
Transform saves without metadata keep independent temporary moves. Removing
both fields from a transformed new save can resemble that historical format;
the reader does not infer object sharing from equal move values. Eight malformed
copying states are written at legal save points with valid checksums and rejected
on loading, separately from the partial-schema checks.

Full game-core tests pass. Both 1,000-run paths finish, and the paired 200-run
save/no-save digest remains `c6cf5f73`. These check integration consistency,
not upstream run generation parity.

The initial app soak reached another instance of the previously source-proven
Helping Hand stalemate, at run 1,510, wave 14: Jirachi and an Imposter Ditto had
exhausted their other moves. The fake singles player now declines learning
nearby-ally-only moves as well as replacing them when learning a usable move.
It still consumes the confirmation RNG draw. No source gameplay, expected
output, seeds, run counts, progress budgets or coverage floors change. The
10,000-run app check then passes with 119,647 waves, 486,887 commands, 60 completed
and 67 cancelled evolutions, 17 checked evolution moves and zero stalls or model
violations. Source battle comparisons remain separate from this legal test
player policy.

The custom resume check also exposed the app catch-up path emitting healing
before a changed maximum HP. It now emits the maximum first, like the battle
event converter. The regression is the unchanged 100-run custom resume check;
the battle core's HP values are not altered by this rendering fix.

## Boundaries

This slice does not establish complete battle-core parity, every doubles
interaction, arbitrary in-flight summon-reset references, or negative PP on
direct unlimited-move usePp calls. Actual MovePhase does not spend unlimited PP;
the API producer follows that finite-PP call boundary. PP modifiers use the
shared-state bridge but do not each have a new copying-specific source matrix.
Native form/fusion transitions, suppression and arena-tag removal, Commander,
wide levels/stat order and final-boss/interlude prerequisites remain separate.
Host proofs and 3DS CI are distinct from emulator/hardware validation.
