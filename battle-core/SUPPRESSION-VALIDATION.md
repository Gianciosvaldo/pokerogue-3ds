# Gastro Acid and Core Enforcer suppression callbacks

Base: After You merge `310b80bc23e88516f408cfb9d80f4faa6853e529` (#287).
Reference: upstream `8555c08c823b856cbec4eb99ca84ea52a955836d`, locales
`270ed0a2938b2c3e2e34e7de940e84aecadf548b`, Node22.23.3.

The raw `SuppressAbilitiesAttr.apply` calls its parent's HP gate, queues the
suppression message, runs passive loss callbacks before primary loss callbacks,
sets suppression and invokes weather-form reversion. Its own condition is a
separate query; applying the raw leaf does not check it. Core Enforcer's
`SuppressAbilitiesIfActedAttr` first calls its HP gate, then invokes the base
effect only if the target acted and the fresh base condition succeeds.

The shared condition checks existing suppression and raw primary/passive
suppressability flags, rather than current ability activation. A present NONE
passive still participates in that query. The existing Gastro Acid move
condition now shares the same helper, retaining concrete-cache compatibility.

## Actual native observations

`tests/suppression.producer.test.ts` records 16,384 complete cases: 5,462 raw
base effects, 5,461 conditional Core Enforcer effects and 5,461 condition-only
queries. The condition itself is additionally observed in every case. The
original BattleScene RNG method is captured before GameManager construction
and restored after battle startup. Core Enforcer's concrete native attribute
is obtained from its owning move; that child class is absent from the pin's
string-attribute registry. No gameplay method or expected result is replaced.

The matrix includes both sides, living/fainted and acted/unacted targets,
existing suppression, eleven primary and eleven passive ability choices,
present NONE passives, primal weather loss, Forecast/Flower Gift, Neutralizing
Gas counts/source ownership and actual recovery callbacks. It compares the
complete represented 125-field actor snapshots before/after/reset, source
condition booleans, ability eligibility, weather/gas/world state, exact phase
levels and dynamic queues, localized text and both RNG streams. All 16,384
cases match, with no unsupported cases. The C effect dispatcher has a void
interface; native effect returns are recorded, while boolean comparison is
performed for the exposed condition query.

The matrix exposed an existing PostSummon weather callback bug: the C code
scheduled ability display before its full canApply check and could schedule
it twice. The display now occurs once, only after the complete check. Restoring
that bug causes 81 mismatches in the unchanged capture. Ignoring a present
NONE passive causes 84 condition/state mismatches.

The guard tool compiles sixteen runtime faults, including omitted leaves or
HP/acted/eligibility gates, an incorrect condition gate on the raw base effect,
missing message/affix, omitted or reversed loss callbacks, setting suppression
too early or omitting it, missing weather reversion, NONE-passive handling and
premature ability display. All are caught; twenty-two malformed captures are
rejected. Removing suppression also causes twelve covered complete-turn
failures in the plain seed777 source sample.

## Owning turns and callers

The three actual source doubles captures from #286 remain unchanged. Each has
all 2,000 requested/written turns and zero source exceptions. The exact #287
baseline substitutes all three changed original runtime objects when linking
the same comparator; source choosers, comparisons, seeds/counts and exclusions
remain unchanged.

| Sample | Covered before | Covered after | Gained IDs | Lost IDs |
| --- | ---: | ---: | ---: | ---: |
| Plain seed777 | 1,905 | 1,937 | 32 | 0 |
| Rich seed777 | 1,905 | 1,928 | 23 | 0 |
| Plain seed2034 | 1,911 | 1,950 | 39 | 0 |

All 5,815 compared states and exact localized messages match. Covered-ID sets
prove zero loss; 185 turns remain explicitly unsupported. The retained 8,000
mixed turns preserve every covered ID: plain1,971→1,974, rich1,965→1,969,
summon1,991→1,991 and switch1,994→1,995. Acupressure's 16,384 API cases and
8,194 owning turns, and After You's 8,192 API cases, also remain exact.

The augmented singles host fixture passes 56 checks. Real fight commands
apply Gastro Acid/Core Enforcer suppression, with the enemy's action order
verified through actual battle events. It confirms that post-turn saves remain
forbidden. A separate real battle API projection at the permitted first-command
boundary round-trips suppression and all RNG states, then resumes real commands
on original/restored runs. Its starter moves and enemy ability are fixture
inputs; it is not a natural starter/run, arbitrary active-turn save or legal
clear proof. No save policy or serialized fields change.

The full game-core suite passes, including normal custom/save checks; paired
200-run saved/unsaved digests are both `d42eb77a`. Exact-head host, app/custom/
save and 3DS CI remain merge gates, with results recorded in the PR. Only the
dedicated custom-only Task63 workflow is excluded.

## Remaining boundaries

The direct matrix uses ordinary non-fused actors, no held inventory/status and
bounded levels. Queued quiet/stat phases are observed here; owning execution
is supplied by broader real turn samples and existing phase proofs. It does
not prove every fusion/Transform, challenge, doubles partner/command/save or
weather interaction, playable app doubles, wide-level Endless or whole-core
completion. Attention/redirection, Flame Burst, ally healing and other callbacks
remain open. No run-core changes or emulator/hardware acceptance are included.

Reproduce with `tools/suppression-diff.sh UPSTREAM`, then
`TURNTEST_LOCALES=UPSTREAM/locales/en python3 tools/check-suppression-proof.py
build/suppression.txt DOUBLES_TRACE` and the unchanged `build/doublestest`.
