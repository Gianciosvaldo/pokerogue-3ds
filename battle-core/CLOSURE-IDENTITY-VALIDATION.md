# Closure audit and captured constructor arguments

`tools/closures.py` omitted captured numeric `vals` when hashing closures,
although `gen/gen_tables.py` includes them. The constructor closures for
Rollout, Ice Ball, Fury Cutter and Echoed Voice therefore appeared to have
different identities in the helper. `fill` could write the wrong registry hash,
and `status` could report existing mappings as absent. The production generator
and the existing C registrations already used the correct hashes.

Both tools now use the same pure identity function: source text, optional
captured `cap`, then optional captured `vals`, with the existing serialization
and ordering. `show` displays both captured fields. The helper accepts
`--dump-dir DIR` for an existing native dump. Its status count includes the
actual wildcard registrations and constant-true builtin, and explicitly refers
to generic registry mappings. Direct class-specific dispatch and semantic
coverage require separate review.

The retained native capsule contains the complete original data-dumper output
at upstream `8555c08c823b856cbec4eb99ca84ea52a955836d`, with pinned locales/assets
and the producer hash. This is registry initialization data, not a battle/RNG
execution proof. The tests:

- Match all 735 native closure identities and owner positions against the
  committed generator manifest, including 52 `cap` records and four `vals`
  records.
- Exercise actual `show` and `fill` commands, including filling every native
  owner position. Changing the captured numeric limit changes its identity.
- Compare helper accounting with `pr_closures_ported()` in the actual C
  library. At this pin, 617 closures map through the generic registry/builtin.
  The other 118 are audit candidates, not 118 missing mechanics; many have
  direct message, restriction, form or moveset-generator owners.
- Run the actual generator over the retained native dump in temporary output
  storage and compare all five output files byte for byte: `pr_data.c`,
  `pr_data.h`, `pr_attrs.h`, `pr_enums.h` and `closures.txt`.

Normal builds continue using the committed tables without requiring an
untracked upstream dump. When that dump is present, changes to it, the
generator or shared hash function trigger regeneration. Explicit `make gen`
still requires the native dump. Optional `PR_BATTLE_DUMP_DIR` and
`PR_BATTLE_GENERATED_DIR` permit isolated regeneration; defaults are unchanged.

```sh
python3 battle-core/tests/closure_identity_test.py
# Node 22 and the prepared clean pinned checkout:
bash battle-core/tools/reproduce-closure-identity.sh /path/to/upstream /path/to/output
python3 battle-core/tools/closures.py --dump-dir /path/to/output/native status
```

The fresh native capsule must match after decompression. No generated battle
table or gameplay behavior changes. This prevents a stale audit tool from
recreating already implemented work; it does not finish the enabled-source
inventory, prove every closure input/interaction, or establish battle-core
completion. Normal core/app/custom/save and ARM checks remain required. No new
Windows builder is released.
