# TM selection in the reward shop

The app opened every targeted item as `PARTY_MODIFIER`, so TM choices displayed
Apply and HP bars, with no compatibility labels or selection filter. It also
prevented move-targeted items from opening their move list. The core's TM adapter
accepted incompatible and already-known moves, allowing an ineffective selection.

TM offers now open `PARTY_TM_MODIFIER`, show Teach and the localized Able / Not
able / Learned labels, and reject targets that cannot learn that TM. The core
checks the same filter before recording a TM or queuing any learning phases.
Other move-targeted offers open `PARTY_MOVE_MODIFIER` with their move list.

B closes a Pokémon's options; B again returns to the reward shop. The party
Cancel button also returns to the shop. Neither path submits or consumes an
item. After learning or declining a TM, upstream intentionally returns to the
reward shop; Continue advances from there.

## Pinned source and evidence

Checked directly at upstream `8555c08c823b856cbec4eb99ca84ea52a955836d`:

- `PlayerPokemon.getCompatibleTms` / `isTmCompatible`, `src/field/pokemon.ts`:
  current species/form TMs plus fusion TMs. The teaching filter excludes known
  moves, without excluding level-up moves or used-TM history. Fainting does not
  change this compatibility.
- `TmModifierType`, `src/modifier/modifier-type.ts`: `isTmCompatible(moveId,true)`
  rejects a target with `PartyUiHandler.NoEffectMessage`.
- `PartySlot`, `src/ui/handlers/party-ui-handler.ts`: known moves take label
  priority over compatibility; otherwise Able or Not able replaces the HP bar.
- `SelectModifierPhase.openPartyScreen`, `src/phases/select-modifier-phase.ts`:
  TM mode, TM move ID, select filter, and cancellation back to modifier select.

`tools/tm-compat-diff.sh` executes the actual native compatibility methods and
97,500 actual `TmModifierType.selectFilter` checks. All **1,084 species** and
**1,500 valid forms** are covered with three profiles per form: unfused, known
moves, and a deterministic fusion with known moves and zero HP. Used-TM history
is populated independently. The strict C reader checks **4,500 profiles** and
**4,144,500 move states**, all exact (339,036 Able; 3,793,464 Not able; 12,000
Learned). It requires the complete ordered species/form/profile inventory and
footer, and refuses trailing input. Three compiled compatibility faults are caught
and ten malformed captures are rejected by `check-tm-compat-proof.py`.

This is a Classic-mode compatibility proof, not a complete reward-phase or
challenge proof. The Moveset Randomizer's `PLAYER_TM_COMPATIBILITY` hook remains
unported, as already reported by reward generation.

`game/tests/party_item_test.c` runs the actual `party_screen.c` and `party.c`:
all three rendered labels, Teach, incompatible/already-known refusal, B and
touch cancellation, eligible selection, and clearing TM labels/filter when a
move-targeted item opens. Rendering, the UI stack and unrelated summary dialog
are host fakes. The normal core reward test checks rejection without queued
phases/history mutation, all three TM tiers, known-move priority and fainted
fusion acceptance. Normal app/custom/save regressions remain required.

```sh
. /workspace/shared/activate-parity.sh  # Node 22 in the development workspace
./game-core/tools/tm-compat-diff.sh /path/to/pinned/upstream
make -C game/tests party_item_test
game/tests/build/party_item_test
```

The refreshed Windows builder is built from current main plus these fixes. It
therefore includes the UI changes merged after #337, rather than repackaging
that old game binary. Re-extract the new ZIP and rerun `BUILD-SD-CARD.bat` to
replace the bundled game on the SD card.
