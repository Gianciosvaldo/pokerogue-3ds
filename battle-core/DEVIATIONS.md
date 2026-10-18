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
