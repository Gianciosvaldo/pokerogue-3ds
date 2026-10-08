# Multi Lens loaded inventory bridge — 2026-10-08

Pinned upstream `8555c08c823b856cbec4eb99ca84ea52a955836d`, Node 22.
Canonical loaded Multi Lens reaches the existing battle-core count and damage
helpers (#157), on both sides, with real stacks 1–2 and nonnegative transient
stacks totaling at most two; arg -1/0. Malformed records remain generic opaque,
so the helper cannot apply a ghost damage multiplier. Holder, wildcard -1 and
detached owner 0 remain distinct. Repeated reconciliation consumes each matching
modifier at most once; represented inventory is the existing 96-entry prefix.
Acquisition, transfer/restacking policy and whole source session migration are
outside this slice.

`tools/multidiff.sh /workspace/shared/upstream-lane-c 2000 SEED` for seeds 777,
9001, 4242, 31337 reuses the unchanged owning producer and comparator read-only.
All 8,000 full turns and rendered message sets match, with zero uncovered,
parse/stat differences or skipped cases. Each seed maps 1,000 items per side,
including 200 virtual-stack cases. Link observers measure actual helper changes,
not mapping visits or intrinsic multiple-hit moves: count gains 300/300 each
seed; damage multiplier changes 659/650, 651/645, 644/647, 660/645. Their sums
exactly equal source countApplied=600 and damageApplied=1309/1296/1291/1305;
the new script requires those source/core totals and nonzero actual effects.
Owner refusals, status/fixed/intrinsic multiple-hit/charge moves, faint/switch,
real and virtual stacks retain the source oracle's original scope.

An external actual gc_items.c mutant replacing its Multi class assignment with
a generic held class fails immediately (exit 1). A missing completion footer is
rejected (exit 2). No established producer, comparator or host check changed.
Two early runs of the newly added host fixture failed on assumptions about the
next command's reset hitCount and unsigned ternary conversion of wildcard -1;
these new assertions now observe the actual helper call and explicit int64 owner.
Those early failed runs are retained, and only corrected final replays are credited.

The additive host has 201 checks, including malformed records causing neither
count nor damage effects, ownership, prefix preservation, defeated detachment,
actual command/save/load/resume and opaque virtual reward contexts. Four real
pinned ModifierData reconstruction answers confirm transient stacks omitted/reset
on load and actual helper count/damage before/after; the host with storage has
253 checks. Existing zero-virtual stocks retain normal/save equivalence;
virtual-fixture equivalence is intentionally unclaimed.

`make -j2 -C game-core test` passes. All four established apply/candy/ball/options
outputs remain byte-identical; 28 existing Mint/vitamin/Money/Orb/Seed/Lens/Golden
source replays pass at their established counts. Before/after 1,000 normal and
1,000 saved runs are byte-identical: 1,000 ended, zero stuck/crash/timeout,
49,206 turns, paired digest `15d6d01d`; 14,291 save/load trips, maximum 11,400
bytes. Paired 200-run host digest is `2f376abe`. All printed stub and unsupported
rows are unchanged: random soaks do not establish this rare item's source effect.
No hardware/emulator claim; exact-head host and 3DS CI are required before merge.
Evidence: `/workspace/validation/lane-c/ten/multi/`.
