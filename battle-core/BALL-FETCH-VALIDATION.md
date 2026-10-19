# Ball Fetch inventory and once-per-battle history

Source `8555c08c823b856cbec4eb99ca84ea52a955836d`, locales
`270ed0a2938b2c3e2e34e7de940e84aecadf548b`, Node 22.23.3.

FetchBallAbAttr requires a player, a non-null last failed ball and a non-simulated
call. Its registered condition also refuses an ability already in the holder's
waveData.abilitiesApplied. Apply increments that global inventory entry without
any reward cap, clears lastUsedPokeball and queues the translated holder/ball
message. The real dispatcher surrounds it with ability display and records the
ability in wave and summon histories. Primary, passive and second-holder ordering
use the same shared last-used value.

The port previously reported inventory unsupported. Its condition already called
pr_wave_ability_applied, but that helper still returned false despite real wave
history now being stored. The native comparison exposed that stale stub in 388
cases. Read the existing history and implement the inventory/message callback.
Battle inventory has an explicit known-input flag and JavaScript-number counters;
missing input stays unsupported. The game synchronizes its inventory before and
after actual battle calls, after the command spends a thrown ball. A returned ball
cannot exceed its pre-throw count. The run's existing byte/save representation
therefore remains valid, including tested count 255; invalid bridge states are
reported instead of silently truncated. This does not clamp the raw callback.

Reproduce `tools/ball-fetch-diff.sh UPSTREAM 777`, then 2034. Native battle RNG is
captured before GameManager and restored after natural double startup. Actual
registered applyAbAttrs executes twice, on the same holder or its buddy. All
32,768 raw cases match exactly, including full 125-field frames for four actors,
wave/summon history, eight global counts, complete queued phase payloads, text,
and both RNG states. Coverage includes primary/passive/explicit selection,
simulation, fainting, suppression, prior wave use, absent/all six named ball
values and counts from zero through 2^53-1. Each seed has 1,164 actual applies,
194 explicit-passive applies and no second successful return.

All 16,384 complete owning turns match exactly with zero source exceptions or
unsupported cases. Each seed has 4,096 actual native applications; remaining
controls retain wide damage, ally/spread, faint and switch floors. Source traces
carry the actual global ball counts before and after each turn. These turns
start with a last-used ball from a previous failed-throw state; they exercise the
actual PostTurn owner, not a native capture attempt. The separate game host test
executes real failed throws and passes 60 checks: four throwable ball types,
first return, second refusal, wave reset and legal inventory save/load.

All 17 compiled faults and 19 malformed captures are detected. The simulation
fault uses the live scene engine to test its wrong state/message outcome rather
than crashing on a deliberately absent simulated display engine. Normal game-core
tests pass with saved/unsaved digests both `e69b03a1` on the pre-integration base.
Normal app/custom/save checks, retained/cross-slice replays and local 3DS build
are still pending, as is integration onto the newer main changes.

These bounded source callbacks/turns and the real game bridge do not establish
full battle-core, arbitrary capture/run progression or hardware parity. Preserve
all explicit deviations and the normal merge gates; skip only dedicated Task63.
