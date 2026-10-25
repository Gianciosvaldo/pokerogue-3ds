# Egg presentation and species-candy purchases

Ready eggs now emit a separate appearance event before their existing hatch and
move-unlock messages. The app pauses for a shaking/cracking egg, then shows the
actual generated Pokémon using the shared animated front-sprite loader (including
form, gender, shiny colour and variant). A/B can reveal early; the result remains
visible for at least 500 game milliseconds before continuing. Presentation never
rolls a Pokémon or changes profile rewards.

Customize → Use candies → Buy species egg purchases an egg of that starter.
Pinned upstream `8555c08c823b856cbec4eb99ca84ea52a955836d` supplies the prices,
hatch-count discounts and constructor behavior: 99-egg limit, source type 3,
shiny chance 1/12, boosted egg-move odds and the existing same-species hatch ability
roll. Species-specific tier and variant support come from the actual registry,
including starters absent from random-gacha pools (such as Pikachu). The constructor
still consumes its initial random tier/ID before overriding the species tier.
Species purchases do not affect voucher counts, gacha pity or pull statistics.
A failed save restores both candy and egg storage.

Validation includes a committed native producer/corpus with all 572 starters,
21 hatch-count price boundaries per starter, and 2,288 egg constructions. The C
comparison passes 93,242 checks, including exact generated fields, entropy states,
unchanged scene RNG, complete profile/save round trips and purchase refusals.
Native samples include 171 shinies, 45 variants and 225 rare egg moves. These are
bounded constructor samples, not a claim of exhaustive RNG coverage.

The actual hatch renderer is checked through recorded draw calls for egg/crack/
Pokémon progression, all three shiny variants, held-input suppression, early reveal,
minimum dwell and one continuation/asset cleanup. The real victory/profile/save
hatch test also checks 99 eggs without event overflow (144 checks). Expanded
starter UI tests cover species purchase/reload and failed-write rollback (11,871
checks). The actual 3DS build passes with a strong 524,288-byte stack; 2,278 compiler
frame reports are checked, maximum frame 45,056 bytes. Full core/app regressions
and exact-head CI remain required before merging. Physical-console presentation
has not been tested here.

The Lure/doubles integration is a separate follow-up: the existing probability
callback still feeds the previous deliberate single-battle fallback at this point.
