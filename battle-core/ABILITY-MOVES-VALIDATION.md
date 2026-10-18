# Ability-changing move callbacks

Pinned upstream `8555c08c823b856cbec4eb99ca84ea52a955836d`, locales
`270ed0a2938b2c3e2e34e7de940e84aecadf548b`, Node 22.23.3.

Implement AbilityChangeAttr, AbilityCopyAttr, AbilityGiveAttr and
SwitchAbilitiesAttr for Worry Seed, Simple Beam, Role Play, Doodle, Entrainment and
Skill Swap. All raw callbacks retain the parent's HP gate and omit move-condition
and chance gates. Their registered move conditions remain separate.

Change queues individual weather-form reversion before and after setting the
ability, breaks any existing illusion and announces the acquired ability. Copy
announces and sets the user, then reads the target again for a living partner
when Doodle runs in doubles. Give uses the user's primary ability. Swap snapshots
the old user ability before either setter, then invokes the field weather-form
reversion scan. All setters reuse the actual lose/override/gain implementation.
No save fields or generated tables change; switch/summon reset clears overrides.
Illusion registrations remain disabled at the source pin.

Reproduce `tools/ability-moves-diff.sh UPSTREAM 777`, then 2034. Both producers
capture native BattleScene RNG before GameManager construction and restore it
after natural double-battle startup. The raw matrix observes registered apply,
actual move.applyConditions and resetSummonData. UI refresh is intercepted; actual
callbacks, phase construction and RNG are observed.

- Final raw matrix: 12,544 per seed, all 25,088 exact. Includes six moves, all four
  field positions and self/ally/opponent aliases, zero/alive HP, levels up to
  1,000,000, suppression and passive inputs, nine ordinary primary inputs plus
  Primordial Sea. The NONE input retains native fallback semantics.
- The original 12,288 structural cases are retained. Each seed adds 128
  Castform/Cherrim raw controls with 172 quiet-form phases. All those form controls
  fail the separate move conditions; their raw API observations do not establish
  natural move eligibility. Another 128 controls invoke 133 actual primal-weather
  loss callbacks; 117 of these controls pass the separate move conditions.
- Each seed records 2,448 denied raw calls, 7,042 failed move conditions and 1,377
  living Doodle partner branches. Compares all four actors' 125-field input/output/
  reset frames, weather state/durations, message order/levels/text, quiet-form
  recipient/index, cached ability identity/passive, animation marker and dynamic
  payload, plus both RNG states. C dispatch is void; native boolean returns are
  observed without a C return-value parity claim.
- The original 8,192 owning turns per seed are retained and exact (16,384 total),
  with full states, text, trace, queues and RNG, no source exceptions or unsupported
  cases. Actual gain callbacks include Intimidate, Download, Intrepid Sword and
  Drizzle. Original ally/spread/faint/switch coverage floors remain intact.
- Expanded owning matrix: 8,320 per seed, all 16,640 exact. Each seed appends
  128 real Role Play/Doodle/Skill Swap turns with a sole Primordial Sea source
  under heavy rain; every added turn invokes its actual weather-loss callback.
  This checks loss/clear and subsequent gain ordering through real phases, in
  addition to the raw API proof. Original 8,192-turn captures remain exact.
- All 24 compiled implementation/helper/caller faults are caught, including raw
  gates, source/recipient, both form triggers, Doodle's partner and HP gate,
  swap snapshot, messages, RNG, lose/gain helpers and owning dispatch. Twenty-four
  malformed captures are rejected.
- Retained doubles are 1,981/1,985/1,987; mixed 1,992/1,990/1,994/1,998. Thirty
  new exact cases, no lost covered IDs. Normal game-core tests pass, including
  equal saved/unsaved digests.
- The preliminary protocol omitted waveSeed and could not establish the weather
  scan's seeded tie order. It is excluded. Complete versions 2/3 remain accepted
  and exact. The first primal capture rejected a real dynamic CommonAnim marker;
  it is incomplete and excluded. The final transport captures that marker and its
  full payload, including animation IDs above 255, without rewriting source values.

The matrices are bounded and do not close every fusion/Transform/form/item/terrain
interaction, natural high-wave progression or the final source inventory. Normal
app/custom/save and exact-head 3DS CI remain merge gates; only the dedicated
custom-only Task63 workflow is skipped.
