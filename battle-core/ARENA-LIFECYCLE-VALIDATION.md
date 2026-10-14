# Arena reset, Neutralizing Gas and Imprison

Pinned source: `8555c08c823b856cbec4eb99ca84ea52a955836d`; locales:
`270ed0a2938b2c3e2e34e7de940e84aecadf548b`. Producers require Node 22.
Tracked upstream gameplay methods, formulas and expectations are unchanged.

## Behavior

`pr_prepare_player_cleanup` now follows `Arena.resetArenaEffects` before Return
and party init: reset timed weather, reset timed terrain without animation,
reset player faint count, then invoke arena removal callbacks in list order.
Permanent effects (zero duration) remain. The arena list remains available until
all bulk callbacks finish. Game-core no longer clears weather before those
callbacks or repeats the reset afterward; weather restored by ability recovery
can therefore survive trainer entry.

Neutralizing Gas starts with one source. Activation and overlap use the actual
caster; loss callbacks and recovery callbacks follow speed order, passive first.
Removal exposes `beingRemoved` before ability recovery. A leaving source removes
the tag at zero, or reactivates the remaining carrier's other ability at one.
The existing tag serialization is preserved. Source-count arithmetic reports an
engine limit rather than wrapping an int8 counter outside its representable range.

Imprison now adds/removes battler restrictions, applies to grounded switch-ins,
lapses when its caster is inactive, and cancels matching effective moves before
execution with the source message. Restriction checks read the temporary moveset
when present. Its battler tag does not count down after each move. Upstream's
`getField(true)` filters living/challenge-allowed slots, without requiring them
to be on the field; the caster's sustaining predicate does require field presence.
Challenge eligibility remains an existing separate core prerequisite.

## Evidence

| Boundary | Complete source cases | Result |
| --- | ---: | --- |
| Gas arena add/overlap/source-leave/remove/bulk APIs, strengthened carrier recovery | 8,000, seeds 777/2034 | Exact state, predicates, world, queues/text and both RNGs |
| Gas primal-weather loss/recovery APIs | 8,000, seeds 4242/31337 | Exact; Heavy Rain/Harsh Sun with Primordial Sea/Desolate Land and Drizzle/Drought |
| Standard bulk removal | 8,000, seeds 777/2034 | Exact callback message order and empty arena |
| Imprison add/overlap/remove/bulk/lapse/PRE_MOVE/entry application | 8,192 | Exact restrictions, cancellation, message queues and global RNG |
| Actual owning encounter cleanup with timed/permanent effects and tags | 8,192, seed 4242 | Exact world, actor state, queued/executed phases, text and both RNGs |
| Owning cleanup including Drizzle restored by Gas removal | 8,192, seed 31337 | Exact; source queued animation is retained in queue/trace but presentation is skipped |
| Actual complete Gas singles turns, including carrier KO and switch/faint recovery | 8,000, seeds 777/2034 | Every turn covered; exact state/RNG/text with unchanged comparator/tolerances |

The initial 8,000 Gas API captures remain retained as an additional regression.
The strengthened matrix gives a carrier Intrepid Sword as its other ability,
in both primary/passive arrangements: omitting last-carrier recovery must fail.
All **12 compiled faults** are caught and **16 malformed fixtures** rejected.
Retained 24,000 ordinary turns have no covered mismatch or loss; summon seed 777
increases from 1,897 to 1,914. The 72,000 dedicated prior ability turns and 16,000
Ditto phase/turn cases remain unchanged and pass.

The full game-core host suite passes. An additional **78 checks** exercise legal
arena saves/load, biome encounter cleanup, and the trainer cleanup/arena bridge
with restored Drizzle. The trainer checks call the actual bridge directly; they
do not claim a run-core generated fixed-trainer encounter. Whole-run/save paired
host digests remain identical (`f44fb2f0`). The full app suite passes, including its 10,000-run soak (119,420 waves,
485,135 commands), system-data saves, ordinary custom runs and custom resume.
Exact-head host/3DS CI is recorded in the PR once complete. No emulator or hardware proof is claimed.

## Reproduction

`tools/arena-diff.sh UPSTREAM MODE [N=8192] [SEED=777]` supports `gas`, `weather`,
`bulk`, `imprison`, `cleanup`, and `turn`. It checks source/locales pins and refuses
tracked upstream changes. Source producers use ordinary framework initialization;
asset/audio/UI/timer stubs provide presentation transport, without replacing
battle formulas or gameplay methods.

After generating fixtures, run `tools/check-arena-proof.py GAS BULK IMPRISON
CLEANUP` with `TURNTEST_LOCALES=UPSTREAM/locales/en` to compile faults and validate
transport refusals. `make -C game-core arena-cleanup-host-test` runs save/bridge
checks; `make -C game-core test` runs the ordinary host suite.

## Limits and excluded drafts

Four-actor Gas API coverage does not establish complete doubles turns, Commander,
redirects, all fused/Transformed form-loss interactions, or Challenge eligibility.
The standard bulk matrix excludes NONE, Gas, Imprison and Pending Heal; Gas and
Imprison have separate proofs. Pending Heal's entry/lapse prerequisites remain
separate work. Imprison entry API coverage uses grounded ordinary species; broader
levitation/field mechanics retain their existing tests and prerequisites.

An initial Gas quiet-control draft used an ignored extra argument to source
`removeTag`; it was replaced by the actual `removeTagOnSide` API and retained
separately, without changing expected output. An initial Imprison draft left a
framework moveset override active while transporting permanent moves; it is
excluded. Final producers clear that override and compare effective restrictions.
An initial weather replay guard incorrectly used FOG as the enum maximum; the
guard was corrected to STRONG_WINDS, without changing source expectations.
The first recovery mutation exposed a fixture with no observable other-ability
effect; the strengthened actual-source fixtures catch that omission.

Local app LeakSanitizer cannot run under ptrace. The successful local rerun uses
`ASAN_OPTIONS=detect_leaks=0`; AddressSanitizer/UBSan checks remain active. CI uses
its normal sanitizer configuration. The custom-core/Task63 workflow is excluded
per the user's standing instruction; ordinary custom-run/save app tests remain
required. No run-core source is changed.

Next: Stance Change's registered quiet pre-move/leave-field transitions, followed
by native form/fusion/Transform and suppression interactions, then remaining
partner and arena prerequisites. This document does not claim complete battle-core parity.
