# Wide battle-level storage foundation

Upstream pin: `8555c08c823b856cbec4eb99ca84ea52a955836d`.
Locale pin: `270ed0a2938b2c3e2e34e7de940e84aecadf548b`.
Source execution uses Node 22.23.3, unmodified tracked source, and additive test
producers. This change is a prerequisite for wide-level Endless, not completion
of Endless or battle-core.

## Behavior and schema

Pinned `src/field/pokemon.ts:342` stores the supplied numeric level; its new-mon
constructor stores that level as metLevel at line 428. `calculateStats` at line 1578
uses that level directly, with fusion and Spliced Endless base-stat handling at
line 1621. The C runtime instead stored level and met_level in unsigned bytes;
levels 256 and 65536 became 0 before stat calculation.

`pr_pokemon.level` and `gc_mon.met_level` are now int32_t. Both game-core
constructors retain their int level input, and the app EXP animation's temporary
level no longer narrows to a byte. Stat algorithms and run-core are unchanged.

The existing fieldwise version 2 save writer records the new four-byte sizes.
The reader zero-extends historical one-byte values only for the level field of
pokemon_fields and met_level of mon_fields. Unrelated field size mismatches keep
the established drop behavior. Negative levels/met metadata are rejected;
met_level 0 remains allowed for old saves that lacked that metadata. No save-point
policy, checksum validation or generic unknown-field policy is relaxed.

The committed hexadecimal fixture is an actual native old-writer save captured
at merge `815ceeee14f073c87c7fecbb1914f6eed968095a`, before any widening. Decoded
length 8582, SHA256 `a50e354c5cfb4ab44860726c344b9cd44c00fbca837d4bbe023f44c8087864be`.
Its generator and provenance are in game-core/tests/fixtures. It has not been
rewritten into a hypothetical old schema. The new reader loads party levels and
met levels 255/127 and enemy metadata with zero dropped fields, then resaves and
loads successfully. The new saves require a reader supporting the wide fields;
this does not add forward compatibility to older application versions.

## Actual source comparisons

`wide-level.producer.test.ts` calls actual calculateStats. It captures native
BattleScene.prototype.randBattleSeedInt before GameManager construction and
restores it after battle startup. No gameplay/stat calculation is replaced.

16,800 complete source cases match all six permanent stats and HP exactly, with
zero unsupported cases. Explicit levels are 1,199,200,201,254,255,256,257,511,1000,
65535,65536,100000,1000000. Cases cover twelve selected species, their actual
forms, both sides, all 25 natures, varied IVs, unfused/Pikachu-fused actors,
Endless/Spliced Endless, permanent/passive Wonder Guard, and fresh/zero/positive/
clamped HP. These are sampled combinations, not a Cartesian product or every
species/form/modifier interaction. Inventory, challenges and Transform are
outside this added stat matrix. Source and C each preserve their initialized RNG
streams; this deterministic stat API consumes no RNG.

An additive wide boss-score reader replays the unchanged existing boss-score
producer at its already supported LEVEL_MAX=1000. All 1,000 source cases match
both actual C capture/faint score callers: 2,000 numeric comparisons, zero
mismatches. 1,442 caller contexts still separately flag unsupported phase work;
these numeric score comparisons do not prove whole capture/faint phases.
The original producer, original 255-bound comparator and their contracts remain
unchanged.

ABI-consistent byte-level and 16-bit core rebuilds demonstrate runtime failures:

- Byte: 9,600 stat failures and 1,464 score mismatches.
- 16-bit: 3,600 stat failures, including levels 65536 and above.

The proof guard separately compiles missing level migration, missing met-level
migration and signed-byte extension faults; all are caught by the native fixture
host test. Twenty malformed stat captures are rejected, including overflow,
fractional/nonfinite values, invalid fields, missing footer and extra tail.

## Owning callers and retained coverage

The added game-core host fixture checks both constructors at all 14 explicit
levels, new save/load of party/enemy level and metadata, stored stats and EXP,
and real Splash commands resumed from original/restored first-command saves.
Command screens, HP, levels and all three RNG projections remain equal. Signed
invalid-save and historical-unknown-metadata controls are included. This is an
augmented command/save fixture, not naturally reached high-wave progression.

Unchanged retained source captures preserve the previous covered IDs:

- Three broad doubles samples: 1,937/1,928/1,950 exact, 185 unsupported remain.
- Acupressure owning matrix: all 8,194 exact.
- 8,000 mixed turns: 1,974/1,969/1,991/1,995 exact; 71 unsupported remain.
- Acupressure 16,384 API cases, After You 8,192 API cases and suppression 16,384 API
  cases remain exact.

Full game-core tests, normal app/custom/save tests and the 3DS build remain merge
gates; final results are recorded in the PR. The custom-only Task63 jobs are
excluded as requested. No run-core files are changed.

## Remaining work and limits

This establishes representation, bounded permanent-stat/score parity and save
continuity. It does not prove arbitrary int32 levels or JavaScript's full safe
integer range. Stats/HP remain int32; values outside their numeric range need a
separate design and proof. Wide damage calculations, natural encounter/EXP/
reward progression, all held modifiers and high-wave clears remain unproved.

Game-core still caps addExp at 200, candy rewards at 255 and EXP-to-next display
at 200. The app's separate save-slot preview header still clamps displayed levels
to 255; the full run save preserves wide values. Those limits require separate
source-backed changes. Doubles attention/redirection, Flame Burst, ally healing
and other callbacks remain open. No hardware/emulator acceptance is claimed.

Reproduce with tools/wide-level-diff.sh UPSTREAM, then
python3 tools/check-wide-level-proof.py build/wide-level.txt build/wide-score.txt
from battle-core. Run make -C game-core test for constructors, save compatibility
and all existing game-core checks.
