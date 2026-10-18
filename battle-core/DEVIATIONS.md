# Deliberate departures from the source pin

Each departure is a bit in `pr_battle.deviations` (`PR_DEV_*` in
`include/pr/battle.h`). The default 0 is the pin. The differential harnesses
never set a bit, so their results stay comparisons with
`8555c08c823b856cbec4eb99ca84ea52a955836d`. game-core sets the bits for real
runs (`gc_battle.c`). A departure is tested by its own spec tests and is never
counted as upstream parity.

| Flag | Asked by | Behaviour | Pin behaviour | Spec test |
| --- | --- | --- | --- | --- |
| `PR_DEV_ENEMY_ZERO_TO_HERO` | lorenzo, 2026-10-18 | An enemy Palafin that switches out with Zero to Hero takes its Hero form, like the player's | QuietFormChangePhase ends early for an off-field enemy, so the form never changes (the source's own TODO asks whether that is intended) | `make -C game-core enemy-zero-to-hero-host-test`: a trainer's Palafin uses U-turn and is Hero with the flag set, Zero with it cleared |
| `PR_DEV_ILLUSION` | lorenzo, 2026-10-18 | Illusion works, following the pin's commented-out draft. On entry, a Pokemon with a trainer (every player Pokemon too) is disguised as the last other party member still able to battle (not when either side is a Terastallized Ogerpon or Terapagos). A wild one is disguised as a random species of the biome, taken from game-core's `illusion_species` callback on a copy of the run's RNG, so the run's own rolls are unchanged. Fused holders get no disguise. A damaging hit ends it ("illusion wore off"), and so does the end of the battle. The 3DS view shows the disguise's species, form, gender, name and types | `.unimplemented()`: the draft's attributes are commented out and setIllusion is never called, so Illusion does nothing | `make -C game-core illusion-host-test` (29 checks): a real run's Zoroark enters wave 2 disguised as the last member and the foe's Tackle ends it; the disguise rules one by one; the flag cleared gives no disguise. `bash game/tests/run_host_tests.sh` checks the 3DS screen against the shown look |
