# Mystery trainer presentation, shiny sparkles and missing result names

Source: `8555c08c823b856cbec4eb99ca84ea52a955836d`.
Locales: `270ed0a2938b2c3e2e34e7de940e84aecadf548b`.
Assets: builder pin `87426a79611f9d212c4dc8af58e2834a05b93725`.

Pillar Men start a trainer fight through GC_EV_TRAINER without a new WAVE.
The app ignored that event, retaining the mystery scene. Rebuild the trainer
scene on that transition, using current wave/biome metadata rather than the
TRAINER event's class/variant fields. This enables existing custom trainer art
and iconic dialogue. A WAVE-owned TRAINER (including save reload) is consumed
by its wave scene and must not rebuild again. Track extra scenes separately;
the original wave/music coverage requirements remain.

PR_EV_SHINY_SPARKLE was explicitly discarded. Forward it and play the three
native `images/effects/shiny[_2/_3]` atlases with `se/sparkle`. Source
ShinySparklePhase waits 1000ms; animations.ts supplies frames 0.png..34.png
at 32fps and Pokemon creates the sprite at origin (0.5,1). The builder's pinned
three JSON atlases were fetched and checked with gfx.atlas_frames: each has
those 35 frame names. All four field slots use their actual position/scale.
Release transient sparkle texture caches after the last overlapping animation.

Two source-backed encounters omitted dialogue values: both Part-Timer result
messages need selectedPokemon (including its primary Pokemon on option 3), and
Snorlax restResult needs snorlaxName. Supply values at their actual callbacks;
do not remove unknown interpolation syntax from the battle-core formatter.
Pinned Part-Timer sets selectedPokemon for all three choices; pinned Snorlax
sets snorlaxName on initialization. Their localized result strings require it.

Validation:

- Actual core ShinySparkle phase -> actual event adapter: 12 cases, all four
  field slots and all three variants. Retained 24 Dig/charge visibility cases
  still pass under ASan/UBSan/LSan.
- Actual sparkle drawing helper: 144 slot/variant/frame-boundary combinations,
  plus absent slots and overlapping lifetime/cache-release controls. Retained
  sprite-memory retry, command undo and double-battle box checks pass.
- Actual encounter callbacks + actual locale formatter/UI: added checks fail
  seven times before the token fix with literal {{selectedPokemon}} or
  {{snorlaxName}}. Afterward all 438 checks pass under ASan/UBSan/LSan.
- App driver: 10,000 ordinary runs; 300 strong custom runs (42 extra same-wave
  trainer scenes, 85 JoJo introductions and iconic lines); 100 custom runs
  with save/resume (12 same-wave scenes). All report zero model violations.

Host drawing tests use renderer stubs; they are not a physical 3DS screenshot
comparison. Mysterious Challengers' placeholder battle/reward implementation,
Safari Zone, Teleporting Hijinks and the reported Multi Lens sequence remain
separate work. This change does not claim all mysteries or battle-core complete.
Retain normal app/core/custom/save/football and actual ARM/stack CI; skip only
Task63. No Windows builder is produced.
