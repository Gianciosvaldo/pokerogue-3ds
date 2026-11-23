# Challenge Mode, part 1: the foundation and Fresh Start

Upstream pin 8555c08c823b856cbec4eb99ca84ea52a955836d. What this change makes playable: Challenge Mode (`GAME_MODES_CHALLENGE`) in game-core, with
**Fresh Start** (values 1 and 2) as the only challenge. Every other challenge is refused by `gc_run_set_challenges` and by `gc_run_load` until its own PR.

## What upstream has (checked, not assumed)

* `allChallenges` holds 10 challenges. `LOWER_MAX_STARTER_COST` and `LOWER_STARTER_POINTS` exist as ids but are **not** in that list: no value, no effect
  (the earlier plan counted 12; the producer showed the budget staying 10 with them set). Their maxima are 0 here.
* Challenge Mode is Classic with `isChallenge` and the challenge hooks: every strict Classic check in game-core now reads `gc_is_classic_mode`.
* `species.getLevelMoves()` = the species' level moves + `formLevelMoves[forms[0].formKey]`, no duplicate removal, stable-sorted by level
  (Fresh Start's moveset rule reads it; Calyrex and Indeedee have form rows that matter).
* A starter without a tera type gets `starterPokemon.species.type1` (the species', not the form's). game-core used the form's: fixed in `gc_mon.c`
  (Oricorio Pom-Pom was the case the differential caught; the change is for every mode).

## Differential (upstream functions replayed)

`tools/challengestartersdiff.sh <upstream checkout> [N] [seed]` runs `tests/challenge-starters.producer.test.ts` in the pinned upstream
(Node 22, vitest): per case a random profile (dex, natures, abilities, passives, IVs, egg moves, value reduction), then for Fresh Start 0/1/2
12 starter queries (cost, `checkStarterValidForChallenge`, `getStarterData` through the real `applyChallenges`) and a party through the real
`SelectStarterPhase.initBattle` (id, form, nature, ability, passive, luck, tera, HP, stats, IVs, moves, and the RNG state after). The comparator
`tests/challenge_starters_diff_test.c` replays the same inputs through game-core.

    cd game-core && make -s -j8 BUILD=/tmp/gcb /tmp/gcb/libgc.a
    BUILD=/tmp/gcb tools/challengestartersdiff.sh <upstream> 400 <seed>

| seed | cases (Fresh Start on) | queries | party members | checks | TOTAL mismatches |
|---|---|---|---|---|---|
| 777 | 400 (342) | 4800 | 1418 | 113304 | 0 |
| 9001 | 400 (342) | 4800 | 1404 | 112912 | 0 |
| 4242 | 400 (342) | 4800 | 1407 | 112996 | 0 |
| 31337 | 400 (342) | 4800 | 1369 | 111932 | 0 |

`tools/challengestartersplant.sh <trace>` replays a trace through copies of game-core with one line changed each (21 plants: cost, each
select-view mask, nature mask, IV cap, form reset, ability wrap, passive, move level bound, form moves, level sort, fill duplicates, luck, IV cap,
tera type, the hard-valid rule, the default tera). On the seed 777 trace every one exits 1 except `default-tera-form`, whose case (a form
with a different first type and no tera type) is not in that trace; on the seed 4242 trace it exits 1 too (`TOTAL mismatches=1`).

## Host test (what has no upstream function to replay)

`tests/challenge_host_test.c`, 184 checks (`make -C game-core challenge-host`): admission (Classic run refuses values; above-maximum, unplayed
challenges, Lower* ids refused; flags synced to run-core; refused after the starters), Fresh Start's nature filter and starter rules through
`gc_run_set_starters`, the ribbon table (Moveset Randomizer none; Flip/Inverse keep only their own; Passives on all; Nuzlocke needs all three;
Single Generation/Type bits; Limited Support), a Challenge win counted as a Classic one with the ribbons, a win with an unplayed challenge
refused and unsavable, the `challenges` save section round trip and its refusals (value too high, unplayed challenge, section missing, Classic
save has none). `tools/challengehostplant.sh`: 9 plants, all exit 1.

## Gaps (stated, not hidden)

* The ribbon table follows upstream's `awardRibbons` as read, **not** replayed against upstream; the host test pins that reading.
* `ChallengeAchv` achievements on a challenge win are not ported.
* `checkStarterValidForChallenge`'s soft step (a species' item-triggered form change to a valid form) is not done; no challenge played so far
  looks at the form. The evolution walk of the soft path is only exercisable with Single Type/Generation (later PRs): `softOnly=0` here.
* Single Generation, Single Type, Inverse, Flip Stat, Passives, Hardcore, Limited Catch/Support and Moveset Randomizer are refused until their PRs.
* Daily-Run/Challenge choices in the app (`game/`) still force Classic: that is PR 1b.
* run-core edit (disclosed, approved by lorenzo per PR): `run-core/src/shop.c` flags the shop UNPORTED for a Challenge run only when it has
  a challenge game-core does not play (`pr_run_has_unported_challenges`), otherwise as for Classic.

# Challenge Mode, part 2: Inverse Battle, Flip Stat, Passives

What this change makes playable: the three battle-rule challenges, alone or with Fresh Start. They are rows of the challenge screen (the app
lists what `gc_challenge_played_mask` plays). Hardcore, Limited Catch/Support, Single Type/Generation and Moveset Randomizer stay refused.

## What upstream does (8555c08, read and replayed)

* **Inverse Battle** is `applyChallenges(TYPE_EFFECTIVENESS)` inside `getTypeDamageMultiplier`: below 1 becomes 2, above 1 becomes 0.5, UNKNOWN
  stays 1. Every caller of that function sees it: `getAttackTypeEffectiveness` (and through it damage, the AI's matchups, hazards), Freeze-Dry,
  Nihil Light, Conversion 2's resistance pick, Strong Winds' check, and rival-team-gen's weakness walk. Here they all read
  `pr_type_chart_multiplier` (battle-core, ai-core) or `chart_mult` (run-core `rival.c`).
* **Flip Stat** is `applyChallenges(FLIP_STAT)` in `calculateBaseStats`: HP with Speed, Attack with Sp. Def, Defense with Sp. Atk, on the
  species' stats before Shuckle Juice / Old Gateau and, for a fusion, on the fusion's before the average.
* **Passives** is `applyPassiveAccess` in `hasPassive`: with the challenge on, the Classic final boss / Endless boss rule no longer removes the
  enemy's passive; value 1 gives a trainer's Pokemon its passive (the final boss is a boss, which has one anyway); value 2 gives every Pokemon one.
  A Challenge Mode game's final boss is wave 200 like Classic's: before this change battle-core only applied that rule to `GAME_MODES_CLASSIC`.
* The app's summary showed the passive from the unlock flag; it now shows it when the Pokemon has one (`hasPassive()`, as upstream's summary).

## Differential

The Fresh Start producer and comparator (part 1) were extended, so the same command checks all four challenges: each case also draws Inverse
(0/1), Flip (0/1) and Passives (0/1/2) and records `getTypeDamageMultiplier` over the whole chart (-1..18 squared, once per Inverse value),
each party member's `getAttackTypeEffectiveness` against the 18 types, its stats (flipped by the real `calculateStats`), its `hasPassive()`,
member 0 fused with member 1 and recalculated, and an enemy's `hasPassive()` for wild/trainer, boss/not, passive/not on waves 5, 25, 200, 250.

    cd game-core && make -s -j8 BUILD=/tmp/gcb /tmp/gcb/libgc.a
    BUILD=/tmp/gcb tools/challengestartersdiff.sh <upstream> 400 <seed>

| seed | Inverse / Flip / Passives cases | chart values | effectiveness | flipped fusions | enemy passives | checks | TOTAL mismatches |
|---|---|---|---|---|---|---|---|
| 777 | 141 / 129 / 259 | 800 | 25416 | 344 | 12800 | 155628 | 0 |
| 9001 | 139 / 137 / 264 | 800 | 25218 | 338 | 12800 | 155075 | 0 |
| 4242 | 137 / 122 / 267 | 800 | 24912 | 335 | 12800 | 154258 | 0 |
| 31337 | 139 / 124 / 269 | 800 | 25848 | 331 | 12800 | 156678 | 0 |

`tools/challengebattleplant.sh <trace>` replays a trace through copies of game-core and battle-core (and run-core, ai-core, reward-core: they
build battle-core from their own `../battle-core`), each with one line changed: see the PR for the run.

## Gaps

* The rival weakness walk under Inverse (`run-core/src/rival.c`) is ported by reading, not replayed: the producer has no rival team.
* Inverse inside a whole turn (damage numbers, messages) is not replayed; the chart, `getAttackTypeEffectiveness` without a source, and the
  places that read the chart are. With a source the immunity-ignoring abilities and tags run on the reversed chart value as upstream's do
  (they only look at 0, which Inverse never gives).
* run-core edits (disclosed, approved by lorenzo per PR): `rival.c` (Inverse in the weakness walk), `include/pr/run.h` (the three challenge bits
  and `challenge_passives`; `PR_CHALLENGE_RULES_PORTED` includes them).

# Fix: the rewards read the run's challenges

Before this fix game-core's reward context (`gc_reward.c` `ctx_base`) left `challenges` at 0, so reward-core never saw a Challenge Mode game's
challenges:

* Fresh Start: the Eviolite and the Mini Black Hole stayed in the pool when unlocked (upstream: `!gameMode.isFreshStartChallenge()`).
* Flip Stat: a Mint or a vitamin recalculated the stats with a `pr_battle` of zeros, unflipped, and wrote them back to the Pokemon (they stayed
  wrong until the next recalculation). reward-core refused Flip Stat outright, but the refusal never fired because the bit never arrived.

Now `ctx_base` sets `challenges` from the run (Challenge Mode only), and reward-core's Mint and vitamin recalculate with Flip Stat
(`pr_reward_stats_battle`, `src/pools.c`) instead of refusing it. Hardcore's revive rules in reward-core (part 3) read the same field.

The challenge-starters producer and comparator now also check, per case: the Eviolite's and the Mini Black Hole's PLAYER pool weights with both
unlocked (upstream's weight functions, the port's `pr_reward_regenerate_thresholds` through `gc_reward_ctx`), then a Mint and a vitamin on the
first member through `gc_port_apply_reward` (applied or not, hp, the six stats).

| seed | Fresh Start cases / open pools | Flip cases with both applied | reward weights | reward applications | checks | TOTAL mismatches |
|---|---|---|---|---|---|---|
| 777 | 342 / 50 | 123 | 400 | 800 | 162428 | 0 |
| 9001 | 342 / 52 | 134 | 400 | 800 | 161875 | 0 |
| 4242 | 342 / 53 | 118 | 400 | 800 | 161058 | 0 |
| 31337 | 342 / 51 | 118 | 400 | 800 | 163478 | 0 |

Plants (`tools/challengebattleplant.sh`): `rewards-without-challenges` (the old `ctx_base`), `rewards-stats-without-flip`,
`rewards-fresh-pools-ignored`. reward-core's `minttest`/`vitamintest` self-tests: the Flip Stat refusal check became a check that the Mint and the
vitamin apply with flipped stats.

# Challenge Mode, part 3: Limited Catch, Limited Support, Hardcore

The three are now in `gc_challenge_played_mask`: the challenge screen lists them (upstream's order: after Flip Stat, before Passives), a run takes
them, and a save that holds them loads. Single Generation, Single Type and Moveset Randomizer are still refused.

## What upstream does (8555c08, read and replayed)

* **Limited Support** (value 1 no heal, 2 no shop, 3 both). `applyShop` keeps the shop only with value 1 (`gameMode.getShopStatus()`),
  `applyPartyHeal` keeps the end-of-biome heal only with value 2. Without the heal, `SelectBiomePhase.setNextBiomeAndEnd` queues a
  `SelectModifierPhase` with the current wave's fixed battle's `customModifierRewardSettings` where the `PartyHealPhase` was. No Classic fixed
  battle at a wave X0 (only the Champion's, 190) sets any, so it is a plain reward screen; the X0 wave has no shop (`getPlayerShopModifierTypeOptionsForWave`
  is empty there). `unshiftNew` keeps the order of its calls: the screen, then `SwitchBiomePhase`.
* **Hardcore**. `SHOP_ITEM`: no item of the "revive" group (Revive, Max Revive, Sacred Ash) in the shop. `WAVE_REWARD`: none in the rewards
  (reward-core, already). `PREVENT_REVIVE`: `PartyHealPhase` skips a fainted Pokemon entirely (no hp, status or PP; `playerTerasUsed` still resets),
  a revive's select filter always says no effect (reward-core, already). `POKEMON_FUSION`: no fainted fusion (game-core, already). `POKEMON_MOVE`:
  Revival Blessing is not valid. It is read in three places: `PokemonMove.isUsable` for the player's Pokemon (text `battle:moveCannotUseChallenge`),
  `CallMoveAttrWithBanlist.isMoveAllowed` for any user (Metronome, Copycat, Mirror Move, Assist, Sleep Talk), and `MovePhase.checkValidity`. The last one
  starts its holder at `false`, which Hardcore leaves alone (`if (isValid.value)`), so it never cancels a move: not ported, as it does nothing.
  The party screen's `updateOptionsHardcore` for a fainted member.
* **Limited Catch** (`applyPokemonAddToParty`): a catch joins the party only when the Pokemon's `metWave` (the wave it was made in, always the
  current one here) is a biome's first (`% 10 === 1`), or the battle is Teleporting Hijinks' and its selected option is not `options[2]`.
  Otherwise `AttemptCapturePhase` says `battle:pokemonCaughtButChallenge`, `setPokemonCaught` still runs, and the Pokemon is removed with no
  party prompt. An encounter's `catchPokemon(.., isObtain)` always adds an obtain (the Salesman's).

## Where it is ported

| rule | file |
|---|---|
| shop status, shop items, party heal status, prevent revive | `run-core/src/shop.c` (disclosed run-core edit, with `include/pr/run.h`: the three bits, `challenge_limited_support`) |
| the value on the run, the played mask, `gc_challenge_add_to_party_ok` | `game-core/src/gc_challenge.c` |
| the no-heal reward screen, the heal that skips the fainted | `game-core/src/gc_phases.c` (`set_next_biome`, `party_heal`) |
| the catch that does not join, the Safari Zone's text | `game-core/src/gc_catch.c`; the battle's text from `bt->catch_not_added`, set at the ball command (`gc_battle.c`) |
| Teleporting Hijinks' exception | `gc_teleport.c` sets `me_teleport_catch` for the first two options; `gc_new_battle` clears it |
| Revival Blessing | `battle-core`: `pr_challenge_move_allowed` (`damage.c`), read by `pr_move_usable` (`commands.c`) and `move_allowed` (`called_moves.c`) |
| the fainted member's party options | `game/source/party_screen.c` sets `party_args.hardcore` (party.c already had `updateOptionsHardcore`) |
| the challenge screen's description scroll (`updateDescriptionScroll`) | `game/source/challenge_screen.c`, `chm_desc_scroll` in `challenge_model.c` |

## Differential

`tools/challengestartersdiff.sh <upstream> 400 <seed>`: each case now also sets Limited Catch (0/1), Limited Support (0..3) and Hardcore (0/1) in
the real Challenge game mode, and records `getShopStatus()` and `getPlayerShopModifierTypeOptionsForWave` (item and cost) on two waves, the
`PARTY_HEAL` and `PREVENT_REVIVE` answers, Revival Blessing, Tackle and a random move through `applyChallenges(POKEMON_MOVE)`,
`new PokemonMove(move).isUsable(member 0, true)` and Copycat's `isMoveAllowed`, and `applyChallenges(POKEMON_ADD_TO_PARTY)` for three enemies
(met on a biome's first wave or not; no encounter, Teleporting Hijinks or Fight or Flight, each option).

| seed | shop lists / items | heal + revive values | move values | add-to-party values | checks | TOTAL mismatches |
|---|---|---|---|---|---|---|
| 777 | 800 / 7355 | 800 | 3600 | 1200 | 178787 | 0 |
| 9001 | 800 / 7706 | 800 | 3600 | 1200 | 178087 | 0 |
| 4242 | 800 / 7646 | 800 | 3600 | 1200 | 176305 | 0 |
| 31337 | 800 / 7478 | 800 | 3600 | 1200 | 179163 | 0 |

Coverage on seed 777 (the producer's counters, each required above 0): 372 Hardcore shop lists, 406 closed shops, 205 cases with no heal,
207 Revival Blessing refusals, 269 refused party adds, 71 teleport adds away from a first wave.

The reward application differential (`tools/applydiff.sh`) no longer skips its Hardcore cases: on seed 777 (N 4000), 680 of the 2730 replayed
cases are Challenge Mode with Hardcore, 0 mismatches (revives refused, Sacred Ash's no effect, the rest as in Classic).

Plants (`tools/challengebattleplant.sh <trace>`, 26 in all, 0 missed): the 14 of parts 2 and the fix, and `shop-hardcore-keeps-revives`,
`shop-hardcore-keeps-sacred-ash`, `shop-hardcore-outside-challenge`, `support-shop-open`, `support-heal-on-both`, `prevent-revive-off`,
`support-not-synced`, `revival-blessing-allowed`, `usable-skips-challenge`, `copycat-skips-challenge`, `limited-catch-any-wave`,
`limited-catch-no-teleport`.

## Host tests

* `challenge_host_test` `run_rules`: per Limited Support value the statuses, the shop on wave 12, and the queue after the biome's last wave
  (heal or reward screen, then the biome switch; the X0 screen has no shop and leaves the hp alone); with and without Hardcore the shop of wave 199,
  `PartyHealPhase` on a fainted and a hurt member, Revival Blessing for the player's and the enemy's Pokemon, and Copycat.
* `catch_test limited`: a real throw on the first wave (kept), on wave 2 (refused, the text, the dex, no prompt even with a full party), after a
  teleport (kept); an encounter's catch with c = 1 (obtain, kept), 2 (refused, no text) and 3 (refused, the challenge text).
* `mystery_teleport_test`: the flag after options 0 and 1, not after 2, and cleared at the next wave.
* `game/tests/party_item_test` `hardcore_cases`: a fainted member offers only Cancel on the Ether and Apply screens and only Release on a catch's
  release prompt, with Hardcore; the usual options without it, and for the member that is not fainted.
* `game/tests/challenge_screen_test`: the three rows in order, Limited Support's four values and descriptions, the description clip, and the
  scroll's timeline (delay, halfway, hold, back, restart on a new description).
* Host plants (`tools/challengehostplant.sh`, now also on `catch_test` and `mystery_teleport_test`): 23, 0 missed. By hand on the app (not in a
  script): `party_screen.c` without the flag on the item screen or on the release prompt, and the challenge screen without the scroll, each made its
  test fail.

## Gaps

* The battle's switch screen and Check Team keep their usual options for a fainted member under Hardcore. Upstream offers Release alone there, and
  game-core has no release outside a catch's full-party prompt (Check Team hides Release in this port already).
* The detuned faint cry under Hardcore (`faintCry`, audio) is not ported.
* `PartyHealPhase` and the no-heal reward screen are host-tested on game-core's phases, not replayed against upstream's phases.
* The game speed's effect on the description scroll is assumed to be the same as upstream's tween (the screen's update gets the scaled time).
* With all ten challenges (part 4) the challenge window would be 12 rows and no longer fit above the description strip: part 4 needs a
  scrolling list (upstream shows 9 rows and scrolls).
* Nothing here was run on a 3DS or in Azahar.
* run-core edits (disclosed, approved by lorenzo per PR): `include/pr/run.h`, `src/shop.c`.

# Challenge Mode, part 4a: Single Type, and the in-battle rule it shares with Single Generation

Mono Type (`SINGLE_TYPE`) is now in `gc_challenge_played_mask`: the challenge screen lists it first (Single Generation, before it upstream,
is still refused), a run takes it, and a save that holds it loads. Single Generation's in-battle rule is ported with it (one function
answers both), but Single Generation itself stays refused until its fixed battles are ported (part 4b).

## What upstream does (8555c08, read and replayed)

* **STARTER_CHOICE** (`checkSpeciesValidForChallenge`): the form's types (or generation) must match. With `soft`, a starter also passes when an
  evolution of it does (an evolution whose `preFormKey` is set and is not the starter's form key is not followed; an empty key `""` is falsy and
  is followed), or when one of the species' form changes whose trigger has an `item` leads to a form that does. The party select allows such a
  "soft" starter next to a valid one (`starterSelectUiHandler:invalidParty` otherwise).
* **STARTER_SELECT_MODIFY**: Single Type drops Ralts' female bit under Fighting (Gallade), Snorunt's male bit under Ghost (Froslass), Burmy's
  female bit under Flying (Mothim) and its male bit under Grass, Ground and Steel (Wormadam); Single Generation 4 drops Ralts' female and Snorunt's
  male bits. `applyChallenges` runs `ret ||= c.applyX(..)`, so once one challenge answers true the later ones are not called: Fresh Start,
  Single Generation and Single Type all answer true, so only the first that is on (allChallenges order) applies.
* **POKEMON_IN_BATTLE** (`isAllowedInChallenge`): only the player's Pokemon are judged. Single Type: `isOfType(value - 1, {includeTeraType:
  false, bypassSummonData: true, ignoreThirdType: true})`, or Castform under Normal (`TYPE_OVERRIDES`). Single Generation: the species' and the
  fusion species' generation. `isAllowedInBattle` is that and not fainted, and `isActive` is built on it, so every "is this Pokemon active" check
  of the battle reads the challenge.
* **TurnInitPhase**: a player Pokemon on the field that is not allowed gets `challenges:illegalEvolution`; with none allowed the phase queue is
  cleared and `GameOverPhase` runs (the clear drops that message); with as many allowed as battlers (or, in a double, the first allowed one not on
  the field) it is switched out through the party screen; otherwise it just leaves the field; a double with one allowed Pokemon toggles to a
  single position.
* The party screens refuse a member the challenge does not allow with `partyUiHandler:cantBeUsed` (`FilterChallengeLegal`), the rewards' pool
  weights and the win's ribbons skip such members, and the Mystery Encounters' party helpers read `isAllowedInChallenge` / `isAllowedInBattle`.

## Where it is ported

| rule | file |
|---|---|
| `isAllowedInChallenge`, `isAllowedInBattle` | `battle-core/src/damage.c` (`pr_challenge_allowed`, `pr_allowed_in_battle`), the values on `pr_battle` (`challenge_generation`, `challenge_type`) |
| every `isActive` site (the `hp > 0 && on_field && !switch_out_status` pattern) | `battle-core` `ab_attrs.c`, `arena_tags.c`, `battler_tags.c`, `called_moves.c`, `closures_*.c`, `commands.c`, `engine.c`, `item_transfer.c`, `modifiers.c`, `move_effects.c`, `phases.c`, `pokemon.c`, `switching.c` |
| TurnInitPhase's switch | `battle-core/src/engine.c` `pr_run_turn_init_challenge`, called by game-core's `RP_TURN_INIT` (`gc_battle_turn_init_challenge`) |
| the values on every battle | `gc_challenge_to_battle` (`gc_challenge.c`), used by `gc_battle.c`, `gc_encounter.c`, `gc_phases.c` |
| STARTER_CHOICE soft step, STARTER_SELECT_MODIFY | `gc_challenge.c` (`species_valid_soft` over battle-core's `pr_item_form_rules`, `gc_challenge_starter_view`) |
| party screens' refusal | `gc_battle.c` (command switch), `gc_phases.c` (`RP_SWITCH`), `game/source/party_screen.c` (the app's switch modes) |
| rewards, ribbons, Mystery Encounter helpers | `gc_reward.c` (`allowed_in_challenge`), `gc_classic.c`, `gc_mon.c` (`gc_mon_allowed` now takes the run), `gc_mystery.c` |
| the challenge screen's Mono Type row | `game/source/challenge_screen.c`: the type icon (`types` atlas frame, `unknown` at 0) and `singleType.desc` / `descDefault` |

reward-core edit (disclosed): `reward_apply.c` no longer refuses a DNA Splicers fusion when either member is not allowed by the challenge.
Upstream's `FusePokemonModifierType` filter checks only `isFusion` and Hardcore's `POKEMON_FUSION`; that refusal was this port's own.

## Differential

`tools/challengestartersdiff.sh <upstream> 400 <seed>` now also sets Single Generation (about 1 case in 4) and Single Type (3 in 4), and records
`isAllowedInChallenge` for every starter and for 10 probes per case (member 0 made another species and form, from the form-change list or any
species, fused from the 7th probe on). The starter view now covers Single Type's and Single Generation's genders and the `ret ||=` precedence.

| seed | mono cases | allowed values | checks | TOTAL mismatches |
|---|---|---|---|---|
| 777 | 334 | 5486 | 209288 | 0 |
| 9001 | 334 | 5404 | 205686 | 0 |
| 4242 | 351 | 5403 | 205501 | 0 |
| 31337 | 339 | 5409 | 205581 | 0 |

The producer requires each coverage counter above 0: allowed, barred, fused and soft-form probes, and gender-dropped views. The cases without
Fresh Start cycle through Single Generation 4 and Single Type Fighting, Ghost, Flying and Grass and query Ralts, Snorunt and Burmy with both
genders caught (`J` lines: 174 views, about 60 with a gender dropped, on each seed); one case in ten starts its party with Calyrex, whose level
moves are not in level order (Fresh Start's moveset sort, which the new random stream had stopped reaching). Seed 31337 found the soft step not
following a `preFormKey` of `""` that no form has (Hisuian Basculin to Basculegion, Dunsparce to Dudunsparce); fixed.

Plants:
* `tools/challengestartersplant.sh <trace>`: 30, adding `choice-ignores-type`, `choice-ignores-gen`, `soft-no-item-forms`,
  `soft-empty-prekey-not-followed`, `soft-no-form-prekey-not-followed`, `view-fresh-then-type`, `view-type-burmy`, `view-type-ralts`,
  `view-gen-ralts`. On the seed 777 trace 28 exit 1; `soft-no-form-prekey-not-followed` is caught on the 31337 trace (the only one with that
  case), and `default-tera-form` is missed as before (see part 1). On 31337, 28 exit 1 (`soft-empty-prekey-not-followed`, caught on 777, and
  `default-tera-form` are missed).
* `tools/challengebattleplant.sh <trace>`: 31, adding `allowed-type-off`, `allowed-gen-off`, `allowed-fusion-gen-off`, `allowed-first-type-only`,
  `allowed-no-castform`. On the seed 777 trace: 31, 0 missed.

## Host tests

* `challenge_host_test` `single_type`: the admission (Single Type taken, its value on `run->bt`; Single Generation still refused); the party rule
  under Flying (Pidgey valid, Charmander only soft through Charizard, Bulbasaur not at all, Charmander alone `invalidParty`) and under Dragon (Mega
  Charizard X, an item form); `pr_challenge_allowed` for the foe (never judged), a Water tera (ignored), sunny Castform under Normal, a fainted
  member (`pr_allowed_in_battle` false). Then a real wave-1 battle of a Fire run (Charmander, Cyndaquil, Torchic): the lead and Torchic become
  Water, the next turn says `challenges:illegalEvolution`, the lead leaves, the switch screen offers Cyndaquil only and refuses the lead and Torchic
  with `partyUiHandler:cantBeUsed`, Cyndaquil comes in and the next turn has no message; the command screen's switch refuses the lead too. With
  every member Water: game over, and no message (upstream's `clearPhaseQueue` drops it). A win with one Water member: no ribbon for it.
* `game/tests/challenge_screen_test`: Mono Type is the first row, its value is the type icon (`unknown` at 0, Normal at 1, Fairy at 18, no
  further), its description `descDefault` at 0 and `desc` with the type's name after. `challenge_model_test` no longer assumes Fresh Start is the
  first row.
* Host plants (`tools/challengehostplant.sh`, which now copies battle-core for a `battle-core/` plant): 38, 0 missed, adding
  `single-type-unplayed`, `type-not-on-the-battle`, `party-soft-alone-ok`, `soft-no-item-forms`, `allowed-counts-tera`, `allowed-judges-the-foe`,
  `allowed-no-castform`, `allowed-fainted`, `turn-init-no-check`, `turn-init-leaves-only`, `turn-init-not-in-game-core`, `switch-refusal-no-energy`,
  `switch-takes-the-barred`, `ribbons-for-the-barred`, `switch-screen-offers-the-barred`.

Old plant and proof scripts whose anchors the new signatures changed (`gc_mon_allowed(run, m)`, `gc_dance_has_move(run, m)`, `is_active(bt, p)`
and the like) were updated: `avarice`, `berries`, `breeder`, `chest`, `dance`, `fiery`, `fof`, `snorlax`, `training` plants and battle-core's
`check-{arena,attention,commander,final-boss-faint,stance,weather,xerneas-active}-proof.py`. Every anchor was checked to match the new source;
only `check-final-boss-faint-proof.py` (the one in CI) was run, the others need traces from upstream. Six plants that already matched nothing
before this change (`berries win-callback-dropped`, five in `fofplant.sh`) were left as they were.

## Gaps

* **TurnInitPhase's timing.** In the pinned upstream (a headless GameManager run, Splash against Splash), `TurnInitPhase` queues the message,
  opens the party screen and goes on: no CommandPhase is pushed for the empty slot, so the foe's move runs, and the switch-in (a deferred
  `SwitchSummonPhase`) came only at the next turn's start, after another foe-only turn. How much of that is the test harness's timing of the pick
  I don't know. This port asks for the pick first, brings the Pokemon in, then starts the turn again: the newcomer gets a command and the foe gets
  no free turn.
* The double-battle branches (leave the field only; ToggleDoublePositionPhase with one allowed Pokemon) are ported but not tested: no wave-1
  double was found to host them.
* The description does not colour the type's name (upstream: the type's colour and shadow). The `unknown` frame at value 0 is taken from upstream's
  `setFrame`; it was not looked at on a 3DS.
* The message names the Pokemon by its current name; upstream uses `p.name`, which a direct species change does not update (the probe printed
  "Charmander" for the Squirtle-ified lead). Through a real evolution both say the new name, I believe, but did not check.
* The rewards' Attack Type Booster weights that skip members the challenge bars are not in the differential (the trace's reward weights pass,
  but no case isolates them).
* The Mystery Encounters' party filters (Shady Vitamin Dealer, the offer screens, the damage helper) read the new rule, but none is host-tested
  under Single Type, and their refusal texts stay the generic ones.
* Not every one of the ~250 `hp > 0` sites in battle-core was audited by hand: the `hp > 0 && on_field && !switch_out_status` pattern
  (upstream's `isActive(true)`) was converted everywhere; sites upstream writes as `isFainted()` / `hp > 0` (Counter, Revival Blessing, ability
  `canApply` with bypassFaint, the enemy-only and custom JoJo sites) were left alone on purpose.
* run-core still holds an unused `pr_run_challenge_capture_allowed` that has no upstream counterpart (from before part 3).
* With Single Generation (part 4b) the window is 11 rows and no longer fits: the scrolling list comes with it.
* Nothing here was run on a 3DS or in Azahar. No run-core edit in this part; one reward-core edit (above).
