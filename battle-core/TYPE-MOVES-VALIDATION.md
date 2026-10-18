# Reflect Type, Conversion and Conversion 2

Pinned source `8555c08c823b856cbec4eb99ca84ea52a955836d`, locales
`270ed0a2938b2c3e2e34e7de940e84aecadf548b`, Node 22.23.3.

Implement the three missing registered effect handlers. Their existing move
conditions remain separate from the raw callbacks:

- Reflect Type checks the target's HP, obtains types with original typing for
  Stellar, replaces UNKNOWN with NORMAL, copies them to the user and queues the
  copy message. The source comment says Grass, but its code uses Normal.
- Conversion checks the user's HP and uses the first moveset entry's static move
  type. Its type argument is localized.
- Conversion 2 checks the user's HP and the target's newest history entry,
  including failed, missed, pending, NONE or virtual entries. It uses the move's
  original static type, enumerates resisting/immune types in enum order,
  excludes the user's current types (original types for Stellar), and selects
  using the user's battle RNG. Its type argument is the literal English enum
  title. The source marks this move partial; the original-type behavior is
  deliberately retained.

No state fields, save schema or generated data change. Reuse existing move
conditions, type storage, history and text transport. An empty first moveset
would throw upstream; the C handler explicitly reports unsupported at that
invalid boundary.

Reproduce `tools/type-moves-diff.sh UPSTREAM 777`, then 2034. Native RNG is
captured before GameManager construction and restored after natural doubles
startup. Registered source methods and conditions run without gameplay patches.

Both seeds match all 25,344 raw cases and 50,400 calls. Compare four actors'
125-field snapshots after each call, full input/output history entries and
results/use modes/targets, real move conditions at sequences 2/3/4, all queued
message levels/order/text, actual RNG-call counts/ranges and both RNG states.
Include self/ally/opponent aliases, fainted actors, levels 1..1,000,000, repeated
calls, 18 first-move types, all 19 Tera types, UNKNOWN/added Ghost/Grass and up to
four temporary types. Native boolean returns are recorded; C dispatch returns
void and no boolean-return parity is claimed.

Protocol 2 preserves the original 12,288 cases and appends 128 no-resistance
controls and 128 single-resistance controls. The latter must still call battle
RNG with range one; the former must not draw. Source spies pass through to the
real method, and a linked C wrapper observes actual call arguments. Protocol 3
adds 128 historical Tera Blast controls whose target is Tera Fire: Conversion 2
must resist Tera Blast's original Normal type. Earlier captures remain required.

All 16,896 complete owning turns match across both seeds, with zero source
exceptions or unsupported cases. Successful native callbacks for Reflect Type/
Conversion/Conversion 2 number 3,971/4,219/1,965 and 3,980/4,226/2,002. Each seed
includes 128 UNKNOWN/added-type/Stellar controls and 128 actual move-history
controls; the owning attributes must execute in every appended control. Existing
ally/spread/faint/switch comparison floors remain unchanged.

The first owning control attempt supplied historical moves only to opponents,
but Conversion 2 can target the ally too. Its callback-presence assertion
failed. Those incomplete attempts are excluded. The corrected fixture supplies
all eligible targets with the intended state and reruns all cases; it does not
skip targets or weaken assertions.

All 29 compiled faults are caught, including parent/raw-condition gating,
recipients, original Stellar types, UNKNOWN replacement, first-slot selection,
history owner/virtual filtering, original versus effective historical move type,
candidate order/current-type exclusions/immunities, zero/one-choice RNG, missing
setters/messages and owning dispatch. Twenty-three malformed captures are
rejected. Earlier raw protocols and merged Pledge/HP-cost/Pain Split captures
remain required integration evidence. No coverage floors are reduced.
Normal game-core tests pass, with matching saved/unsaved digest `361c1d0e`.
Retained doubles are 1,989/1,988/1,992 and mixed turns are
1,996/1,994/1,994/1,998: twelve additional exact cases and no lost covered IDs.

These are bounded observations of ordinary Classic/Endless challenge settings.
Inverse/type-effectiveness challenge inputs are not modeled by the existing
core and remain an inventory gap. Arbitrary custom-type overrides, all
fusion/Transform/inventory/passive interactions, natural high-wave progression
and the full source inventory remain open. Normal app/custom/save tests and
exact-head 3DS CI are required before merge; skip only dedicated Task63.
