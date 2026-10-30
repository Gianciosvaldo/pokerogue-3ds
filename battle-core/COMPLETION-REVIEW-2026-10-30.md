# Battle-core completion review — 30 October 2026

Reviewed main `59ae8deac1c83d640e0d6954493b5738a92fadf9` against source
`8555c08c823b856cbec4eb99ca84ea52a955836d` and locales
`270ed0a2938b2c3e2e34e7de940e84aecadf548b`.

## Answer about the finish line

Broader source coverage and integrated Endless acceptance are the two remaining
umbrella workstreams. They are **not two small final checks**, and this review
cannot certify that no further implementation fixes will emerge. Identity/copy,
doubles interactions, lifecycle/save behavior and capacity classification belong
inside the source-coverage workstream; they have not been independently waived.
After both workstreams close, the integrated regression suite, actual ARM build
and final evidence/exception inventory are required for sign-off. Hardware
acceptance remains separate from source parity.

PR #492 passed all three CI jobs on its exact head and is now merged. PRs
#487–#492 must not be recreated: retained Tailwind sources, all 48 public arena
keys and the audited direct maximum-HP getter fixes now have their own evidence.

## Broader source coverage

| Area | Current evidence | Remaining decision/work |
| --- | --- | --- |
| Registered source inventory | Closure identities, constructor arguments and generator stability are retained. All 33 held subclasses have owner/evidence mappings. | Reconcile enabled move/ability attributes, conditions, tags, phases and public callers into one implementation/evidence/exception ledger. Registry-name misses alone do not establish missing mechanics. |
| Explicit unsupported and capacity paths | Fresh static collection records 305 candidate source lines. Held catalogue audit passes: bound 1,447 versus storage 2,048. Arena public-key bound 48 is implemented. | Classify actual branches as source-disabled/exceptional, invalid or missing context, externally supplied context, or an enabled valid-input gap. Candidate lines are neither distinct missing mechanics nor a completion percentage. A real missing path must not be converted to a no-op. |
| Source identity and field/party lookup | Tailwind now follows full parties. History/actor references, fusion evolution, Shedinja/TM sharing, IV sharing and Eviolite clones have later owner proofs. | Three battler-tag blocks still query only the field: Trapped/Octolock admission, Seed on-add and Substitute on-add. Native `BattlerTag.getSourcePokemon` searches full parties. Prove the relevant owners and classify bench/fainted/absent-source cases before changing guards. Missing-source Substitute can leave undefined native HP; it must not be silently mapped to a normal zero-HP doll. |
| Generation and owning callers | Ordinary wild, unfused boss, natural fused-boss/passive movesets and enemy fusion projection have separate retained proofs. | `gc_generate_moveset` still rejects trainer/challenge, fusion with learned-move form triggers, and certain explicit passive inputs. Determine whether each higher-level owner supplies equivalent behavior or actually falls back. Do not classify all trainer generation as absent merely from this helper's return value. |
| Interactions and persistence | Doubles callbacks, complete history queries, event delivery, selected copy/save and form/item owners are implemented. | Reconcile simultaneous faint/switch, suppression/passive, fusion/Transform, clone/IV mutation and phase-transition combinations against enabled source branches. Preserve later fixes when reading older exclusion lists. These are evidence obligations, not a claim that every listed interaction is broken. |

Concrete source references for the lookup review are
[Trapped/Octolock admission](src/battler_tags.c), Seed/Substitute on-add in the
same file, native `src/data/battler-tags.ts::BattlerTag.getSourcePokemon`, and
[the full-party helper](src/history.c). Generation admission is in
[gc_movegen.c](../game-core/src/gc_movegen.c).

The held-class and catalogue audits were rerun successfully for this review.
The 305-line collection is a discovery aid; semantic classification remains
unfinished. No new native counterexample for the field/party candidates is
claimed by this document.

## Endless: established evidence and remaining acceptance

| Boundary | Established | Still needed for closure |
| --- | --- | --- |
| Finite stock Endless waves | [Wave sweep](../run-core/ENDLESS-WAVE-SWEEP-VALIDATION.md): all waves 1–5,850 in both modes, 11,700 native rows and 245,700 comparisons, including caps, level/RNG generation, boss segments and lure/doubles chance. | Connect those rules to complete generated encounters. The sweep does not construct every party, form, moveset or held inventory, or execute whole runs. |
| Encounter construction | [Enemy fusion transport](../game-core/ENEMY-FUSION-PROJECTION-VALIDATION.md), [ordinary wild movesets](../game-core/WILD-MOVESETS-VALIDATION.md), [unfused bosses](../game-core/WILD-BOSS-MOVESETS-VALIDATION.md) and [natural fused-boss owner](../game-core/FUSED-BOSS-OWNER-VALIDATION.md). | Retain whole constructor/generation finish RNG, including rolls after moveset selection; reconcile post-moveset form and remaining caller contexts. The fused-boss proof explicitly separates the later boss IV roll. |
| Battle, EXP and progression | Wide stats/damage, EXP/candy requests, actual EXP-award phases, learning/evolution and selected owner/save paths have individual evidence. | Validate their actual chained ownership: generated encounter → battle → EXP/reward → executed learning/evolution → save/load → next encounter. An earlier request-only proof does not close execution; a later execution proof must also not be mistaken for absent work. |
| Display, saves and terminal flow | Wide values and legacy-save migration have proofs. [Terminal owner](../game-core/ENDLESS-TERMINAL-VALIDATION.md) already implements clearing at wave 5,850 and its game caller. | Include display and save/resume in the integrated high-wave acceptance chain in both modes, especially the 65,535/65,536 boundary and final-wave transition. Do not reimplement terminal clearing or old 200/255 caps already superseded by wide progression/EXP work. |

The next Endless acceptance record should identify its exact owner chain,
natural versus synthetic setup, seeds, supported numeric domain, source
exceptions, uncovered results and saved/unsaved comparisons. Source branch
inventory must justify coverage beyond the sampled chains; bounded tests alone
cannot certify every run or every RNG input.

## Ordered closure plan

1. Prove/classify the concrete field/party lookup candidates and remaining
   generation admission/fallback owners. Fix demonstrated in-scope differences.
2. Finish the enabled-source and unsupported/capacity ledger, reconciling later
   identity, doubles, modifier and persistence proofs. Explicitly name justified
   external prerequisites; do not use an ownership label to hide an enabled
   battle-core gap.
3. Execute the integrated high-wave encounter/battle/progression/save chain in
   both Endless modes and address its demonstrated differences.
4. Retain all prior covered IDs, exact states/queues/text/RNG and failure
   controls; run normal core/app/custom/save/football checks and the actual ARM
   build/stack checks; publish the final scope and remaining external limits.

Only dedicated Task63 remains excluded. No Windows builder is produced. There
is no full battle-core completion claim or defensible completion percentage yet.
