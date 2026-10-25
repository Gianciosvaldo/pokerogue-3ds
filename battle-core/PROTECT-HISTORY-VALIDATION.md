# Protect streak and seeded condition owner

Pins: upstream `8555c08c823b856cbec4eb99ca84ea52a955836d`, locales
`270ed0a2938b2c3e2e34e7de940e84aecadf548b`.

ProtectAttr counts the complete trailing run of SUCCESS entries whose moves have
ProtectAttr or are Quick Guard/Wide Guard. Use mode does not exclude an entry.
A different move or any other result stops the streak. Zero uses succeeds
without RNG; otherwise the actual battle RNG draws from `Math.pow(3, times)`.

The old core stopped at eight retained entries and separately rejected ranges
above signed int32. The corrected native comparison found 37,440 mismatches or
unsupported results before this change. A semantic counter now retains the
streak independently of the unchanged eight-entry tail. Missing legacy prefixes
remain unknown until a known stop/reset. Short raw histories and tails containing
a stop can answer exactly. Raw history counters changed without matching summary
state cannot claim completeness, including the last-nonvirtual selector summary.

The new Number-sized RNG owner retains Phaser's arithmetic order and independent
battle seed. It consumes RNG even when `3^times` becomes Infinity (at 647),
including the native NaN outcome from zero multiplied by Infinity. A zero random
fraction can still succeed for large finite ranges. There is no int32 cast or
forced large-streak failure, and the existing integer RNG path is unchanged.
The counter covers native JavaScript array lengths; it does not extend native
array capacity or resolve every other history consumer.

## Actual pinned observations

The producer captures native BattleScene battle RNG before GameManager creation
and restores it after natural startup. It calls actual on-field
Pokemon.pushMoveHistory, Move.applyConditions and phase-manager owners. No
production condition, history or RNG method is replaced.

The complete source catalogue contributes ten ProtectAttr IDs and twelve counted
history identities (including both guards). MAX_GUARD is unused/unimplemented
upstream; its raw history still counts because it has ProtectAttr. Its additional
fail-if-last condition runs in sequence 4, unlike the other moves' sequence 3.
The tests retain that exception; this change does not enable MAX_GUARD.

There are 1,224 histories: twelve identities, six use modes and lengths
0,1,7,8,9,19,20,33,34,40,100,645,646,647,648,65,535,65,536. Five stages observe the
initial streak, a failed protection, a successful Quick Guard, a successful
non-protection (Splash), and summon reset. All ten moves run under lazy seed
initialization, initialized source RNG, zero-fraction and tiny-positive RNG
states. All 244,800 native condition results and exact before/after battle RNG
states match; global RNG is unchanged. Another 24 native sequence-3 observations
use actual empty queues and actual queued MovePhase instances. Their outcomes
and unchanged RNG also match.

The first preliminary capture accidentally queried only sequence 3, which failed
before reaching the streak condition. It is not used as evidence. The retained
producer separately exercises sequence 4 and the actual sequence-3 gate. A
non-protection SUCCESS stage prevents an implementation that counts every
successful move from passing.

The compressed retained transport is lossless and includes a dictionary of actual
RNG state strings. It does not calculate expected outcomes or normalize records.
`tools/protect-history-diff.sh /path/to/pinned/upstream` checks pins and unchanged
production trees, regenerates the capture, requires byte-identical observations,
and reruns the core and codec owners.

## Codec, guards and failure controls

An optional named `<party>_protect` section stores the counter and completeness.
Existing fields retain their names and widths. Missing sections reconstruct only
what the retained tail proves. Duplicate sections/fields, wrong widths/counts and
nonboolean completeness are refused. Partial legacy history can be saved again,
then a stop makes the new streak known.

The codec comparison passes 6,120 roundtrips and 511,661 checks; all 244,800 native
observations are compared live and restored, including all three run RNG streams.
The direct guard suite has 46 checks for raw/legacy knowledge, all result classes,
resets and counts above 65,535. The prior last-nonvirtual proof remains intact;
its raw guard now explicitly starts with default empty-summary knowledge before
supplying a long raw history.

All 18 compiled controls fail as required: tail truncation, each guard identity,
virtual filtering, failed/unrelated move counting, 16-bit wrap, stale stops/reset,
false prefix knowledge, skipped Infinity RNG, wrong RNG stream, forced failure,
extra empty-streak RNG, omitted/forgotten saves and invalid schema acceptance.
All ten malformed transports are rejected. Existing native source cases and
normal app/custom/save/core/football and actual ARM/stack checks remain enabled;
only dedicated Task63 is skipped. No Windows builder or release is made.

This proves the shared condition/append/reset/codec paths and these native
boundaries. It does not establish all natural phase/alias/wave interactions,
other arbitrary history consumers, faint/revive capacity or full wide Endless.
