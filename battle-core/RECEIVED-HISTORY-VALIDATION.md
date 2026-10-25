# Received-hit histories and their owning callers

Target: upstream `8555c08c823b856cbec4eb99ca84ea52a955836d`, pinned locales,
Node 22. The native producers capture `BattleScene.prototype.randBattleSeedInt`
before constructing GameManager and restore it after battle startup. Upstream
`src/`, `test/framework` and `test/helpers` were clean during reproduction.

## Problem and implementation

A single turn can exceed the 32 retained `attacksReceived` records. Pinned
`MoveEffectPhase.applyMoveDamage` prepends a record after positive final damage.
Several callers search the entire array or inspect its oldest element:

| Source caller | Required answer |
| --- | --- |
| `CounterDamageAttr.apply` | Newest non-status, non-ally record of the requested registered move category; damage is multiplied, not summed |
| `getCounterAttackTarget` / `CounterRedirectAttr.apply` | The same record's stored battler index, followed by the source's active-field fallback |
| `TurnDamagedDoublePowerAttr.apply` | Any truthy damage from the target's stable ID; negative damage is truthy in raw API inputs |
| Focus Punch's `PreUseInterruptAttr` | Any positive damage this turn |
| Enigma Berry's actual `shouldApply` | Any super-effective/extremely-effective record |
| `RageTag.lapse(AFTER_HIT)` | Whether the **oldest** record has positive damage (`at(-1)` after prepending) |

Actual move phases resolve their user through `PokemonPhase.getPokemon`: the
field or `BattleScene.getPokemonById`, which searches the two parties. The
ordinary battle roster admits six members per side; switching/revival retain
those identities. Raw helpers can supply additional identities and keep their
overflow explicit.

The tail remains 32 records. A per-turn summary preserves nine latest Counter
answers (three reader contexts and three filters), damage-source membership for
the two six-member parties, and known/present bits for the existential and oldest
queries. The existing length field can saturate without losing these answers.
Current source faint/ability callers that only inspect `[0]` keep using the tail.

A complete short raw list is authoritative. An imported long tail without its
prefix reports unresolved answers as `PR_FEAT_MOVE_HISTORY`; definite retained
witnesses still answer positively. Appending cannot manufacture a missing oldest
record. Inputs exceeding the twelve-source identity budget retain witnesses and
report unresolved membership. Turn reset clears the summary; summon reset retains
turn data, as upstream does. No persistent save-format extension is needed.

## Native observations and C comparisons

`tests/received-history.producer.test.ts` creates actual damage-owner templates,
then supplies complete lists of lengths 0, 1, 31, 32, 33, 64, 65535 and 65536.
The 384 cases cover both sides, either active slot and a bench reader, eight
record patterns, all four Counter-family moves and six target identities including
0, `UINT32_MAX` and the unsigned high-bit boundary. Five observed stages cover the
initial list, an actual damage-owner append, 33 explicit zero/status appends,
actual turn reset and a fresh damage-owner append.

Long lists and zero/negative/status records are explicitly supplied API inputs;
the positive damage owner does not naturally append those zero/status records.
They are not presented as complete command-turn traces. Actual native methods
produce the expected values, Focus interruption/cancellation and Rage phase
arguments. Rage's real tree and dynamic queue also record its marker and phase.

The C matrix includes the actual private Focus Punch and Counter redirect phase
owners, checks the public damage/condition/power/berry callers, Rage's battler,
source and +1 Attack payload, marker type/count, tree-level count, dynamic queue
count and unchanged global/battle RNG. It runs **59,520 observations across 1,920
stages with zero mismatches or unsupported cases**. The corresponding old-source,
old-ABI baseline reports **17,256 mismatches/uncovered observations** with the
same matrix. These counts describe this fixture, not a completion percentage.

The raw guards add **180 checks** for missing prefixes, short raw authority,
context changes, identity overflow, negative/NaN truthiness, aliased appends,
stale/invalid cache metadata, reset and length saturation. Publicly supplied
summary metadata and retained records must describe the same history; replacing
a long history directly requires resetting its transient summary.

## Actual command and save owners

`received-history-command.producer.test.ts` runs four normal commands: Maushold
uses Population Bomb, then three Oranguru use Instruct on it. Fixed levels,
IVs/natures, identities, no-ability overrides and legal accuracy/speed stages are
fixture inputs. Speed sorting is native; there is no forced turn-order override.
The resulting seven MovePhases produce **40 positive records**, spend four PP and
leave the defender at 2770/2960 HP. HP, PP and history receive no mid-turn repairs.
This establishes reachability from the supplied battle state, not generation of
that setup through a full run or its AI choices.

The actual C `pr_run_turn` matches HP, PP, all fields of the 32 retained records
and final global/battle RNG, without overflow or unsupported flags (**42 checks**).
The source producer stops after BerryPhase; C stops at the next TurnInitPhase
before its reset. With these fixture inputs, later end-turn phases leave the
compared HP, PP, records and RNG unchanged.

`game-core/tests/received_history_host_test.c` separately drives both player
commands through the real run controller: Population Bomb hits an ally and
Instruct repeats it, producing twenty damage events with finite PP. The controller
clears turn history for the following command screen and rejects a save after the
battle's initial save point. A subsequent normal command wins, then the next
encounter saves/loads with no stale received-hit answers (**29 checks**). This is
controller/save consistency evidence, not a paired upstream full-run comparison.

## Reproduction and checks

```sh
# With Node 22 and dependencies installed in the clean pinned checkout:
battle-core/tools/reproduce-received-history.sh /path/to/upstream /path/to/output
python3 battle-core/tools/check-received-history-proof.py
```

The checked-in gzip captures retain the actual native outputs. Reproduction
compares both raw captures and generated C fixtures byte for byte. Both current
captures reproduced identically. Strict builders reject missing coverage, malformed
records, invalid RNG data and fractional/invalid Counter observations.

The retained checker runs the matrix, guards and actual command/save owners. It
also catches **13 compiled production faults**, rejects **15 malformed captures**
and detects **three well-formed forged observations** (damage, command HP and
command RNG). Compilation failure does not count as a caught production fault.
The standard CI workflow retains this checker alongside all prior checks.

Scoped ASan/UBSan/LeakSanitizer runs pass for the changed history, move attribute,
move closure, modifier, battler-tag and phase translation units, the actual
included phase-owner matrix, and the game-core battle owner. Supporting archives
use ordinary optimized builds; this is not a claim that every linked unit was
instrumented. Normal app/custom/save/football and actual ARM/stack checks remain
required on the PR head. Only the dedicated Task63 check is skipped.

The complete enabled-path inventory, remaining identity/fusion interactions,
wide-level callers and whole-run/hardware acceptance remain separate work in the
[completion checklist](COMPLETION-CHECKLIST.md).
