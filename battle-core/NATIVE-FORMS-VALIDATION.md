# Native ability form callbacks

Source pin: `8555c08c823b856cbec4eb99ca84ea52a955836d`.
Locales pin: `270ed0a2938b2c3e2e34e7de940e84aecadf548b`.

The scene trigger now queues the registered native transition for Darmanitan,
Galar Darmanitan, Minior, Wishiwashi, Morpeko, Eiscue, Mimikyu and Zygarde.
Attribute predicates decide whether to call it. The trigger matches the current
raw form to its registered destination; it does not recompute the HP predicate.
This matters when battle initialization reverts a low-HP Pokémon. Mutation,
stats, ability-index clamping, gain/loss callbacks, text and Autotomize removal
run through QuietFormChangePhase rather than the older immediate mutation.

Power Construct's four actual closure functions and Ice Face's captured weather
condition are now linked. The natural Zygarde matrix also exposed Aura Break's
missing summon-message condition. Its field predicate now checks for either
Aura, and simulated summon messages no longer require a phase engine.

## Actual source checks

The producer calls actual applyAbAttrs/applyOnGainAbAttrs and records state plus
queued phase order, target forms and passive flags. The optional phase mode then
executes the queued actual QuietFormChangePhase objects in that order. It leaves
ability-bar/message presentation queued; this is an API/quiet-phase boundary,
not a complete turn. Asset loading, sprite updates and UI callbacks are transport
stubs. Gameplay predicates and formulas are unchanged.

- 8,192 API cases and 8,192 queued-quiet cases, seeds 777/2034: exact, with no
  unsupported paths. Both sides, 32 states and all eight species are required.
- 8,192 natural-ability cases, seed 4242: exact, including absent native abilities
  and HP at zero, one, around one-quarter/one-half, and maximum.
- 8,192 additional natural/Aura cases, seed 31337: exact, including live/dead
  Dark Aura/Fairy Aura field actors, simulation and queued message text.
- 8,000 complete singles turns, seeds 777/9001/4242/31337: every turn is covered
  and exact in state, phases, text and both RNG streams. Actual end-turn dispatch
  reaches native changes; per-species guards require all six species with these
  turn callbacks to change. Ice Face and Disguise are turn negative controls.

The first full-turn smoke accidentally retained the scaffold's Ball Fetch
override and executed no native changes. It is excluded from validation. The
producer now clears ability overrides, selects existing effective moves and
asserts the native ability before running. The corrected 120-turn smoke runs
45 quiet changes and matches exactly; the 8,000-turn corpus uses that correction.

Ten compiled faults are caught, covering scene routing, Schooling, Minior,
Zygarde/Power Construct, HP/level conditions, Ice Face weather and Aura Break
condition/message behavior. Twenty malformed complete fixtures are rejected.

Reproduce with Node 22 and installed pinned upstream dependencies:

```sh
tools/native-form-diff.sh /path/to/upstream 4096 777 api
tools/native-form-diff.sh /path/to/upstream 4096 777 phase
tools/native-form-diff.sh /path/to/upstream 8192 4242 natural
tools/native-form-diff.sh /path/to/upstream 8192 31337 aura
tools/native-form-diff.sh /path/to/upstream 2000 777 turn
TURNTEST_LOCALES=/path/to/upstream/locales/en python3 tools/check-native-form-proof.py \
  build/native-form-api-777.txt build/native-form-phase-777.txt \
  build/native-form-natural-4242.txt build/native-form-aura-31337.txt
```

The API traces assert unchanged source RNG; they do not independently compare
C RNG. The complete-turn comparison checks both streams. Retained 24,000 sampled
turns have no covered mismatch or coverage loss: rich seed777 increases from
1,951 to 1,955 and summon from 1,893 to 1,895. The retained 72,000 dedicated ability/
Beat Up cases and 16,000 Ditto cases also pass.

## Save/controller integration and limits

The 216-check game-core test drives actual fights, a real knockout, reward
continuation to legal save points and load. For all eight species it checks
form/ability/index/stats/HP preservation and the actual new-biome cleanup
controller. The recall snapshot contains the initialized form before summon
data is cleared. A subsequent summon may reapply a low-HP or high-level form;
the test does not require it to stay reverted. No save layout is changed.

Full game-core tests pass. Both 1,000-run paths finish; the paired 200-run
save/no-save digest is `bf86e547` on both paths. It changes from the earlier
baseline because native form behavior now runs. This is consistency evidence,
not upstream run-generation parity.

The full app suite passes, including ordinary saves, custom runs and custom
resume. Its unchanged 10,000-run soak completes 119,695 waves and 489,267 command
screens, with 46 completed/59 cancelled evolutions and 16 checked evolution
moves. There are no stalls, screen/model violations or save failures.

This closes these unfused native callback/quiet transitions. Stance Change's
older caller, other form species/item eligibility, fusion and Transform/native
transitions, suppression/arena-tag removal, Commander/full doubles, wide levels/
stat order and final-boss/interlude behavior remain separate. The turn matrix
uses existing moves and custom ability overrides to isolate native dispatch;
the separate natural matrix tests ability indices. Source proofs, host/app
integration, 3DS CI and emulator/hardware checks are distinct gates.
