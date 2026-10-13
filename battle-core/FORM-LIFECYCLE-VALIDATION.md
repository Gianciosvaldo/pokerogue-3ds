# Indexed form state and quiet form changes

Source `8555c08c823b856cbec4eb99ca84ea52a955836d`, locales
`270ed0a2938b2c3e2e34e7de940e84aecadf548b`, Node 22.

`Pokemon.changeForm` updates the permanent form and clamps abilityIndex against
getSpeciesForm().getAbilityCount(). That count is three when a hidden ability
exists and two otherwise, including forms whose secondary ability is NONE.
The pin gives some Mega forms abilities in all three slots; Mega does not imply
a missing hidden slot. Incoming Mega/Max forms also end Terastallization and
clear its Stellar history before recalculating stats. Form changes preserve
existing HP according to the source calculateStats rule.

`pr_change_form_state` implements that core state portion. The existing quiet
phase, ordinary form paths, Mega helper and reward bridge use it. The owning
caller retains scene assets, seen/caught records and modifier inventory updates.
Battle return/start boundaries synchronize the changed index back to gc_mon,
which supplies evolution/reward/save metadata.

The source API producer calls the unchanged Pokemon.changeForm on randomized
registered multi-form species. Eight thousand cases (4,000 each, seeds 777 and
2034) compare permanent/effective abilities, indices, HP, all six calculated
stats, Tera state and Stellar history. Inputs include primary/secondary/hidden
indices, custom NONE/Wonder Guard/Ball Fetch or no custom override, temporary
NONE/Ball Fetch or no temporary override, both sides and zero/partial/full HP.
All 32 scenarios are required. Both source RNG streams remain unchanged.
These API cases have no fusion, Transform or enabled passive.

A separate eight-thousand-case producer executes actual QuietFormChangePhase
start/end, with invisible actors to avoid animations. It retains the real form
method, ability callbacks and Autotomize reset. Only assets/info/sprite-pipeline,
field-scale display, text display and phase transport are stubbed. It compares
the same core state, ordered form-change ability history and rendered English.
Stable NONE/Wonder Guard/Ball Fetch inputs isolate these semantics; this does
not prove all native ability gain/loss combinations or full battle turns.

That phase oracle first exposed duplicate NONE entries, primary-before-passive
history order and wrong Mega/Gigantamax/Eternamax text. The port now follows the
source Set's passive-first insertion and uniqueness, reserves the actual number
of new entries and supplies both preName and the new pokemonName when required.
The original 128-case failure corpus is retained in the execution workspace.

Twelve compiled API/phase mutants and two compiled integration mutants are caught, including disconnected quiet callers,
index clamping/counts, stale ability caches, wrong HP preservation, missing
Tera/Stellar reset, duplicated history and wrong/missing text. Eighteen malformed
form traces and four indexed-lookup overflow/wrap traces are rejected, including IDs/levels that would wrap through scanf's int
conversion. Complete headers, counts, state bounds, unique history entries,
hex text and terminal footers are mandatory. Unsupported cases cannot pass.

The unchanged weather-form producer also matches 2,000 actual trigger/callback
cases, including Forecast, Flower Gift and Ice Face and natural passive inputs.

Fourteen host integration checks exercise an actual internal turn with Mega
input, bridge synchronization and Tera reset; separately, retained Mega state
is saved and loaded at a legal pre-turn command boundary and summon-reset.
Existing mid-turn save refusal is preserved. The game-core command menu skips
Mega precommands and exposes no Mega choice; the integration test supplies the
existing internal engine input explicitly. This change does not add that UI.
Historical actors with unknown lookup metadata retain their existing caches.

Reproduce:

```
battle-core/tools/form-lifecycle-diff.sh /path/to/upstream 4000 777
battle-core/tools/form-lifecycle-diff.sh /path/to/upstream 4000 2034
make -C game-core form-lifecycle-host-test
```

Form eligibility/trigger selection, native gain/loss combinations, fusion and
Transform form transitions, Battle Bond predicates and Ash callbacks, Mega
item/form choice and the Max caller remain separate source-backed work. Scene
inventory updates with held modifiers need their own contexts. No complete
forms, battle-core, emulator or hardware parity is claimed by this matrix.
