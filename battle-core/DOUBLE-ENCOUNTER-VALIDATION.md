# Single/double encounter queue continuation

Reference: upstream `8555c08c823b856cbec4eb99ca84ea52a955836d`, Node22.23.3.

`pr_start_battle` now uses the native trainer prelude and EncounterPhase.end
player-entry queues. Fresh doubles with two available players position and
summon the second member; returning to singles recalls an existing second
member. Wild/mystery switch prompts use the correct minimum party size, queue
both field slots in doubles and retain the constructor's named-prompt argument.
Trainer doubles queue a second enemy summon only when more than one enemy is
alive. Enemy shiny sparkles cover both enemy slots. The separate queue helper
honors the loaded-session boundary by omitting fresh player entry and prompts.

## Actual source comparison

`tests/encounter-end.producer.test.ts` observes real EncounterPhase.end and,
for the second capture, the real trainer doEncounterCommon method through its
summon callback. A native Youngster supplies the trainer and actual dialogue
selection; visual/audio/dialogue completion is intercepted without replacing
source gameplay or RNG. The native BattleScene RNG method is captured before
GameManager's maximum-roll stub and restored after startup.

The version2 producer records every static queue level, each marker's dynamic
type, all nonempty dynamic queues and full relevant constructor arguments:
battler, field/party member, player/enemy, double-position and named-prompt
flags. Original version1 captures omitted dynamic payloads and are historical
only; they are not the completion evidence. Transport code computes no expected
queues. The replay compares static and dynamic payloads, marker types and both
unchanged RNG streams. PartyMemberPokemonPhase's derived fieldIndex is checked
separately from its constructor's partyMemberIndex, including slot1 returns in
singles. Trainer dialogue text/presentation is not claimed as C parity.

- Actual end queue: 8,192 complete cases at seed777, 4,096 doubles and 4,096
  loaded, zero mismatches; Classic/Endless/Spliced Endless/Daily/Challenge,
  wild/trainer/mystery battle types, one to three available party members,
  fainted/switching controls, shiny enemies and multiple wave boundaries.
- Actual trainer common method through end queue: 8,192 complete cases at
  seed2034, 4,096 doubles and 4,096 loaded, zero mismatches; one/two enemy
  parties and a fainted second enemy control.
- Nine compiled runtime mutants caught; fourteen malformed captures rejected.
- Full game-core test suite passes, including existing encounter/command/save
  suites. Paired200-run saved/unsaved digest remains `d42eb77a`.
- Retained24k mixed source turns and exact-head host/app/3DS CI are merge gates;
  final results are recorded in the pull request.

## Remaining requirements

This is a queue-construction boundary, not full doubles completion. Summon
execution still records its doubles limitation until the actual owning phase
is compared. CheckSwitch's missing-field/Commander contexts, double command
selection/targeting, natural double run generation, game-core/app presentation
and double save/recovery still require integration work. The app still converts
ordinary double rolls to singles and this patch does not remove that accounting.

The captured enemy species is Magikarp with no inventory. Encounter-end
Eternatus Mini Black Hole locking and IV Scanner phases are not established by
these captures. Broad challenge eligibility rules and mystery encounters are
not completed merely by exercising their mode/battle-type flags. Endless still
needs wide-level/stat/XP/save representation and its remaining battle lifecycle.
No run-core edits, emulator or hardware acceptance are included.
