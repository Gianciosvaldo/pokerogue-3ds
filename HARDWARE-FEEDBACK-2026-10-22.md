# Hardware feedback follow-up

Base: merged PR372, 8e2a7a53a91a32c6d4d1987cda6c11a1dae399d1.
Upstream: 8555c08c823b856cbec4eb99ca84ea52a955836d.

Requested fixes and implementation status (validation in progress):

- Doubles command undo: B from the second ally's command menu restores the first ally's command prompt and pre-selection RNG/commands. Target-screen B still cancels just that target selection. Locked/automatic first commands have no undo offer.
- Names: battle and menu name lookup now share localized move keys; UI hides implementation suffixes and uses a readable fallback when a locale name is missing. TM item labels are rebuilt from live move labels instead of the stale generated English debug names. Battle-core's native text protocol is unchanged.
- Sacrifice: native SacrificialAttr explicitly ignores boss shields; apply the same option. Actual normal-user Self-Destruct controller check and 24 native HP/damageTaken callback comparisons cover Self-Destruct, Explosion and Misty Explosion, normal/segmented users and zero/positive HP. This bounded callback check does not claim every owning-turn interaction.
- PP: item target menus include existing PP Ups and positive max-PP overrides.
- Doubles visuals: drop secondary ally when returning to singles, move healthboxes outward into physical screen margins, draw borders before bars. Use the shared sprite atlas without loading another full copy into img's cache; absent slots do not load/animate. Hardware performance still needs a console retest.
- Candy cost reduction: two purchases at native balance prices; existing native value rule subtracts one above cost1 and halves costs at/below1. Profile save failure rolls back cost/debit.
- Pokerus: **requested custom rule**, three unique species every UTC three hours. The pin uses FIVE per UTC day. Both use the same timestamp-seeded isolated RNG and starter registry order; no run RNG is consumed. Rotation marks starter icons and reaches actual starter/run Pokerus state, whose EXP/spread mechanics already exist.
- Hatch: neutral background, IV radar using native axis coefficients/order and IV improvements, nature, newly unlocked egg move displayed before leaving the hatch scene. No delayed hatch/egg-move text over the next battle.
- Shop TM details: hold SELECT to replace the teaching instruction with the move effect; release restores it.
- Level-up stat window: bottom screen only.
- Free TM reward: actual native LearnMovePhase consumes the copied reward phase on success, preserves it on cancellation, records used TM only on success. Learn happens before its copied reward screen.
- Summary reading: menu description time is independent of battle speed.
- Nemesis: claimed records remain in the archive and disappear from the available radar list; after an offer, show the accepted/rejected wild foe that was deliberately hidden during the intro.
- Fury Cutter/Rollout/Echoed Voice: real run wave/save/reload checks confirm existing boosts persist while staying on field and reset on leaving. Fight now previews boosted power (including Ice Ball) without modifying gameplay state or RNG. Native callback histories/caps/failures and reset ownership agree on the bounded fixtures.

Evidence: game-core/tests/starter-feedback.producer.test.ts invokes actual native reduction cost/value methods for all572 starters and isolated RNG selection for64 three-hour seeds; at8 midnight seeds its three selections equal the first3 of actual getPokerusStarters. Both this producer and sacrifice-hp.producer.test.ts capture native Battle RNG before GameManager and restore it after natural battle start. Their committed fixtures are bounded comparisons, not full battle/run completion proofs.

Pending: finish focused tests, native consecutive-move checks, retained proofs, full core/app/custom/save suites, ARM/stack, exact-head CI and builder checks; then merge and deliver updated Windows builder. Only separate custom-only Task63 skipped. No unrelated lane.
