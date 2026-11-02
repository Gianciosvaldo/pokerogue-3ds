# Endless learning, evolution and final-wave integration

This extends the [wide journey regression](ENDLESS-JOURNEY-VALIDATION.md) without
removing its 16 contexts, assertions, diagnostic refusals or losing seeds. The
new runner executes that retained runner first, then all eight fixed seeds in
both modes for each of two added families: **48 total contexts**.

- At wave 25, prepare Ralts level 19, one EXP below level 20. Generate actual
  opponents and play the battle, including double encounters. Execute the real
  EXP, evolution and move-learning phases: evolve Ralts to Kirlia, accept Psybeam
  into slot 3, pass the natural reward screen and save at the next generated
  encounter. Compare saved/unsaved progression-event metadata and states after
  every choice, then every serialized save byte. Preserve losses, including a
  case that evolves and learns before losing the remaining double battle.
- At wave 5,849, prepare Mewtwo at level 1,000,000. Play the actual generated
  encounters through wave 5,850, save/load again before the final battle, and
  require Clear at exactly 5,850. Refuse a terminal session save and leftover
  new-battle/reward requests. Never overwrite enemy HP, stats, moves or items.

The prepared player/wave states are synthetic API contexts, not proof of a
naturally accumulated run, legal starter moves or a practical stock win strategy.
The deliberately high-level terminal player keeps the final battle bounded;
ordinary/spliced enemy construction, fusion, boss segments and battle damage
still execute normally. Natural token accumulation across thousands of waves
and browser/hardware presentation remain separate obligations.

**9,075 combined assertions pass.** The original 16-case matrix retains its
4/1 wins and 4/7 losses. The learning family produces 7/5 evolutions and learned
Psybeams, 6/5 next-encounter wins and 2/3 losses. The terminal family produces
8/8 final-wave clears. No unsupported, deferred or game fallback path is accepted.
Both RNG streams, party/enemy identity, forms/fusion/ability, levels/EXP, HP/status,
stats, moves/PP, screens and money compare after each command; save buffers must
match byte-for-byte at each tested permitted boundary.

This is joined C integration evidence, **not native full-run parity**. Source
component proofs remain pinned to `8555c08c823b856cbec4eb99ca84ea52a955836d`:
[terminal phase](ENDLESS-TERMINAL-VALIDATION.md),
[wide constructor](CONSTRUCTOR-FINISH-VALIDATION.md),
[all stock wave formulas](../run-core/ENDLESS-WAVE-SWEEP-VALIDATION.md), and the
retained level-up/evolution and fusion learning owner comparisons. The current
source registry/unsupported-path ledger and final combined acceptance remain
open. The samples alone cannot certify battle-core completion.

The new runner also passes scoped UBSan (runner instrumented; retained linked
archives unchanged). Run `make -C game-core endless-lifecycle-host-test`. Normal core/app/custom/save/
football and actual 3DS/ARM stack checks remain merge gates. Only dedicated Task63
is excluded. This change adds no builder and changes no production behavior.
