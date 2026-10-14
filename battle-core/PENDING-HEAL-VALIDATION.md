# Stored Healing Wish and Lunar Dance

Target: source `8555c08c823b856cbec4eb99ca84ea52a955836d`, locales
`270ed0a2938b2c3e2e34e7de940e84aecadf548b`, Node 22.23.3, GCC 14.2.

The native sacrifice effects now store a pending heal in the user's field slot.
Effects deduplicate by position and move, preserve their source identity and
message, and remain until an eligible recipient enters. Healing Wish restores
HP/status; Lunar Dance additionally restores effective moveset PP. PostSummon
applies the effect after resetting Toxic's counter and before entry hazards.
An empty pending tag expires at arena lapse.

Pinned details are intentional: simulation reports eligibility without consuming
or validating the source; real application discards missing sources and tries
the next eligible effect. Full HP with only spent PP needs Lunar Dance. The
effect is consumed when it queues the heal, including if Heal Block subsequently
prevents healing or the recipient is inactive. Source Ally Switch does not call
this entry effect; this port does not add an absent source caller.

Sixteen bounded records fit the native four-position/two-move state. An overflow
reports the engine limit instead of wrapping or dropping effects. Save records
use named scalar fields, without struct padding, in a separate optional section.
The arena's saved count requires that section for nonempty effects. Loading checks
counts, required fields, duplicate position/move pairs, moves, positions, PP flags
and frozen message identities. Ordinary older saves default to no stored effects;
the previous implementation could not create complete healing effects to recover.

Validation:

- Actual PendingHealTag, arena lapse, PostSummon, PokemonHeal and ResetStatus:
  8,192 cases each for seeds 777/2034 in singles and natural doubles. They compare
  stored records, source validity, simulation, duplicate requests, pristine,
  damaged, Toxic, PP-only, inactive and Heal Block recipients, all four field
  positions, phase parameters/messages and both RNG streams. The final V3 matrix
  strengthens eligible missing-source cases; all 32,768 final cases match exactly.
- Complete ordinary singles turns: 8,000/8,000 exact, 2,000 each at
  seeds 777/9001/4242/31337. Each seed executes 1,000 sacrifices/faints/replacements,
  625 healing phases and 250 status-reset phases; enemy single-member parties
  exercise the source's own-party refusal. Remaining stored effects and their
  frozen messages are included in the owning state comparison. Existing
  comparison and tolerances remain unchanged; no requested turn is skipped.
- Another 2,000 actual following turns at seed 4242 match exactly, including
  before-states with stored effects and their frozen messages. This bounded
  fixture leaves the original native source identity unchanged; arbitrary frozen
  strings and identity changes are separate representation contexts.
- Eight compiled behavioral faults are caught and twenty-one malformed captures
  rejected. The first missing-source mutant survived because the invalid first
  Healing Wish faced only spent PP, so it could not activate. V3 uses damaged/
  status recipients, and catches the same mutant without changing expectations.
- Host commands, legal saves and biome cleanup: 166 checks pass. Actual sacrifice,
  faint pick and entry cover health/status/PP restoration and pristine retention.
  Actual KO/reward continuation reaches a legal wild save with its effect intact;
  no can-save flag is forced. Three saves each reject six checksum-correct
  corruptions of positions, moves, PP flags, names, required fields or section.
  Ordinary saves also load when the new optional section is absent.
- Full game-core suite passes, including paired save/no-save digests `f44fb2f0`.
  All 24,000 mixed retained turns, 72,000 dedicated ability turns and 16,000 Ditto
  cases pass with no coverage loss. Item seeds 9001/4242/31337 each gain one
  covered turn; trainer seeds 777/9001/4242/31337 gain 3/2/7/3 respectively.
- Full app host suite passes, including 10,000 ordinary runs, ordinary saves,
  custom runs and custom slot resumption; no gameplay or save errors reported.
- Host and 3DS CI are separate publication gates; see the PR check results.

The initial API matrix coupled trigger mode to recipient status, so it did not
prove status restoration. The final matrix varies those independently and runs
actual ResetStatus phases. A missing wave seed initially marked Heal Block fixture
setup unsupported; the final capture supplies the actual source wave seed/turn.
No source gameplay method or formula, expected result, retained exclusion or
tolerance is replaced. Additive producers stub only UI/asset/timing transport.

Reproduce:

```sh
battle-core/tools/pending-heal-diff.sh /path/to/pinned-upstream api 8192 777
battle-core/tools/pending-heal-diff.sh /path/to/pinned-upstream double-api 8192 777
battle-core/tools/pending-heal-diff.sh /path/to/pinned-upstream turn 2000 777
battle-core/tools/pending-heal-diff.sh /path/to/pinned-upstream follow 2000 4242
# Repeat API seed 2034 and turn seeds 9001/4242/31337 using Node 22.
TURNTEST_LOCALES=/path/to/pinned-upstream/locales/en \
  python3 battle-core/tools/check-pending-proof.py /abs/pending-api.txt /abs/pending-turn.txt --follow /abs/pending-follow.txt
make -C game-core test
ASAN_OPTIONS=detect_leaks=0 JOBS=2 game/tests/run_host_tests.sh
```

LeakSanitizer is disabled only for the local ptrace environment; CI sanitizer
settings stay unchanged. Natural doubles here cover the API and owning entry
dispatcher, not complete doubles move turns. Arbitrary custom heal messages,
frozen identity changes, fused source display names, full doubles, Commander
and other remaining core slices are separate.
No run-core change, emulator or hardware validation is claimed.
