# Flame Burst and its indirect-damage callers

Source pin: `8555c08c823b856cbec4eb99ca84ea52a955836d`.
Locales: `270ed0a2938b2c3e2e34e7de940e84aecadf548b`.
Actual source execution uses Node 22.23.3 and additive tests; tracked source is
unchanged. The existing producers, comparators and coverage contracts remain
unchanged. No run-core files are edited.

## Implemented behavior

Pinned `FlameBurstAttr.apply` in src/data/moves/move.ts gets the actual target's
ally without calling the parent HP/condition/chance gates. It applies the ally's
BlockNonDirectDamageAbAttr before rejecting a switching ally. Missing/blocked/
switching allies receive no damage. Otherwise it calls actual damageAndUpdate
with max(1, floor(maxHP/16)), result INDIRECT, no source and normal faint queuing.
The C leaf now follows that sequence, using the existing party-slot getAlly and
shared damage path. It also preserves raw singles-reserve getAlly behavior.

The actual owning source matrix exposed three shared prerequisites:

- Boss handleBossSegmentCleared calls weightedPick, whose randSeedInt consumes
  Phaser's global RNG. C incorrectly consumed the battle RNG. The caller now
  uses the supplied global RNG, flagging its absence explicitly.
- The source still queues StatStageChangePhase with no changes when every stat
  is capped. C previously omitted that observable empty phase.
- Wild Wimp Out/Emergency Exit with no active ally calls
  clearEnemyHeldItemModifiers() and updateModifiers(false). C omitted removal
  and party stat refresh. It now clears all enemy held items, retains the
  established modifier-boundary flags and recalculates party stats.

These fixes follow actual source observations; no expected values were changed
or failing cases removed. They are prerequisites encountered through Flame
Burst, not a claim that all boss/switching interactions are complete.

## Actual source API evidence

The producer captures native BattleScene.prototype.randBattleSeedInt before
constructing GameManager and restores it after battle startup. It calls the
actual registered owning FlameBurstAttr and native damage, shield, ability and
wild-flee methods. It stalls phase advancement and mocks asset/info boundaries;
it does not replace gameplay methods or RNG formulas.

All 16,384 complete cases match, zero failures/unsupported. Inputs include:

- Both sides and all holder/target slots, doubles and raw singles/reserve calls.
- User/target/ally HP 0, switching allies, floor/minimum HP boundaries, Endure and
  Substitute. Indirect damage bypasses Endure and prevents instant revival.
- Eight primary choices including Magic Guard, Wimp Out/Emergency Exit and four
  passive choices, suppression flags and Neutralizing Gas counts.
- Boss segments 0/2/3/5, native cached shield indexes, capped and partially capped
  stats. There are 148 shield changes and 1,375 new faints in the source capture.
- Named BaseStatModifier objects on both sides, including two enemy owners;
  removal compares actual inventory records, not only the fleeing holder.
- Source hitCount/hitsLeft 2 controls: supplying the user as damage source would
  incorrectly suppress post-damage abilities. The correct call has no source.

The transport compares the full existing 125-field actor projection before and
after, extra shield/Endure/Substitute-HP/hit state, inventory, captured world
state, exact queue levels/dynamic order, text and stat changes, both RNG streams,
and damage/faint/stat/switch metadata. The source boolean return is recorded;
the C effect API returns void, so boolean-return parity is not claimed.

This is an augmented API matrix: Rattata actors at levels 30..50, selected
abilities, deliberately supplied HP/stat boundaries, and sequential native
leave-field state. These are sampled combinations, not every form, fusion,
Transform, held-item interaction or legal command path. Queued boundary phases
are observed, not all executed by this direct proof. Substitute's sprite
presentation and hardware behavior are outside its state projection.

## Guards and retained source turns

Nineteen compiled runtime faults are caught, including parent/ally HP gates,
wrong ally/rounding/minimum/result/source, omitted damage or faint processing,
missing ability checks, wrong switch-check ordering and skipping singles.
The source-sensitive shared faults fail as follows:

| Fault restored | API failures |
| --- | ---: |
| Boss draws from battle RNG | 134 |
| Empty capped-stat phase omitted | 14 |
| Flee stat refresh omitted | 108 |
| Flee item clearing omitted, or limited to fleeing owner | 18 |
| User supplied as damage source | 133 |

Twenty-four malformed captures are rejected. Missing damage also fails 9 actual
owning turns from the unchanged broad doubles source capture.

Unchanged captures retain every previously covered case ID, with zero compared
state/text/RNG mismatches:

| Capture | Before | After | Newly covered | Lost |
| --- | ---: | ---: | ---: | ---: |
| Doubles plain777 | 1937 | 1946 | 9 | 0 |
| Doubles rich777 | 1928 | 1935 | 7 | 0 |
| Doubles plain2034 | 1950 | 1951 | 1 | 0 |
| Mixed plain/rich/summon/switch, 2000 each | 1974/1969/1991/1995 | same | 0 | 0 |

The three broad doubles captures now compare 5,832 exact turns; 168 remain
explicitly unsupported. Those samples prioritize work and do not establish a
completion percentage. Acupressure 8,194 owning turns, its 16,384 API cases,
After You 8,192, suppression 16,384 and wide stats 16,800 remain exact. The unchanged
wide-score source capture still matches 2,000 numeric caller results while 1,442
contexts separately flag unsupported phase work.

## Integration and finish line

The added host fixture passes 39 augmented callback/save/real-command checks.
It applies the public leaf to a singles reserve's ally, then saves/loads at the
existing permitted first-command boundary and resumes actual Splash commands.
Magic Guard controls, HP/ability restoration and all three RNG projections
remain consistent. It is not a natural doubles command/save acceptance proof.

Full game-core tests pass. Paired 200-run saved/unsaved digests are both `5e19bd8a`;
the previous digest was `d42eb77a` before these source-backed shared changes.
This is continuation consistency, not upstream whole-run parity. Exact-head
normal app/custom/save tests and the 3DS build remain merge gates, with outcomes
recorded in the PR. Only the dedicated custom-only Task63 jobs are excluded.

The completion checklist now tracks the finish line: source inventory review,
remaining doubles and identity/form/phase paths, wide-level progression/combat,
interaction evidence and final integration audit. Its static candidate-location
tool is not a semantic count of missing mechanics. Bounded proofs here do not
close those other categories or justify full battle-core completion.

Reproduce from battle-core:

```sh
tools/flame-burst-diff.sh UPSTREAM
TURNTEST_LOCALES=UPSTREAM/locales/en python3 tools/check-flame-burst-proof.py \
  build/flame-burst.txt DOUBLES_TRACE
```

Run make -C game-core test for the new host fixture and all retained game tests.
