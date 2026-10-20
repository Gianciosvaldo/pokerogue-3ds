# Synchronize encounter nature

Pinned upstream `8555c08c823b856cbec4eb99ca84ea52a955836d`, locales
`270ed0a2938b2c3e2e34e7de940e84aecadf548b`, Node22. Native battle RNG is
captured before GameManager construction and restored after natural startup.
Gameplay RNG is not replaced.

## Behavior and state ownership

SyncEncounterNatureAbAttr calls target.setNature(user.getNature()) even in
simulation. Generic ability gates still apply; the attribute has no ability
banner. Nonsimulated applications still record summon and wave history.
Native getNature prefers a custom/Mint nature. Native setNature updates the
underlying nature and recalculates stats, retaining any custom nature override.
Thus a custom target can change its underlying nature without changing its
effective nature or its calculated stats.

Battle-core stores effective getNature in pr_pokemon.nature. Game-core already
stores the underlying nature and custom flag in gc_mon, with explicit save
fields. An optional transient nature setter in pr_ab_params preserves that
ownership. The core default sets an unoverridden nature and recalculates stats;
a scene with custom nature state supplies its setter. The actual game setter
updates underlying nature while retaining an effective custom nature, then
recalculates stats. No Pokemon/save layout or save schema is added.

The public encounter helper dispatches for the new field's first one/two player
party slots in reverse order. It includes recalled and switching players. The
game caller invokes it after actual wild construction, before enemy completion
and the first save point, using canonical party order and the **new** battle's
field size. Constructor nature rolls remain consumed. Trainer, loaded,
mystery-encounter constructors and explicit unsupported stand-ins do not gain
this callback. The existing double-as-single game projection remains reported.

## Exact bounded evidence

Both seeds, 777 and 2034, match all 204,800 direct dispatcher cases: eight user
and target roles, all 25 underlying natures, independently supplied user/target
custom natures including Hardy zero, primary/passive/duplicate/NONE-primary
abilities, fainting, suppression, switching, physical removal and simulation.
Levels include 1, 50, 256 and 1,000,000. The real game setter is used for custom
targets; unoverridden targets also exercise the core default setter.

Every case compares eight complete actor frames, stats, types, ability history,
ordered queues/text and both RNG streams. The frame's nature field explicitly
means effective getNature. Additional per-actor NI/NO rows compare **all three**
underlying, custom and effective nature values independently before and after.
Underlying state is not dropped or normalized into the effective nature.
Each seed records 76,800 applications, 74,400 underlying changes, 37,200
simulated changes and 40,800 changes to custom targets' underlying natures.
Actual pre-port ab_attrs fails 76,800 of 102,400 first-seed cases.

Both actual EncounterPhase.start fixtures match all 3,072 post-constructor
callback groups, 1,536 per seed from 1,024 encounter phases. A passthrough
observer captures the state before and after the native reverse lead sequence;
it does not replace that sequence. Each seed executes 1,984 native attribute
applications. Constructors use four configured species and levels through
1,000,000. Custom target setup happens before the observed boundary. Asset
loading is left pending and UI updates are presentation stubs, so later
encounter continuation is not executed. This is a real owner-prefix proof,
not a claim of complete generation or encounter-controller acceptance.

The real game caller passes 4,429 host checks across 512 natural new-battle
encounters (232 singles, 280 doubles), with canonical and reordered parties,
primary/passive gates, old/new field sizes and constructor nature observation.
Legal command/save/load checks retain the synchronized nature. A simulated
callback into a target with a custom nature preserves its effective nature,
stats and RNG while changing and saving its underlying nature.

Both encounter callbacks also project actual challenge-rule context before
passive/stat checks. An initially empty Challenge run previously recorded a
false unsupported path. The new first-encounter regression retains the real
Synchronize effect without that report. Actual unknown/selected challenge
limitations remain visible. Both seeds additionally match 3,072 native
Challenge encounter-prefix groups with all challenge values explicitly checked
zero. These retain the original Classic groups as separate required matrices.

Native wave history resets later in the encounter phase. The implementation
retains that lifecycle rather than inventing permanent callback history.

## Gates and limits

Twenty compiled implementation faults and eighteen malformed-input checks
pass on the final implementation. Normal game-core tests pass with
identical saved/unsaved digest 3fc2f6ff. The final normal app/custom/save sanitizer
suite passes (10,000 normal runs, digest 9c223fca), with no model violations,
stuck runs or screen/run mismatches; the 3DS build passes. Forty-two cross
matrices, twenty-six supplemental matrices, entry/context checks and eighteen
latest callback/generation matrices pass. Seven historical corpora retain every
previously covered case ID. Only dedicated custom-only Task63 checks are skipped.

The run-core archive used by the double-generation runner now depends on shared
battle headers too, so transient callback-layout changes trigger its rebuild.
A disk-space failure before the initial app build ran is preserved; the full
two-job sanitizer run passes after freeing space. Large source captures were
losslessly compressed and verified; cases, state, queues and RNG are retained.

Reproduce with `battle-core/tools/sync-nature-diff.sh PINNED_UPSTREAM SEED`.
These bounded cases do not close all stat-modifying inventories, fusion,
Transform, custom challenge combinations, complete game doubles, the remaining
callbacks, final source inventory or wide-level Endless acceptance.
