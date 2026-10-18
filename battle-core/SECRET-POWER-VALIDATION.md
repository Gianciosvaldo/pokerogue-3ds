# Secret Power secondary effects

Source pin `8555c08c823b856cbec4eb99ca84ea52a955836d`, locales
`270ed0a2938b2c3e2e34e7de940e84aecadf548b`, Node 22.23.3.

Secret Power's outer callback checks target HP, then constructs a fresh status,
stat-stage or flinch attribute. Terrain takes precedence over biome. Implement
the pinned four statuses, five stat drops and flinch selection, reusing existing
chance, status, stat and tag machinery. The status helper is factored from the
existing StatusEffectAttr/MultiStatusEffectAttr path without adding a parent HP
gate to those ordinary callbacks. No state/save fields or generated data change.

Keep the pin's rainbow exception: Secret Power does not receive the rainbow
chance multiplier, including when it constructs a flinch child. Serene Grace,
Shield Dust, Sheer Force, passive abilities and flinch immunity continue through
the existing dispatchers. A flinch child still calls the range-one duration
selector. `Battle.randSeedInt(range <= 1)` returns the minimum without advancing
RNG state; call observations, rather than state alone, distinguish this path
from omitting the call. The same detail applies to Conversion 2's singleton
selector in the preceding slice.

Reproduce `tools/secret-power-diff.sh UPSTREAM 777`, then 2034. Capture native
BattleScene battle RNG before constructing GameManager and restore it after
natural doubles startup. Source factory and RNG spies pass through to the real
methods; no gameplay results are mocked or replaced.

Both seeds match all 24,576 raw cases. Every one of the 35 sparse biome IDs has
positive native effects with no terrain; all five terrain states and all ten
secondary-effect variants have positives. The smaller observed positive count
per biome without terrain is five (777) and three (2034). These counts describe
the fixtures, not exhaustive interaction coverage.

Compare all actors' 125-field input/output snapshots, full status timing and
pending state, stored Flinched and rainbow tag fields, world input, queue levels,
order and text, stat and status dynamic payloads, ability displays and both RNG
states. Protocol 2 adds real call counts/ranges; the original captures remain
accepted. Native boolean returns are recorded; C dispatch is void and no C
return-value parity is claimed. Rainbow/existing-tag setup uses actual source
factories; their setup presentation is outside the raw boundary.

Per seed, 6,144 rainbow controls and 3,092 fainted targets are observed. Native
positive/negative returns are 2,040/10,248 and 2,001/10,287. Each status/stat/flinch
variant is observed with positive returns. All 16,384 owning doubles turns match
exactly, with zero source exceptions or unsupported cases. Actual successful
callbacks are 659 and 662; wide raw damage counts above INT32 are 3,706 and 3,726.
Existing ally/spread/faint/switch comparison floors remain intact.

The first native transport omitted dynamic ObtainStatusEffectPhase payloads and
stopped at its assertion; those incomplete attempts are excluded. The corrected
transport observes both status and stat dynamic queues. An initial C name-check
mistake treated a template as a locale key. Its formatter-only fixture was fixed
to use the real production Pokemon-argument renderer; no captured names changed.
A failed build briefly reused the previous failing binary. Dependent commands
now stop on build failure, and the corrected binary passes both complete traces.

All 28 compiled perturbations are detected, including one call-observation
perturbation for the range-one selector; 20 malformed captures are rejected.
They cover mapping, terrain precedence, outer HP gates, double/chance/rainbow
handling, child recipients/sources/options, status quietness, overlap and owning
dispatch. The first duration-call omission was invisible to RNG state alone;
adding native call observations strengthens the comparison without changing
source outcomes or reducing any previous cases.

Retained doubles are 1,993/1,989/1,993 and mixed turns are
1,997/1,997/1,994/1,998: ten additional exact cases and no lost covered IDs.
All 24 retained/integrated captures pass after rebasing on verified type-move
merge #309, including original/observed Secret Power, type moves, Pledge,
HP-cost boosts and Pain Split. Normal game-core tests pass; saved/unsaved
digests both equal `361c1d0e`. Normal app/custom/save tests and the 3DS build
run on the exact proposed head in CI.
These are bounded default-challenge Classic/Endless observations, with canonical
non-fused actors. Full fusion/Transform/item/passive interactions, hardware,
natural high-wave progression and the complete source inventory remain open.
Normal app/custom/save tests and exact-head 3DS CI remain required; only the
dedicated custom-only Task63 workflow is skipped.
