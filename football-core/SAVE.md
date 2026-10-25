# Football checkpoints

Football has a dedicated `football.sav` under the existing save directory, with
its own `.bak` and `.tmp`. Classic/Custom slots and the title's Continue slot are
not read, changed or deleted. Entering Football Playoff resumes its checkpoint;
backup recovery is reported before play continues. START offers Save and return,
or explicit deletion followed by return to title. Deletion requires confirmation.
Enter Football Playoff again after deletion to draft a new team.

## Save points and retained state

Automatic checkpoints occur between rounds, before snaps, in the shop, and at
the completed/eliminated screen. Active snaps finish before manual save; EXP,
evolution and move-learning choices finish before the next shop checkpoint.
Those transient phases cannot be manually suspended. Closing unexpectedly there
returns to the last checkpoint. Offensive and defensive playcalling menus reset on
resume. This is checkpoint persistence, not an exact animation-frame snapshot.

The file preserves regular roster identities, levels, EXP, moves, variants and
held stones using game-core's named Pokemon/item fields; independent roster and
season RNGs; round/wins/wallet; vitamin bonuses; selected mega holder; shop offers
and sold flags; and active match teams, effective base attributes, fatigue,
score, field spot, clock, down/distance, overtime, difficulty and match RNG.
Loaded match/team pointers are rebound to the new dedicated roster/session.
Mega forms remain match copies; the saved roster retains regular forms.

The new football roster codec never relaxes `gc_run_can_save` and cannot load as
a Classic battle save. Pending shared phase queues are not serialized: encoding
requires an idle roster with an empty event/phase queue. The app saves shared
profile changes through its existing save gate before committing the football
checkpoint. This is two existing save files, not a cross-file transaction.

## Format, memory and failures

The outer football format has its own magic, strict version, explicit little
endian integers, bounded counts, and CRC. It writes fields rather than pointers,
enum representations or C struct padding. The roster payload independently has
its own magic, version/API identity, CRC and named fields. Unknown/newer schema
versions are rejected. Outer version 2 adds runner/dive choices; version 1 match
checkpoints migrate to Balanced / Standard without changing their saved RNG. Home identity/form/attributes must agree with the roster,
mega choice and vitamin bonuses after load. All inputs are bounded before loops.

The existing `save_write` provides checksum-protected temp/backup replacement.
A failed write leaves the prior checkpoint available; the screen pauses and
offers retry or continued play with retry at the next changed checkpoint. Loading
never overwrites an unreadable file. Oversized or inaccessible files are rejected
before the shared reader allocates. Semantic failures preserve both files; an
explicit deletion is required to start fresh. The buffer is static, and the
dedicated roster remains one checked heap allocation, never a stack copy.

## Validation

`make -C football-core test` includes byte corruption and truncated-file rejection,
roster and active-match round trips, uninterrupted/resumed RNG/score/fatigue
replay, post-EXP shop stock and level preservation, sold-item guards, held stone
and mega-match restoration. File tests use the real atomic writer in a temporary
directory, force a failed temp write, recover a backup, preserve corrupt bytes,
and verify a Classic file remains unchanged. Screen tests cover autosave, resume,
failure/retry, protected corrupt files and confirmed deletion. Existing football
run/evolution/contact tests remain enabled. CI validates the 3DS build and stack.

Visual testing remains deferred to the next Windows builder. Check resume and
delete prompts, backup notice, save-error controls and resumed match sprites then.
