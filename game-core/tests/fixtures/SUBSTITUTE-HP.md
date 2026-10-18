# Historical Substitute HP saves

`substitute-hp-int32-saves.bin` contains eight actual `gc_run_save` buffers from
commit `720aaab6b6dd58f5804e5e34b76c2aaee35884c8`, before tag HP widened.
SHA256: `87d0d0332d797512ffa9e069eb8e306498b33a41fbd977ada564446d2fcb219b`.
The container is a native little-endian uint32 count followed by uint32 length
and bytes for each save. These fixtures were produced on Linux x86_64.

The retained `tests/wide_combat_legacy_writer.c` must be compiled against that
old commit's headers and game-core/run-core/AI/reward/battle libraries. It
refuses to run if tag HP is no longer four bytes. Build `game-core/build/libgc.a`
and its ordinary dependent libraries in an old checkout, then compile the
writer with that checkout's game-core include/src, run-core/include,
ai-core/include, reward-core/include/gen/out, battle-core/include/gen/out,
and link libgc, reward, AI, run and battle libraries followed by `-lm`.
Pass the output file path as the writer's only argument.

The eight signed HP values are INT32_MIN, -1, 0, 1, 128, 255, 65535 and
INT32_MAX. A neighboring Aqua Ring tag and source metadata detect record
misalignment. Negative HP here tests transport boundaries, not legal saved
command states. The new loader and writer are exercised by the ordinary
`make -C game-core wide-combat-save-host-test` target.
