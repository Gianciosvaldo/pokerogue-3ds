# Revival Blessing callback and party selection

Pinned source `8555c08c823b856cbec4eb99ca84ea52a955836d`, locales
`270ed0a2938b2c3e2e34e7de940e84aecadf548b`, Node 22.23.3.

`RevivalBlessingAttr.apply` overrides the parent gates. The player callback
always queues its party phase, even when its separate move condition is false.
An enemy requires its actual trainer slot and a fainted non-boss party member.
It filters the entire party in order and selects through the user's battle RNG.
The singleton API call is observed; upstream does not advance battle RNG for a
range of one. Healing uses half permanent maximum HP, floored with minimum one.
Enemy status reset is queued, whereas the player choice resets turn data and
status immediately. Source messages differ: enemy affix versus plain player name,
both with the prompt flag.

Preserve the source's unusual enemy switch-removal predicate: pending switch
field index equals the selected **party slot**. Cancel the first cached move for
the revived Pokemon, including its history result, then queue the replacement
switch. Player doubles choices can queue a switch and ToggleDoublePosition(true).
The engine pauses and resumes the actual party phase. The game and app expose a
fainted-only picker with the source's hasEnergy refusal and no cancel option.
Enemy sprite centering is presentation outside this numeric frame's claim.

Reproduce with `tools/revival-blessing-diff.sh UPSTREAM 777`, then seed 2034.
Native battle RNG is captured before constructing GameManager and restored after
natural startup. Native registered callbacks, phase start and UI callback execute
unchanged; presentation stubs delimit UI and phase advance. The owning producer
streams its trace and runs with a 4 GiB Node heap. This changes output buffering,
not source inputs, case counts, comparisons or coverage floors.

The direct proof compares all 17,408 cases exactly through the real C move-effect
dispatcher and phase runner. It covers eight actors, 125-field snapshots, status
and turn data, trainer/boss flags, both RNG states and actual RNG call arguments;
complete queue levels, markers, dynamic cache order and phase payloads; formatted
text and prompt flags. Extra cases exercise maximum HP one, temporary HP overrides,
duplicate pending moves and living player parties with a false move condition.
Earlier valid raw protocols/captures are retained. Diagnostic attempts with an
incorrect source constructor argument or absent transport sentinel are excluded;
the source was rerun after correcting the harness.

The game host test passes 17 checks through the actual command/party interfaces,
including refusal, pause/resume and revived HP at legal save/load boundaries.
Normal game-core tests pass with both saved/unsaved run digests `28fe3f71`.
Both owning seeds match all 16,384 cases, with zero source exceptions and
zero unsupported cases. Each observes 8,192 successful native attribute callbacks,
4,096 actual player party phases and 4,096 enemy status reset phases. Wide
controls include 1,331 / 1,298 damage results above INT32, maximum
345,003,225,010 / 386,403,612,011.
All 31 compiled runtime faults and 20 malformed captures are caught, including
owning move dispatch, party pause and resume-choice faults. The player name
versus player affix mutation is equivalent for these native inputs; it is not
counted as caught. A wrong message recipient is tested instead.
All 32 cross-slice captures remain exact. Fresh normal app/custom/save
AddressSanitizer, UndefinedBehaviorSanitizer and LeakSanitizer checks pass,
including the unchanged 10,000-run and custom/save continuation floors.
The local 3DS build passes with the same devkitARM image and command as CI.
Both native suites and strict C replays complete successfully.
Retained IDs lose none versus the Meloetta parent and gain two: doubles
1,993/1,995/1,995; mixed 1,997/2,000/1,996/1,998.

The owning suite requires all 8,192 cases per seed, zero source exceptions and
zero unsupported C cases. Its first 4,096 cases use the actual trainer battle and
both sides' Revival Blessing; the remaining 4,096 retain wide damage, ally, spread,
faint and switch controls. Actual trainer names and trainer slots are transported.
Historical producers select healthy bench members for every party screen,
including Revival Blessing; their captured native callbacks therefore end without
reviving that selection. The retained comparator keeps that same input policy.
The dedicated `revival-turn-test` uses the new producer's fainted-member input
policy via a compile-time test-driver option. Both use the unchanged strict wide
comparison and coverage floors; neither rewrites expected outcomes or messages.
The native trainer selection API is observed and must remain unused in these
explicit-slot, non-damaging trainer cases. No outcome or message normalization is
used. An initial missing-trainer-input diagnostic and a source run that exhausted
the default Node heap are excluded; neither supplied a complete accepted proof.

These bounded callbacks and turns do not establish complete battle-core, natural
Endless progression, global challenge/save or arbitrary fusion/Transform parity.
The separate explicit enemy-Palafin deviation remains unchanged. Normal CI gates
remain required; only the dedicated custom-only Task63 check is skipped.
