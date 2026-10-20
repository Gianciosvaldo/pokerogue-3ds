# Double-battle encounter chance

Pinned upstream `8555c08c823b856cbec4eb99ca84ea52a955836d`, locales
`270ed0a2938b2c3e2e34e7de940e84aecadf548b`, Node22. Capture native
BattleScene.randBattleSeedInt **before** constructing GameManager; restore it
**after** natural battle startup. No gameplay RNG replacement is retained.

## Source behavior and ownership

DoubleBattleChanceAbAttr divides the NumberHolder by four even in simulation.
The dispatcher supplies the normal primary/passive, duplicate, fainting and
suppression gates. The attribute does not display an ability banner; successful
nonsimulated applications still record summon and wave ability history.
The four enabled registrations are Illuminate, Arena Trap, No Guard and Commander.

BattleScene.getDoubleBattleChance starts with 32 on multiples of ten, otherwise
eight. Each actual global Lure modifier record divides it by four, regardless of
that record's stack count. It applies the ability dispatcher to getPlayerField()
and clamps the result to at least one **after** all applications. That getter
uses the first one or two party slots from the outgoing battle's single/double
state, including recalled or switching Pokémon; it does not require isOnField().

The run-core encounter context now accepts an optional scene callback. Wild and
eligible ordinary-trainer rolls invoke it lazily. Fixed battles, forced trainer
variants, final waves, Endless bosses and mystery encounters retain their native
skip rules. A null callback preserves the existing arithmetic count projection.
The actual game caller supplies the battle-core getter with canonical party
order, refreshed passives, the outgoing field/arena state and canonical Lure
record count. Unsupported reports are retained in game-core diagnostics.

Required fixed-rival cases exposed two existing run-core gaps: Challenge mode
was omitted from the opt-in rival path, and the rival variant always defaulted
to male. Both follow the pinned fixed-battle config now. The context carries
settings.isPlayerFemale; this transient context is not serialized. Existing
opt-in switches remain explicit.

## Proof boundaries

Both seeds, 777 and 2034, compare all 16,384 raw/getter cases each. Cases include
all four registrations, eight actor roles, primary/passive/different/duplicate
abilities, fainting, suppression, switching and physically removed actors,
simulation, fractional holders, outgoing singles/doubles, and zero to three
actual Lure classes with stack counts one or two. All eight complete actor
frames, ordered queues, cached/translated text, ability histories and both RNG
streams are compared. Actual old ab_attrs fails 7,296 of 16,384 first-seed cases.

The actual native newBattle fixtures have no exception-catching or case filters:

- Endless/Spliced Endless: 32,768 cases per seed at waves 2, 10, 49, 50, 201,
  256, 5,850 and 1,000,000. Each seed invokes the getter 20,480 times and skips
  it on 12,288 boss cases. Generated enemy levels, battle type, doubles, battle
  seed and final global RNG match; wide-level enemy values are retained.
- Ordinary Classic/Challenge trainers: 16,384 cases per seed in the actual
  Plains trainer pool at waves 12, 13, 14 and 16. The real mystery-encounter
  cooldown is supplied. First seed: 2,279 trainers, 1,477 trainer callbacks;
  second: 2,235 trainers, 1,499 trainer callbacks. Trainer class, variant,
  template, localized name indexes and all enemy levels are compared too.
- The original Town fixture at waves 6, 7, 8 and 9 remains retained in full:
  16,384 cases per seed, including 4,096 fixed rivals per seed. It is not used
  as evidence of ordinary-trainer callback coverage.
- Extended fixed-rival fixture: both player genders and Classic/Challenge,
  32,768 cases per seed, all native/C exact. Each seed includes 8,192 fixed
  rivals and 24,576 actual wild getter calls. Both player genders are checked
  independently of the Classic/Challenge mode axis.

A passthrough observer captures full actor frames, histories, queues and both
RNG states at the native getter boundary before subsequent scene cleanup.
The constructor and later cleanup execute naturally, and their final global RNG
is compared. This proves generation plus the getter boundary; it does not claim
complete post-battle cleanup acceptance. Every required case ID/footer is
checked, and any unsupported path fails the required matrix.

The real game encounter caller passes 14,336 host checks for first encounter,
canonical party order, primary/passive refresh, suppression/fainting, old field
size, Lure records and lazy boss skips. The pre-existing double-as-single game
projection remains explicitly reported by GC_STUB_DOUBLE_AS_SINGLE.

## Gates and remaining limits

Nineteen compiled implementation faults are caught and twenty malformed inputs
are rejected, including wrong divisions, omitted simulated output/history, party
selection, Lures, clamp placement, missing wild/trainer/game callbacks, party
order, passives, Challenge rivals and player gender. An extra early clamp with
the final clamp retained is observationally equivalent; the clamp-placement
fault instead moves the clamp and removes the final one. No source case or
comparison is removed.

Forty-two cross matrices, twenty-six further matrices and entry/context checks
pass; seven historical corpora lose no covered IDs. Normal game-core tests pass
with identical saved/unsaved digest 15ba877e, and the final clean 3DS build passes.
The final clean app/custom/save sanitizer run passes, including 10,000 normal
runs and all custom/save/quit/resume variants with zero screen/run mismatches,
stuck runs or model violations. Latest Poison Puppeteer raw/full-turn and biome
raw/prefix regressions pass for both seeds. Only the dedicated custom-only
Task63 check is skipped.

A prior app build was cancelled after a mixed-context-layout sanitizer failure
when the transient context changed during compilation. A second clean build
was cancelled for disk pressure; the final clean two-job build is the validated
result. Old logs and all source captures are retained. Earlier inactive proof
captures were losslessly compressed and verified to make space. No sanitizer,
case or coverage requirement was disabled.

Reproduce source-backed matrices and guards with
`battle-core/tools/double-chance-diff.sh PINNED_UPSTREAM SEED` under Node22.
These bounded proofs do not close the complete source inventory, every
fusion/Transform/challenge/suppression interaction, full game doubles, the
remaining callbacks or final wide-level Endless acceptance.
