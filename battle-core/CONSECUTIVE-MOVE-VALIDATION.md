# Consecutive move selection proof

Pinned source `8555c08c823b856cbec4eb99ca84ea52a955836d`, English locales
`270ed0a2938b2c3e2e34e7de940e84aecadf548b`, Node 22, GCC 14.2.

Gigaton Hammer and Blood Moon already have the correct C restriction. This task
adds source-produced validation without changing the runtime or established
fixtures. The source MoveRestriction at move-condition.ts:292 reads the newest
getLastXMoves(1) entry, including failed and virtual entries. It does not use
getLastNonVirtualMove. Pokemon.trySelectMove (3255), PokemonMove.isUsable,
Pokemon.isMoveSelectable, and the real CommandPhase.handleFightCommand (241)
provide the expected decisions and texts. The command caller handles Struggle
when every slot is unavailable; the selection-only restriction is not applied
by PokemonMove.isUsable when forSelection is false.

The fixture uses real initialized player Pokemon and source move instances.
It invokes the real command handler, observes its queued command or error text,
and checks that history, PP, global RNG and battle RNG do not change. UI drawing
is stubbed; the selection/command logic and restriction methods are unchanged.

Four decisions and their full English texts are compared per case: selectable,
trySelectMove, execution usability, and the fight command/Struggle result.
672 cases cover both moves, 14 histories, six use modes, and four PP/slot controls.
Histories include empty, same/different last moves, older matches, failed moves,
virtual moves, NONE/Struggle markers, the other restricted move, and 33-entry
histories that exceed the C retained-history window. The owning C reader rejects
malformed/partial input and also requires unchanged C Pokemon/battle state.

```sh
source /workspace/shared/activate-parity.sh
battle-core/tools/consecutive-move-diff.sh /path/to/pinned/upstream
TURNTEST_LOCALES=/path/to/pinned/upstream/locales/en \
  python3 battle-core/tools/consecutive-move-mutations.py battle-core/build/consecutive-move.txt
TURNTEST_LOCALES=/path/to/pinned/upstream/locales/en \
  python3 battle-core/tools/consecutive-move-malformed.py \
    battle-core/build/consecutive-move-test battle-core/build/consecutive-move.txt
```

Results: all 672 cases / 2,688 decision-and-text comparisons pass. Eight actual
production mutants fail after an intact return-0 control: allowing repetition,
over-restricting histories, filtering failed entries, ignoring Blood Moon,
reading the previous entry, filtering virtual entries, enforcing the restriction
during execution, and removing Struggle fallback. Eight malformed inputs return
2 after the intact control passes: incomplete footer, wrong version, fractional
count, case order, missing case, trailing case, wrong footer, invalid text encoding.

This proof covers singles player command selection with both target moves and
normal/ignore-PP commands. It does not claim new Torment/Encore behavior,
challenge restrictions, enemy AI, graphical UI/emulator input, all command types,
or complete move invocation/turn parity. Source initialization randomness is not
part of the expected outcomes; all selection inputs are controlled explicitly.
Host builds/tests and 3DS CI results are recorded on the task PR.
