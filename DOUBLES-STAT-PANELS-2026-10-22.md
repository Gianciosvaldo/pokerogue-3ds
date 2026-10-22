# Playable doubles and upstream stat panels

Source pin: `8555c08c823b856cbec4eb99ca84ea52a955836d`.
Assets: `87426a79611f9d212c4dc8af58e2834a05b93725`.
Locales: `270ed0a2938b2c3e2e34e7de940e84aecadf548b`.

Lure/native encounter width now reaches the real run controller and app. Wild doubles construct both enemies in order, apply Synchronize before constructing the next enemy, distribute boss shields by BST, and generate held items with the actual width. Each living ally chooses a command; single-target moves offer the native legal targets, spread moves retain all their targets. B cancels target selection. Both enemies choose commands. Trainer pairs retain each trainer's reserve ownership; an ordinary double battle does not make a single trainer into a pair. Balls require one active wild opponent and target that remaining opponent. Run/ball commands suppress the other ally's command.

All four sprites, bars, move animations and stat stages are represented. Ally promotion, fainting in a separate event drain, wild fleeing, release of either active party member for a catch, and Illusion starting/ending between waves retain their identities. Doubles save at the initial command boundary; a partial turn cannot be saved. A separate optional encounter-width section keeps older single-battle saves readable.

Hold SELECT to show the actual upstream BattleInfo stat layout over each active health box; release to hide. The panels use upstream's assets, two-row label order, signed stage frames and 125 ms fade. Release is delivered through overlays so the panels do not remain latched after opening another screen.

## Evidence and limits

`game/tests/battle-stat-panels.producer.test.ts` calls the actual pinned PlayerBattleInfo/EnemyBattleInfo `constructStatContainer` and `updateStats` factories with graphics-only stand-ins whose dimensions come from pinned assets. The committed comparison covers player, enemy and boss layouts and all seven signed stages. The C renderer comparison exercises both player positions and both enemy positions and hold/release while events are playing.

`game-core/tests/doubles-encounter.producer.test.ts` runs the actual native EncounterPhase through both constructors, boss shield distribution and the item-generation boundary. It captures/restores the native battle RNG before/after GameManager construction/start. Its 120 cases compare 240 opponents, their constructor fields, IVs, stats, moves and exact stream state. **The C comparison injects native moveset results and stream states at the existing bounded moveset adapter. This verifies pair sequencing and other constructor ownership; it does not establish parity for the broader unported moveset pools.** Item rolling and Synchronize also retain their separate existing proofs; the new pair capture uses empty stock and no Synchronize.

The focused controller test exercises forced Lure doubles, two commands, target selection/cancel/refusal, initial save/reload and active-ally release. Normal app/custom/save suites retain their coverage floors. Existing theft, Grip Claw, Baton and Mini Black Hole captures are replayed unchanged. Only the separately requested custom-only Task63 check is skipped.

This change makes doubles playable; it does not claim completion of all battle-core or run-core functionality. Existing counted fallbacks for unsupported encounter generation, wider moveset pools and other unported mechanics remain.

The key/foe item bar remains visible during messages and between encounters and refreshes its live stock outside command prompts. A dedicated renderer check covers idle/events/command/fight states. Starter candy menus hide the hidden-ability entry for species with no hidden ability; the existing backend rejection still prevents candy spending through a stale request. The real menu test covers Mew and the existing all-starter guard test remains enabled.

Initial-command saves now retain each active Illusion identity, validated on load; previously a reload dropped the disguise and could add AI Illusion fallback counts. The paired 200-run normal/reload comparison agrees exactly (`7752019f`). Fainted/off-field party slots retain their native field identity on restore. A focused regression covers both shapes. The custom strength fixture heals living members without silently reviving an off-field member without a revival/summon phase; all scenario and coverage floors remain unchanged.
