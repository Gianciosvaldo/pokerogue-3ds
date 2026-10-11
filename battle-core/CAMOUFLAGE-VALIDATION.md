# D03 Camouflage source evidence and validation

Implementation/host validation complete; exact-head CI pending. Base6199b5c5af511a76c443cfac515a45207d2ff458; clean at task start;
open/main PR symbols checked; generated CopyBiomeTypeAttr lacks a dispatcher.
Upstream8555c08c823b856cbec4eb99ca84ea52a955836d, Node22.23.3.

Evidence collected before production edits:

| Source symbol | Relevant lines / intended C behavior | Fixture |
|---|---|---|
| MoveEffectAttr.canApply; SelfStatusMove factory |1744–1750;10382–10383: self parent gate and actual Camouflage attr | both sides/Taunt/sleep and helper refusal |
| CopyBiomeTypeAttr.apply |7586–7617: terrain NONE uses biome; replace summon types with one; preserve added type; payload type message | all35 biomes/all4 terrain overrides |
| getTypeForTerrain |7625–7639: Electric/Electric,Misty/Fairy,Grassy/Grass,Psychic/Psychic; default UNKNOWN | conflicting biomes and fallback helper |
| getTypeForBiome |7648–7704: full mapping below; default UNKNOWN | every defined biome and fallback helper |
| Pokemon.getTypes; C pr_get_types/pr_set_summon_types | C pokemon.c161–228: override list/cache and third type preserved | previous overrides/added Ghost/later actual damage |
| ordinary switch/resetSummonData | C phases.c2501: types and added type reset on ordinary switch | actual preparatory Camouflage then switch |

Pinned biome map: Town/Plains/Metropolis Normal; Grass/TallGrass Grass;
Forest/Jungle Bug; Slum/Swamp Poison; Sea/Beach/Lake/Seabed Water; Mountain Flying;
Badlands Ground; Cave/Desert Rock; IceCave/SnowyForest Ice;
Meadow/FairyCave/Island Fairy; PowerPlant Electric; Volcano Fire;
Graveyard/Temple Ghost; Dojo/ConstructionSite Fighting; Factory/Laboratory Steel;
Ruins/Space Psychic; Wasteland/End Dragon; Abyss Dark. Unknown fallback UNKNOWN.

Baseline before edits: unchanged32 broad traces replayed on preceding D02 build,
plus statitem/trainerswitch/Growth/PsychUp and H as D02 validation records.
Additional typeboostdiff/fielditemdiff2000×4 seeds each pass before these edits.
Same trace hashes are retained for final replay; all established tests unchanged.
H before:1000 ended each,clear0/stuck0/crash0/timeout0,50406 turns,paired176e408e,
14701 saves; host200-run pair4a43b96f. This is host consistency.

## Implementation and caller comparisons

Generic generated CopyBiomeTypeAttr leaf uses the pinned full map and existing
pr_set_summon_types/pr_arg_type/message APIs; no move-ID dispatch or generated
file edits. Source replaces the one-type override and preserves addedType;
C storage is already able to represent that bounded behavior. Natural fusion,
Tera, doubles and broader display/assets are excluded. Rendering/tween/party
UI, EXP and asset refresh are headless fixture boundaries; actual move factory,
parent method and turn phases execute independently and are never replaced.

Seeds777/9001/4242/31337: requested/completed/covered/identical2000 each, zero
skips/throws/parse/stat/state/text differences/untranslated messages. Each has
1300 attribute applications,0 leaf refusals,700 turn-gated Taunt/sleep refusals;
35 defined biomes all occur56–58 times, terrain counts420/420/420/390/350;
states600/350/350/350/350, sides1000/1000. States are mono/dual/previous override
with added Ghost/Taunt/sleep. All18 ordinary damage types occur8–966
per seed; the probe move is selected from the actual source holder's post-turn
first type to exercise STAB for each successful mapped type and compare actual getAttackDamage against pr_get_attack_damage,
including subsequent STAB/type-effectiveness and both RNG states.

Additional128 ordinary switches per seed,512 full state/text/damage comparisons
all identical. Source first executes actual Camouflage on the player with a
previous two-type override and added Ghost, then ordinary Pokémon switch/reset;
input and output include whole party and removed override/added type. An earlier
switch corpus did not set added Ghost; it is retained, not the final reset proof.
Three additional direct actual attribute probes (not legal whole-turn inputs):
unknown biome-1, unknown terrain255, and HP0 parent refusal. All3 C dispatcher
method comparisons match, including unchanged target, retained added type and
queued message count. These are not counted in the8000 caller comparisons and
do not claim full rendered fallback text parity.

Owning transport guard requires header/order/completed count/footer, full two
snapshots, one command per side, legal type/biome/terrain enums and numeric lists,
TRACE/TEXTS/QUEUE/damage probe and exact bounded stop phase.47 malformed main
traces refuse exit2;5 malformed helper traces (including integer overflow) also
refuse exit2. The wrapper enforces pinned SHA/Node22, bounded integer inputs,
nonempty unique seeds, unique retained output paths and original error statuses.
Existing Growth producer/comparator stay unchanged; anchored build overlays
observe zero overrides and add the subsequent actual damage probe.

Real C mutations, seed777 N2000: Forest/Jungle incorrectly Grass causes16 state/
text and16 later damage failures (exit1); ignoring terrain precedence causes960 state/text failures
and960 later damage failures (exit1). Intact2000/2000,exit0, and final clean8k+
512 replays pass. Mutations use temporary copied builds, not changed expectations.

Development failures retained: initial build used the wrong local terrain enum
prefix; initial damage-probe transport rejected the real NO_EFFECT result7;
first helper called getAttrs with a class instead of the pin's class-name string.
These were new-fixture/implementation development errors, not passing evidence.
The current generated enum bounds and actual pinned getAttrs API are used.

## Before/after regressions

All32 broad baseline source traces and seven focused suites at four seeds are
replayed unchanged on the proposed library:60 actual replay files, each2000.
Focused suites: stat-item, trainer-switch, Growth, type-boost, field-effect,
PsychUp and AverageStats. All focused56000 comparisons pass. Broader coverage
retains or increases; no new failure or missing-text count.

| Mode/seed | D02 baseline covered/pass/fail | Final covered/pass/fail | Missing text |
|---|---|---|---:|
|cond-31337|1768/1768/0|1768/1768/0|0|
|cond-4242|1775/1775/0|1775/1775/0|0|
|cond-777|1768/1768/0|1768/1768/0|0|
|cond-9001|1759/1759/0|1760/1760/0|0|
|heal-31337|1948/1948/0|1948/1948/0|0|
|heal-4242|1924/1924/0|1925/1925/0|0|
|heal-777|1935/1935/0|1935/1935/0|0|
|heal-9001|1930/1930/0|1930/1930/0|0|
|items-31337|1948/1948/0|1949/1949/0|1|
|items-4242|1933/1933/0|1934/1934/0|2|
|items-777|1930/1930/0|1931/1931/0|1|
|items-9001|1937/1937/0|1937/1937/0|1|
|plain-777|1948/1948/0|1949/1949/0|0|
|rich-777|1841/1841/0|1841/1841/0|0|
|stat-31337|1893/1893/0|1893/1893/0|0|
|stat-4242|1892/1892/0|1893/1893/0|0|
|stat-777|1884/1884/0|1885/1885/0|0|
|stat-9001|1890/1890/0|1890/1890/0|0|
|status-31337|1974/1974/0|1974/1974/0|0|
|status-4242|1971/1971/0|1972/1972/0|0|
|status-777|1966/1966/0|1967/1967/0|0|
|status-9001|1971/1971/0|1971/1971/0|0|
|summon-777|1867/1867/0|1867/1867/0|0|
|switch-777|1890/1890/0|1890/1890/0|0|
|trainer-31337|1941/1941/0|1943/1943/0|0|
|trainer-4242|1935/1934/1|1937/1936/1|0|
|trainer-777|1940/1940/0|1940/1940/0|0|
|trainer-9001|1938/1938/0|1938/1938/0|0|
|weather-31337|1966/1966/0|1966/1966/0|0|
|weather-4242|1957/1957/0|1957/1957/0|0|
|weather-777|1957/1957/0|1958/1958/0|0|
|weather-9001|1960/1960/0|1960/1960/0|0|

Known trainer4242 CASE1051 remains FAILED, optional queue index transport exit1.
ITEMS strict text completeness remains FAILED1/1/2/1. Historical legacy damage
coverage2000/2000/2000/1999/FusionFlare throw is retained and not rerun for this
bounded type leaf. No established failure was hidden or repaired.

H final:host passes; normal/save1000 runs each end1000, clear0/stuck0/crash0/
timeout0,50406 turns,pairedb8b28c8b,14701 saves,max11472 bytes;200-run host pair
4a43b96f. Before paired176e408e. Newly executing Camouflage replaces summon
state and unported accounting, both included in the run digest. No chooser/
digest rules changed. This is host consistency, not a legal Classic clear,
whole-run parity or hardware execution.

Reproduction:

```sh
. /workspace/shared/activate-parity.sh
battle-core/tools/camouflage-diff.sh /workspace/shared/upstream-lane-a 2000 '777 9001 4242 31337'
CAMOUFLAGE_SWITCH=1 battle-core/tools/camouflage-diff.sh /workspace/shared/upstream-lane-c 128 '777 9001 4242 31337'
CAMOUFLAGE_HELPERS=1 battle-core/tools/camouflage-diff.sh /workspace/shared/upstream-lane-f 20 '777'
TURNTEST_LOCALES=<upstream>/locales/en python3 battle-core/tools/camouflage-malformed.py battle-core/build/camouflage-test <main-trace>
TURNTEST_LOCALES=<upstream>/locales/en python3 battle-core/tools/camouflage-mutations.py <main-trace>
make -j2 -C game-core test
game-core/build/gcplay --auto 1000 --jobs 8
game-core/build/gcplay --auto 1000 --jobs 8 --save-test
```

Raw evidence: `/workspace/work/batch-2026-10-11/` d03 logs and retained D02
regression/hash corpus. Final main/switch producer captures use the same current
source fixture (actual getTypes selects later probe, never C output). Earlier
rotating-probe and switch variants are retained as development evidence; the
final mutation counts above use the stronger STAB corpus. Additional helper
source capture predates that probe-only change and executes the identical direct
method path. No source expectations come from C.

|Final trace|SHA256|
|---|---|
|camouflage-evidence.rXAx9R/camouflage-2000-31337.txt|`c7bf46235fe9e00ec66bf8b666f4edc3e0af20b994fa7fbe47fd06ad50f92ebc`|
|camouflage-evidence.rXAx9R/camouflage-2000-4242.txt|`10a7329ba5adeecad54b61c93feb0364f49621004493146d6802a070ac1a2472`|
|camouflage-evidence.rXAx9R/camouflage-2000-777.txt|`a63072c88a4ac010f7e1122f93d16d0437fdf1c47c5d6d80383dd74c0fab2731`|
|camouflage-evidence.rXAx9R/camouflage-2000-9001.txt|`4de1609a52abfc4574e5e605da485db915c83c9e50104d4782fa2644d187bcce`|
|camouflage-evidence.NkCHwc/camouflage-128-31337.txt|`a45bf15ca0999361184bc3720b1a086611ad47fee4db3f9510d4b2e4c12b4478`|
|camouflage-evidence.NkCHwc/camouflage-128-4242.txt|`8925accab3217dc4cc817665ff85e57dff80418066943b5100ec62a54038df47`|
|camouflage-evidence.NkCHwc/camouflage-128-777.txt|`922ae7654fae15056d737912f8717bf62ee6b2a940a53080b04e75ce2e17ff2a`|
|camouflage-evidence.NkCHwc/camouflage-128-9001.txt|`5a7982d4847af8d281bbe16faaaeef412070973cb05da65bcc225610d4b7fcaa`|
|helper3|`e01b8e12a20d58cc4984b4252037cf83d5ef4ab8959096488f0d94300c8d637f`|

Final producer SHA256: `0865001e44a83969f1da7d3647029ee2dce7afcb0289870fbbd52eccfb495a4f`; helper producer SHA256: `5dd1ebf978e243be58d06399d51cc45547ac10de8bc7584a412fa46d2dc15a73`.
GCC14.2.0; Node22.23.3. Exact-head host/3DS CI pending.
