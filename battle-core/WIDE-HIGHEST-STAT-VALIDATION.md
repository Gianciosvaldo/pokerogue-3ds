# Wide highest-stat tag selection

Pinned upstream `8555c08c823b856cbec4eb99ca84ea52a955836d`, locales
`270ed0a2938b2c3e2e34e7de940e84aecadf548b`, Node 22.23.3.

`HighestStatBoostTag.onAdd` compares all five effective stats as Numbers, with
ability/ally/held modifiers ignored for the comparison. It uses strict greater
than, so the first equal stat wins. The tag is already attached during this
comparison, so its initial Attack x1.3 participates. The selected Speed boost
is 1.5; the other four are 1.3. Overlap is the inherited no-op; removal queues the
associated Protosynthesis/Quark Drive message.

The port's getter now preserves wide values after #301, but the chooser still
narrowed both its running maximum and each candidate to int32. Change those two
locals to double. No gate, tag/save representation or generated table changes.

Reproduce `tools/wide-highest-stat-diff.sh UPSTREAM 777`, then 2034. The producer
captures native BattleScene RNG before constructing GameManager and restores it
after natural double-battle startup. It invokes the actual `Pokemon.addTag`,
duplicate add and `removeTag`; no expected selection formula is copied. Only UI
refresh and automatic phase advancement are intercepted.

- All 16,384 cases exact, each with real add/overlap/remove operations. Four field
  positions, both tag families, levels 1..1,000,000, ordinary/32-bit/2^40 overrides,
  fractional and near-tie inputs, stage changes and Huge Power are included.
  Each seed selects every effective stat at least 100 times; seed 777 counts are
  2,517/1,359/1,430/1,394/1,492.
- Compares all four actors’ 125-field frames before and after every operation,
  every actor's tag count/presence/selected stat/multiplier/duration, numerical
  effective stats after each operation, complete message queue levels/order/text
  and both RNG states. Native remove returns void; its transport marker is -1,
  while actual removal is checked through tag state, numeric getters and text.
- Baseline seed 777:3,798 exact and 4,394 failures using the pre-fix production
  library. The same captured records all pass after the two-local repair.
- Seven compiled faults caught: either int32 local, last-stat ties, wrong Speed
  multiplier, missing initial self boost, including ability multipliers while
  selecting and missing message. Fourteen malformed captures rejected.
- A preliminary capture encoded native void as NaN and did not observe all
  actors' tag records. It is retained separately and is not validation evidence.
  Final captures explicitly transport void and compare all actors' tag records.

These are owning API comparisons, not proof of naturally generated high-wave
encounters or every held/fusion/weather/terrain interaction. Existing whole-turn
covered IDs and normal game-core/app/custom/save tests remain required, together
with exact-head 3DS CI. Only the dedicated custom-only Task63 job is skipped.
The remaining source inventory and Endless completion gates stay open.
