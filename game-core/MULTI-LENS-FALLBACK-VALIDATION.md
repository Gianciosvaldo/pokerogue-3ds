# Multi Lens acquisition fallback and old split-stock saves

Source pin: `8555c08c823b856cbec4eb99ca84ea52a955836d`.
Normal native Multi Lens stacks cap at two. The actual native .add owner combines
matching held modifiers, independently of game-core's constructor-only -1/0 arg.
Native damage factors with two stacked lenses are 0.5, 0.25, 0.25.

Reproduced cause: after canonical acquisition, a vitamin/Soul Dew/fusion makes
the whole-Scene adapter return UNHANDLED. Generic fallback then compares arg,
creating a second independent lens record. The retained native damage formula
applies each modifier separately; an extra strike can recover full damage.
Do not change that formula. Use the existing canonical .add adapter in fallback
when its input is supported, including refusal at the two-stack cap. Keep the
whole-Scene coverage guard and GC_STUB_REWARD_APPLY diagnostic: this does not
prove every co-item/stat/Scene side effect or unsupported imported inventory.

An actual pre-repair C save contains a vitamin and one lens at arg0 plus another
at arg-1. Before repair its three observed factors are .5625, .0625, 1. A load
combines that exact alias split and caps older over-stacked three/four-lens
inventories to native's two-lens maximum, preserving the first stock and other
items' order. Repair requires real holders, recognized constructor args,
positive stacks and compatible transfer/class state. Opaque, detached, transient
or differently locked groups are retained for their unsupported diagnostics.
An optional multi_lens_storage section marks current saves, preserving deliberately
imported native duplicate controls on current save round trips. The tagged-field
format number and existing field schemas do not change.

Evidence:

- `tests/multi-fallback.producer.test.ts`: pinned actual PokemonMultiHitModifier
  .add and apply helpers, 192 records. Six vitamin stats, Soul Dew or fusion;
  initial stocks0/1/2; both C arg aliases and two storage mappings. Captures
  stock list, refusal, hit count and every damage multiplier. Native battle RNG
  captured before GameManager and restored after battle startup.
- Fresh Node22 capture matches the committed JSON and C observations exactly.
  `tools/check_multi_fallback.py` validates the complete record domain and
  compares every observation. Ten malformed/forged captures are rejected.
- Actual gc_port_apply_reward path, actual battle modifier bridge and actual
  save/load: 192 native records plus retained acquisition/atomic guards; 3671
  host checks pass. Every added record checks post-load strike factors too.
- `tests/fixtures/multi-fallback-legacy.save` was written before the optional
  marker/migration. Its writer is retained as `.source.txt`; setup and serializer
  are from presentation head 2fc64fe4447b2695a846cff8d4a9761ed15b5aa0.
  Compile the writer in work/multi-fallback against that head's libraries to
  reproduce it. Additional current-save marker removal controls exercise six repair shapes (including three and four lenses) and eight
  nonmatching/lock cases and preserve deliberate current duplicates.
- Six compiled faults are caught: generic fallback, missing repair, repair of
  current saves, repair of opaque groups, missing over-cap clamping, full damage on later lens hits.
  Capture and compiled-fault checks are wired into normal game-core tests.
- Scoped ASan/UBSan/LSan instruments acquisition fallback, canonical bridge,
  item projection and save owners, linking retained supporting core archives.

Native capture reproduction (Node22, verified source pin, tracked upstream
sources and test framework clean): copy the producer additively to upstream's
`test/tests/zz-dump/`, run Vitest with MULTI_FALLBACK_OUT pointing to a JSON path,
then run `python3 game-core/tools/check_multi_fallback.py <capture>` from this
repository. Do not regenerate expected factors from the C implementation.

This is an API-owner proof, not full battle/Scene parity, general save repair,
or battle-core completion. Normal app/core/custom/save/football and actual
3DS/ARM stack CI remain required; skip only dedicated Task63. No builder.

Final local integration: 10,000 ordinary app runs and 100 strong custom
save/resume runs pass with zero model violations. The final scoped sanitizer
run passes all 3671 host checks without address/undefined/leak failures.
