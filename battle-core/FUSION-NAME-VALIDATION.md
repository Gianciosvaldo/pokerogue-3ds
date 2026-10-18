# Frozen native fusion names

Base: merged #277, `c4b3683a68e3fd992ca35b2f646f3a4d3bd64a17`.
Source: `8555c08c823b856cbec4eb99ca84ea52a955836d`.
Locales: `270ed0a2938b2c3e2e34e7de940e84aecadf548b`; Node 22.

Battle messages previously named only the primary species. Messages now snapshot
both permanent species/form halves, or both Illusion halves, before formatting
native fusion names. Transform's temporary copied appearance does not replace
its permanent rendered name. Its target-name announcement deliberately uses the
actual target's permanent identity, as Source uses `target.name` there.

The fragment helper preserves the pinned function's literal regex character
class, greedy A-vowel run, match-array-length `slice(2)` quirk, repeated-character
boundary rules, prefix/suffix preference, and Rapidash safeguard. UTF-8 names
are sliced in UTF-16 code units. English native names, including accents,
gender signs and apostrophes, are covered. Inputs exceeding 255 code units are
refused. Arbitrary nicknames, other locale casing rules and unrestricted text
are not established by this proof.

Wish, Healing Wish and Lunar Dance now freeze both name halves. Wish's primary
name was also missing from its old save field list. New saves serialize named
scalar fields for both halves and require them through a saved schema marker.
Malformed primary/fusion identities and missing required fields are rejected.
Historical ordinary and unfused pending-heal saves without the marker remain
loadable; the missing new marker is counted as one defaulted field. Historical
Wish saves never stored their original names, which cannot be recovered exactly.
No old frozen name is reconstructed from a later changed source.

Actual-source comparisons:

- 16,384 raw name pairs at seed 2034, from all native species/form names and
  324 punctuation/form-prefix edge pairs; all exact. Small output capacities
  preserve the required length and NUL/truncation contract.
- 8,192 `generateName` / `getNameToRender` / `getPokemonNameWithAffix` cases at
  seed 777, including player/wild/trainer/final-boss prefixes, Illusion and the
  owning Transform phase; all exact. Messages remain immutable after actors change.
- 8,192 stored Healing Wish/Lunar Dance cases at seed 2034, including entry,
  eligibility, held healing/status phases, missing sources and changed source
  names; all exact. This extends the retained PendingHeal comparison, without
  relaxing its state, queue, message or RNG checks.
- 4,096 actual WishAttr cases at seed 4242 through two PositionalTagPhases,
  including held heal amount/target/message and changed source names; all exact.
  This is a dispatcher/held-phase proof, not a new full-turn fusion corpus.

Initial helper captures sampled only species with explicit form arrays. The
final generators include ordinary species with empty form arrays too. An early
healing fixture used modulo zero for those arrays and was correctly rejected;
its setup was repaired, without changing runtime expectations. A host assertion
initially inspected a load target after corruption tests overwrote it; the
identity check now precedes those rejection probes.

Commands: activate the parity toolchain, then run
`battle-core/tools/fusion-name-diff.sh PINNED_UPSTREAM MODE N SEED`, with modes
`name`, `message`, `heal`, `wish`. No Source gameplay method/formula is replaced;
only UI/asset/timing transport is stubbed. Existing retained exclusions and
comparison tolerances remain unchanged. Mutation/malformed validation is in
`tools/fusion-name-guards.py`; pass the battle-core directory, capture directory
and pinned English locales directory.

Eleven compiled faults are caught (fragment boundaries/preferences, missing
snapshot/store/delivery fusion halves); thirteen malformed captures are rejected.
Host command/entry/save/cleanup checks pass 240 assertions, plus eighteen checks
loading and rewriting two saves emitted by the previous build. CRC-correct
malformed stored names and omitted required fields are rejected. Complete
game-core and app host suites pass, including 10,000 ordinary app runs and the
existing ordinary/custom save/resume checks. Paired save/no-save digests remain
`99e9513d`. Retained 24,000 mixed turns have unchanged exact coverage and no
mismatch; 72,000 dedicated ability turns and 16,000 Ditto cases also pass.

The first app build ran out of disk space. Only verified ignored build
directories in older merged worktrees were removed; the successful retry uses
four compiler jobs. 3DS CI is recorded with the PR after publication. No run-core file, custom-content workflow, emulator or hardware claim.
Native full fused turns, form callback interactions, broad doubles and wide
levels remain separate audit items.
