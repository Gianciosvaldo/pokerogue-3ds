# Move labels and live battle stages

Party, starter, summary and learn-move menus now resolve move labels from the
pinned English move locale using the existing enum-to-locale keys. The generated
battle-core Move.name field contains implementation annotations and, for Return,
a translation fallback: `Pursuit (P)` and `return.name`. The UI displays `Pursuit`
and `Return` without changing move mechanics or generated battle-core tables.
Missing locale results are copied into persistent rotating buffers because the
locale API may return the supplied key pointer.

Open Pokémon, select your active Pokémon's Summary, then open its Stats page.
Permanent stat values now have signed current battle stages in parentheses,
including zero; the row above them shows accuracy and evasion stages. These are
stages, not recalculated stat values. The existing IV toggle still shows IVs
without stage annotations. Inactive Pokémon and stored history entries retain
their ordinary summary. Both game-core-to-menu conversion paths copy the current
battle stages, including Baton Pass's inherited stages.

Reference: upstream 8555c08c823b856cbec4eb99ca84ea52a955836d and locales
270ed0a2938b2c3e2e34e7de940e84aecadf548b. Existing summary rendering regression
covers positive/negative stages, accuracy/evasion, and hiding stages outside the
live battle summary; its locale fixture covers Return and Pursuit. Normal app,
custom and save tests and the real 3DS build remain merge requirements.

Egg hatch completion, IV Scanner prompts, trainer/rival artwork and final-blow
run history are separate follow-ups. This change does not claim those are fixed.
