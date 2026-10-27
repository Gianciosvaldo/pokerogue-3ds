# Complete TM learning history

Pinned upstream: `8555c08c823b856cbec4eb99ca84ea52a955836d`.

The real `LearnMovePhase.learnMove` callback can record 312 different implemented
TMs on Mew. Game-core silently stopped after 64. Reward projection repeated that
limit, and byte-sized counters also could not represent histories above 255.

The native species/form catalog and generated `pr_tm_tier` table contain exactly
322 registered TM IDs. The shared capacity now follows this complete catalog;
the callback records each move once. Both core histories and their reward
projection use 16-bit counts. Memory Mushroom queries retain the complete
ordered list. Invalid histories beyond the catalog budget are reported rather
than silently clamped.

The native producer executes the actual learning callback for all 312
implemented compatible TMs, followed by a repeated first TM. It records every
before/after history, moves/PP, ordered `getLearnableLevelMoves` result and RNG.
Native battle RNG is captured before GameManager and restored after startup;
the learning method's identity is checked. Animation, sound and text display are
presentation boundaries; observation stops before text acknowledgement. The
raw catalog includes the source's existing species/form fallback observations.
No native result is normalized away.

The host compares all 313 callbacks, every ordered history entry, reward
projection and complete memory query, including the 64/65 and 255/256 boundaries.
It also compares catalog identities against generated C data. There are 314
command-save roundtrips, including the initial frame and each callback. Every
loaded frame matches the native history, moves, PP, memory ordering and RNG.

An authentic command save from merge `5c07dea`, written before changing the
64-entry layout, retains all 64 records with zero dropped fields. Its SHA-256 is
`13160c63bc64d6794f9f446da14e65a43e435b4e9aa852b948c70b9e14a17948`.
The loader accepts exactly the historical 128-byte-array/one-byte-count pair or
the new 644-byte-array/two-byte-count pair. It rejects mixed, partial, duplicated
and oversized schemas. Unrelated field-size accounting and historical format-1
behavior remain unchanged. The save format version is unchanged.

```sh
make -C game-core tm-history-host-test
python3 game-core/tools/check-tm-history-proof.py
TM_HISTORY_SANITIZE=1 python3 game-core/tools/check-tm-history-proof.py
```

The 517,788-check replay passes. Ten compiled faults, eleven malformed saves,
ten malformed native captures and four forged observations must fail. Save
mutations retain valid section sizes and checksums, so corruption detection is
tested beyond the outer checksum. Scoped ASan/UBSan/LSan covers learning,
projection, save handling and reward-core memory queries; supporting archives
are unsanitized. Raw native JSON and the C fixture reproduce byte-for-byte.

This closes the demonstrated TM-history truncation. It does not implement
Shedinja's extra-party creation or shared TM-array lifetime, nor establish full
battle-core completion or every frontend menu arrangement. Normal
core/app/custom/save/football tests and the ARM build remain required; only the
dedicated custom Task63 checks are skipped. No new Windows builder is released.
