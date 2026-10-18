# Native form callbacks in fusion and Transform contexts

Base: merged #278, `53fba9b112c932023a4b08fcf4ad5aad9b68a5d4`.
Source `8555c08c823b856cbec4eb99ca84ea52a955836d`, locales
`270ed0a2938b2c3e2e34e7de940e84aecadf548b`, Node 22.

This extends the native form and copied-ability proofs with registered callback
contexts and naturally indexed secondary abilities. No runtime defect was found
in these captures, so no runtime implementation is changed. The existing
NoFusionAbility and NoTransformAbility gates are retained; disabling either is
caught by the source comparison.

The actual API matrices each match 16,384 cases (8192 at seeds 777/2034):

- Native Darmanitan, Galarian Darmanitan, Minior, Wishiwashi, Morpeko, Eiscue,
  Mimikyu and Zygarde with a secondary species/form carrying its natural indexed
  ability. PostSummon, PostTurn, PostBattleInit and PostFaint dispatch includes
  normal/simulated/on-gain controls, HP, Tera and weather boundaries. State,
  hierarchical queued phases/text, actual quiet-phase state, applied ability
  sets and RNG are compared. Source's NoFusion gates intentionally cancel these
  form callbacks; their absence is checked rather than bypassed.
- Actual legal `canTransformInto` followed by PokemonTransformPhase, then the
  same native registered callback dispatch and quiet phases. These API actors
  retain native primary species; the complete owning turns below use Ditto.

Complete owning comparisons match 8,000 fused turns and 8,000 Transform turns
(2000 each at seeds 777/9001/4242/31337), with the existing state/trace/RNG/text
comparator and tolerances unchanged. Fused turns use naturally indexed native
abilities and Splash, exercising actual turn ownership and intentional form
refusal. Transform turns use Ditto and natural native targets, compare the
initial Transform turn as well as complete following copied-Splash turns, and
include real native target quiet form changes in the same battle. The prior
#274 damage/status-turn proof remains complementary; Splash does not replace
that damage coverage. Actual Source canTransformInto forbids fused user/target,
which these complete Transform fixtures preserve.

Two compiled gate faults are caught and fourteen malformed API captures are
rejected. An initial parser allowed an unknown temporary species to pass when
its ability gate made the identity irrelevant to a callback; strict species/
form validation now rejects it, and all valid captures still match. Complete
owning runners require every declared case, footer, state/text match and covered
turn; no unsupported or missing case counts as a success.

Reproduce with `tools/callback-diff.sh PINNED_UPSTREAM KIND MODE N SEED`, where
KIND is `fusion` or `transform` and MODE is `api` or `turn`. Pins, Node 22 and
tracked Source cleanliness are required. Only asset/UI/timing transport is
stubbed; gameplay methods/formulas are unchanged. `tools/callback-guards.py`
accepts battle-core, fusion captures, Transform captures and English locales.
The CI build now includes all four new runners.

Production sources and save schemas are identical to validated #278, whose
complete host/app tests, retained battle replays and 3DS build passed. This
proof-only change reruns the new runners and receives its own CI before merge.
No run-core file, custom-content workflow, emulator or hardware claim.

This closes these eight native callback contexts, not every fused move/item,
weather-form registration, nickname/locale, partner/doubles or wide-level path.
The explicit fused species-stat/crit/evolution-item full-turn admission guard is
the next source comparison, followed by broader partner/doubles behavior.
