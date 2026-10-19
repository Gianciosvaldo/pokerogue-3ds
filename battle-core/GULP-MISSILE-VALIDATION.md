# Gulp Missile and Cramorant forms

Pinned upstream: `8555c08c823b856cbec4eb99ca84ea52a955836d`.
Locales: `270ed0a2938b2c3e2e34e7de940e84aecadf548b`. Node 22.23.3.

The registered Surf move effect and Dive charge effect use GulpMissileTagAttr.
Its parent only checks the user's HP. Its override checks active Gulp Missile
and raw Cramorant species, chooses the tag from the rounded HP ratio, and returns
true even when addTag refuses the tag. There are no extra chance/condition gates.
The tag checks Surf/Dive, raw base form and absence of both Gulp tags. Ability
form changes select from raw form keys and current HP, independently of tag
presence; add/remove before queued forms execute can therefore queue duplicates.

Retaliation ignores its lapse-type argument. Underwater and Substitute retain
the tag. Otherwise the actual current MoveEffectPhase supplies the attacker.
Magic Guard blocks quarter-max-HP indirect damage but does not block the
Defense drop or attempted paralysis. Removal triggers the quiet form change.
The active(false) trigger also restores an inactive loaded form.

Native RNG is captured before GameManager construction and restored after
natural double-battle startup. The raw producer calls actual registered Surf
attribute/tag APIs and scene form triggers. Every actor's 125-field frame,
status/turn state, full ordered tags, static/dynamic queues and their context,
phase payloads, translated text and both RNG states are compared directly.
QuietFormChangePhase has a Pokemon reference and target form, and no native
battlerIndex; the C internal event slot is not a native constructor parameter.
Protocol 3 also compares actual queued MessagePhase.prompt.

All 33,536 raw cases across seeds 777 and 2034 match exactly with zero unsupported
cases. This includes 512 low-ID delayed getter cases (indices 0–3 resolve field
slots) and 256 actual queued status executions after a field/party swap. The
status message remains constructor-cached while its recipient resolves from the
live field slot. The comparison caught the old field-only delayed attacker
lookup and the wrong default message for bench index -1. Pre-fix binaries and
failure captures remain in the workspace.

Separate actual native probes confirm that DamageAnimPhase(-1) throws while
reading getBattlerIndex, and executing ObtainStatusEffectPhase
for bench index -1 throws TypeError while dereferencing doSetStatus. Queueing
those callbacks is defined and compared above. The exception probes are outside the
successful owning-turn matrix and is not counted as successful full-turn parity.

The first 8,192 complete owning turns (seed 777) match exactly, including text,
all required state and queues, with zero source exceptions/unsupported cases.
They execute 2,715 quiet forms, 397 status phases, 512 stat phases and 1,024 Dive
charge phases, retaining 4,096 wide-level controls. Actual damage exceeds int32
in 1,323 native calls, up to 386,403,612,011. The second seed also passes all 8,192 complete turns with zero source exceptions
or unsupported cases. Its 2,761 quiet forms, 397 status phases and 506 stat phases
are compared exactly. An additional matrix covers actual prior-turn Dive continuation and
delayed bench-caster boundaries.

An unprotected bench-caster diagnostic attempted 8,192 turns and recorded 2,048
native exceptions (DamageAnimPhase(-1)); only 6,144 records were written and no
completion footer was emitted. That failed matrix is preserved and excluded
from successful complete-turn counts. The C engine now explicitly refuses the
same missing-recipient damage/status phase boundaries instead of claiming a
faithful turn. A separate matrix with actual Magic Guard bench casters tests
the defined path without a damage animation. Set GULP_UNPROTECTED_BENCH=1 to
reproduce the failed diagnostic; its exception/coverage gate must fail.

All 17 compiled production faults are caught and all 12 malformed captures
rejected, including the missing-recipient refusal. The real game host passes 72 checks for legal Surf/Dive commands,
ingestion, form execution, retaliation and legal save/load continuation. The
host automatically executes Dive's forced second turn. Normal game-core tests
pass with saved/unsaved digest e69b03a1. All battle runners build and the local
CI-equivalent devkitARM 3DS build passes. Seven retained suites lose no previously
covered IDs and gain eight exact cases. All 40 cross-slice captures also pass after the final exception-boundary guard.
Fresh normal app/custom/save sanitizer tests passed with their 10,000/100/200/100
run floors. The final incremental rebuild and all normal app/custom/save sanitizer checks
also pass after that guard; normal game-core and local 3DS checks pass again.

Reproduce raw and owning checks using `tools/gulp-missile-diff.sh UPSTREAM SEED`.
The additional continuation builder is
`tools/build-gulp-missile-continuation-proof.py`; run its generated native test
with DOUBLES_N=8192 and compare using build/wide-combat-test.

These bounded comparisons do not establish full battle-core, arbitrary Endless
progression or hardware parity. Preserve explicit deviations and all normal
merge gates; skip only the dedicated Task63 check.
