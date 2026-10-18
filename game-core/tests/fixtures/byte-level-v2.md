`byte-level-v2.hex` is the untouched hex encoding of a native save captured
before widening, using merge `815ceeee14f073c87c7fecbb1914f6eed968095a`.
Decoded length 8,582, SHA256:
`a50e354c5cfb4ab44860726c344b9cd44c00fbca837d4bbe023f44c8087864be`.

`export-byte-level-save.c` records its deterministic setup. Compile that source
with the historical game-core and matching battle-core headers/libraries; its
width guard deliberately rejects the current ABI. No binary patching, schema
rewriting or expected-stat generation was used. This is an augmented first-
command save with two assigned party levels (255, 127), not a natural high-level
run. The owning host test also checks the enemy's historical level/metLevel,
zero dropped fields, and a subsequent new-format save/load.
