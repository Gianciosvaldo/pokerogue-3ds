# Mystery encounter UI and reward audit

Source: upstream `8555c08c823b856cbec4eb99ca84ea52a955836d`, English locales
`270ed0a2938b2c3e2e34e7de940e84aecadf548b`. This audit separates the fourteen
source-backed encounter implementations from the older Task 46 stand-ins.
Delibirdy and the three-foe encounters (Mysterious Challengers/Pillar Men) are
left to the concurrent work the user identified.

## Source-backed encounters

| Encounter | Result of the audit |
| --- | --- |
| Department Store Sale | Retained native shop/reward/RNG comparison passes; description markup and paged details use the shared corrected UI. |
| Shady Vitamin Dealer | Announce both actual vitamins and the selected Pokemon. Cheap-side-effect text now also receives selectedPokemon. Keep the actual costs, rolls, IV stack caps and delayed nature/HP effects. |
| Lost at Sea | Retain the actual Surf/Fly requirements, primary owner, experience and damage; keep tooltipDisabled support. |
| Field Trip | Correctly send the original move slot when rows are omitted, and show the actual demonstration and correct/wrong-category explanation. Native replay confirms the existing five rewards; do not add a spurious sixth item. |
| Part Timer | Respect disabledTooltip (a different spelling from Lost at Sea), retaining the native payment, experience and PP rules. |
| An Offer You Can't Refuse | Supply the actual strongest Pokemon, price, Shiny Charm, Liepard and extortion move/ability to the dialogue. Retain the existing sale/removal/reward/experience logic. |
| The Pokemon Salesman | Show the actual purchasePokemon and price, use the shiny description/tooltip for a shiny offer, and deliver both acceptance messages. A preview is an enemy-shaped Pokemon: pass the battle to the name formatter instead of a null battle. |
| Fun and Games | Supply wobbuffetName in the question and intro; retain the minigame/reward logic. |
| Global Trade System | Ask which of all three offers to receive, or which transferable item to give. Back navigation commits nothing. All held-item pages remain reachable. Send the held-item ordinal the actual handler expects, not the inventory index or zero. Announce concrete generated item names. |
| Fight or Flight | Announce the concrete generated reward type, not a missing generic generator locale key; respect disabledTooltip. Retain boss, reward, theft and experience logic. |
| Slumbering Snorlax | Respect disabledTooltip for the unavailable steal; retain rest, theft, boss and reward logic. |
| Berries Abound | Retain the race outcome, owner, experience, berry callback and shop. Shared description/tooltip rendering is corrected. |
| Dark Deal | Announce the actual removed Pokemon in both selected-dialogue messages before the battle. Retain the removal, Rogue Balls and boss/item logic. |
| Training Session | Retain member/nature/ability choices and battle-return rewards. Render the description's markup rather than printing its control codes. |

The shared screen renders marked descriptions, pages descriptions and tooltips
with Left/Right, resets details when changing the highlighted option, and scales
art into the remaining space below the question. Long lists open scrolled to
the requested cursor. Token fallback copies into a bounded separate buffer:
expanding a short missing token cannot overwrite its unread input.
Generated item names use reward-core's pinned English concrete-type catalog;
plain modifier names retain their existing locale keys.

## Validation

- `game/tests/me_screen_test.c` drives the real screens and core with a fake renderer.
  Includes every source-backed encounter's actual locale excerpts, all three GTS
  offers, back navigation, a 29-item inventory across pages, applying the selected
  item trade and reaching its actual upgraded reward, sparse move slots, correct
  and wrong Field Trip text, vitamin/Dark Deal messages, formatting, detail pages
  and art/text separation. These checks do not establish hardware pixel fidelity.
- `python3 game/tests/check_mystery_ui_faults.py`: compile actual UI/core faults
  against normal supporting archives. Only assertion failures count; build errors
  or crashes do not. The ordinary app test separately instruments all its sources.
- Fresh native replays: 2,000 cases each for Department Store Sale, Shady Vitamin
  Dealer, Lost at Sea, Field Trip, Part Timer and An Offer You Can't Refuse; 600
  GTS and 400 Salesman cases. All eight comparators report zero mismatches.
  These include requirements, refused cases, costs, rewards, party/item/dex
  changes, experience, PP and RNG within each producer's documented scope.
- The Shady producer now observes the real selected-dialogue formatting, which
  its older capture omitted. It records those native message keys in addition
  to all previously compared payment and post-option messages. The comparator
  remains strict; no extra C messages are ignored to make the test pass.
- Each changed native producer captures BattleScene.prototype.randBattleSeedInt
  before constructing GameManager and restores it after encounter startup.

Normal core/app/custom/save/football CI and the ARM build/stack checks remain
required. Only the separately excluded custom Task63 checks are skipped.
No new release or Windows builder is part of these repairs.

## Older encounter stand-ins remain incomplete

The following fifteen dispatcher cases still contain simplified Task 46 actions
rather than the source's complete encounters: Mysterious Chest, Safari Zone,
Fiery Fallout, The Strong Stuff, Absolute Avarice, A Trainer's Test,
Trash to Treasure, Clowning Around, Dancing Lessons, Weird Dream,
The Winstrate Challenge, Teleporting Hijinks, Bug-Type Superfan,
Uncommon Breed and The Expert Pokemon Breeder.

Examples: Safari Zone gives a ball/potion instead of its three-Pokemon safari;
The Strong Stuff changes a battle stage instead of applying its permanent base
stat modifiers; Trash to Treasure removes the first inventory entries instead
of running its battle/dig branches. Their selection/intro or smoke tests do
not validate the advertised actions. None is hidden or declared complete by
this UI fix. They require separate source-backed ports, including their
multi-step choices, battles, callbacks and reward paths.
