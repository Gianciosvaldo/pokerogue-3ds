# Held form-item acquisition and party action owner

Pinned upstream: `8555c08c823b856cbec4eb99ca84ea52a955836d`; locales:
`270ed0a2938b2c3e2e34e7de940e84aecadf548b`; assets gitlink:
`87426a79611f9d212c4dc8af58e2834a05b93725`.

The [callback selector](../battle-core/HELD-FORM-VALIDATION.md) and
[inventory/save projection](HELD-FORM-STATE-VALIDATION.md) now have a game-core
owner. Normal and rare form rewards install the item before testing activation.
A failed activation retains the item and accepts the reward. A duplicate across
either factory keeps the original item and substitutes the player pool's default
ball reward at the incoming tier, with the native stack-full message.

`gc_form_item_options` exposes global inventory indices in native order, including
bench ownership and the active Ultra Crystal/Necrozma selection rules.
`gc_form_item_toggle` accepts an offered index only while reward selection waits.
It flips activation and executes the selected form's state owner without consuming
the reward or changing its waiting queue. Missing required caught-profile facts
refuse before changing inventory. Invalid ownership, unsupported stock shapes and
unoffered indices cannot enter this path.

The shared form owner now emits native Mega/Gigantamax/Eternamax messages. It
updates ability metadata, seen/caught records, stats/HP and Terastallization,
removes Autotomized and emits the existing form-animation event. Moves are kept.
Fusion evolution remains a separate, counted gap.

## Independent native observations

Both producers capture the native battle RNG method before constructing
GameManager and restore it after natural double-battle startup. The actual third
party member is on the bench; it is not an off-field flag substituted for party
ownership. Expected behavior comes from native execution, never production C
tables or a second implementation of the mechanic.

- **66,560 acquisitions:** all 416 item-trigger registrations, lead/bench, normal
  and rare factories, four ability/caught-profile gates, fresh/duplicate stock,
  and five offer tiers. Actual `BattleScene.addModifier` observations include
  inventory, return value, complete queued phases, ball counts and fallback text.
  The C replay calls the real reward owner. Native `false` for no activation is
  explicitly normalized to an accepted reward with the newly retained stock.
  This trace stops before form-phase execution.
- **122,112 party lists:** all 159 sparse item identities, Mew/Necrozma, 16 active
  patterns, three party positions, player/enemy inventories, matching/foreign
  holders and both inventory orders. Actual `getFormChangeItemsModifiers` lists
  are compared exactly, including all partial output-buffer capacities.
- **74,880 form actions:** the original 832 cases (416 registrations × lead/bench)
  remain first. Another 62,400 cover levels 5/1,000/65,536 and all 25 natures;
  9,984 cover raw ability indices, Terastallization and fainted/one/full HP;
  1,664 add Mew and Heat Rotom fusion halves to every registration/owner pair.
  These invoke the actual private party action, modal phase dispatch,
  `onBeforeSpriteSetup`, `PlayerPokemon.changeForm`, profile setters and phase
  end/standby restoration. C compares recorded state, every observed dex/starter
  entry, formatted English text and return to the unchanged reward selection.

Rendering, asset loading, widget refresh and animation timing are omitted from
the native phase harness. Mechanical animation completion calls the real source
methods. The source private handler returns `false` even after mutating activation;
that UI return value is not treated as a rejected core command. Nature predicates
can prevent a form change even after the item flips; these observations are kept.

The initial replay passes 17,594,577 menu, 11,607,097 acquisition and 30,575,237
form-action checks. All three seeded C RNG streams remain unchanged. Native global
and battle RNG streams remain unchanged. Thirty-nine additional C guard checks
cover invalid commands, ownership, stock and profile availability.

## Retention and reproduction

```sh
make -C game-core form-item-owner-host-test
# Node 22, prepared pinned upstream checkout:
game-core/tools/form-item-owner-diff.sh /path/to/pinned/upstream
```

Complete transports are losslessly compressed under
`tests/fixtures/form-item-owner`, with SHA-256 checks for decompressed data and
the four unchanged pinned English locale namespaces used to compare messages.
Strict readers require the full geometry and reject truncation, duplicate cases,
bad records, extra data and out-of-range values. Fifteen compiled controls break
bench/holder/active filtering, Ultra priority, toggling, modal return, reward
classification, tier fallback, installation/dispatch, messages, tag removal and
profile updates; all must fail. Eleven malformed transports must also fail.
These controls run in the normal game-core suite.

This closes the measured acquisition, list and mechanical phase owner. It does
not claim app menu navigation, graphical timing, arbitrary fusion combinations,
Transform interactions, quiet/enemy phases, removal histories or full battle-core
completion. The app's Check Team/form-toggle wiring is a follow-up. Normal
app/custom/save, core/football and actual ARM build/stack checks remain required;
only the dedicated custom Task63 check is skipped.
