# Primary and secondary level-up evolutions

Reference: upstream `8555c08c823b856cbec4eb99ca84ea52a955836d`, locales
`270ed0a2938b2c3e2e34e7de940e84aecadf548b`, Node22.

A fusion whose primary species has no eligible evolution must also check its
secondary species. The level-up caller previously checked only the primary.
For example, a Mew/Gimmighoul with the ready canonical tracker was eligible to
evolve its secondary half upstream but received no evolution request here.
The previous source corpus retained 272 ready secondary observations without
crediting them as selected evolutions. All are now compared.

`gc_mon_get_evolution_target` preserves primary-first source order, then checks
secondary species/form and secondary gender. The level-up phase carries the
chosen half in its existing transient `d` field. Confirmation changes that
half's species, conditional form and ability index, then recalculates stats and
formats the new fused name. Gimmighoul cleanup removes the first owned tracker
when the selected pre-evolution half is Gimmighoul. Primary compatibility helpers
remain available to their existing callers. Enum IDs and the save format version
are unchanged.

The source's ability-count rule is retained: a species with no hidden ability
counts as two abilities, and a hidden index2 becomes index1 when evolving from
three to two. Fusion ability lookup returns a NONE entry directly; the primary
lookup's fallback is not applied to it. Concrete legacy ability-cache guards
retain that distinction.

Secondary gender was absent from battle state and reward projection. The new
`fusion_gender`/`fusion_gender_known` pair is saved as optional named metadata.
An older save with neither field loads with unknown secondary gender, without
a dropped-state report. Partial/duplicate/wrong-width metadata, nonboolean known
markers and known genders outside -1/0/1 are rejected. An unknown gender makes
an earlier gender-gated secondary candidate explicitly unsupported instead of
assuming the primary's gender or a default male. Other missing or mismatched
save fields keep their existing accounting and historical level widening.

## Native observations and owners

The producer captures native `BattleScene.prototype.randBattleSeedInt` before
GameManager construction and restores it after battle startup. It calls actual
`Pokemon.getEvolution` and `Pokemon.evolve`; asset loading, status UI and modifier
icons are presentation boundaries. Evolution logic, stat calculation, ability
lookup, tracker cleanup and both RNG streams stay native. No expected evolution,
stat, name or RNG result is calculated by the transport builder.

The retained matrix contains **1,440 cases** across ten species pairs, five
levels, three secondary genders, two ability indices and absent/ready trackers.
Both chest/roaming Gimmighoul forms and explicit day/night cycle offsets are
included. Bulbasaur/Gimmighoul tests primary precedence; Combee and Salandit test
secondary gender; Eevee tests the time-dependent choice; Trapinch/Vibrava tests
the three-to-two ability boundary. Caterpie and Trapinch additions preserve all
earlier native observations. Initial test-scene cycle selection varied between
runs; both observed cycle inputs and outcomes were retained explicitly, without
normalizing either result.

All before/after species/forms, genders, indices, effective ability, HP/stats,
IVs/nature/friendship, pause state, ordered stock, actual rendered fused names and
RNG states are compared. Selected evolution species/level/item/form keys and
primary/secondary identity are also checked. The C formatter receives captured
pinned English Pokémon names through a transparent test template, with native
`Pokemon.name` as the expected string. The raw capture and C include reproduce
byte for byte from a fresh native run.

The matrix has 192 primary and 640 secondary selections. Every before/after
snapshot passes a legal initial-command save/load with zero dropped fields.
All **832 selected cases** also run actual game-core level-up queue construction,
evolution screen setup and confirmation, then compare the native after frame.
These are actual C owning-phase checks, not a paired upstream whole-battle or
whole-run queue/AI proof. Raw save guards cover missing legacy metadata,
malformed schemas and an actual older PR399 save.

The existing Gimmighoul corpus still checks all 576 acquisition snapshots and
12 primary cleanups. It now checks all 3,456 evolution queries, adding all 768
secondary observations to the retained 2,688 primary/no-fusion observations.
Its seven compiled faults and eight malformed transports remain required.

```sh
. /workspace/shared/activate-parity.sh
bash game-core/tools/reproduce-fusion-evolution.sh /path/to/upstream /path/to/output
python3 game-core/tools/check-fusion-evolution-proof.py
python3 game-core/tools/check-evo-tracker-proof.py
```

The 162,167-check host replay passes. Fifteen compiled owner faults, thirteen
malformed captures and four well-formed forged observations are rejected by the
new checker. Compilation failures do not count as caught
faults. Scoped ASan/UBSan/LeakSanitizer instruments the changed mon, phase, save
and reward owners with ordinary optimized supporting archives; full-program
instrumentation is not claimed. Normal core/app/custom/save/football and actual
ARM build/stack checks remain required. Only dedicated Task63 is skipped.

## Remaining scope

This does not implement general DNA Splicers acquisition or fusion item-evolution
selection. Those owners must supply secondary gender where available; older
unknown metadata remains explicit. Nincada/Shedinja's extra-party creation,
every evolution condition/challenge/form/custom ability arrangement, graphical
evolution timing and full-run/AI acceptance remain separate. A passing bounded
matrix does not establish all fusion behavior or battle-core completion. No new
Windows builder is released.
