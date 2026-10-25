# Check Team and held form-item actions

The reward screen now opens **Check Team** for summaries and native eligible
form-item actions. Names and Activate/Deactivate labels use the existing menu
locale keys and pinned item catalogue. Choosing a form action calls the core
owner from PR #402, plays its field events, and returns to the same member in
Check Team. Back returns to the same reward selection; no reward, money or turn
is consumed. The host does not expose unsupported release/unsplice actions.

Form choices retain native inventory order and global item indices. They use a
separate pool from the summary's sorted 24-item row, so that row cannot hide
usable form items. The pool and option storage cover all 2,048 inventory entries;
large option ordinals are handled before move/All codes. Storage is static, and
the scroll scratch buffer contains nine entries rather than a large stack array.
Party order maps back to the actual storage owner, including fainted and benched
members. The list is refreshed after each action, so Ultra Crystal/Necrozma
eligibility follows the source policy validated by PR #402.

Check Team adds a fifth possible shop control. Controls use two rows when needed,
with item row heights and art adjusted to the available space. Existing reward,
shop, reroll, lock, transfer, description and TM-detail controls remain present.
The retained 13-shape geometry corpus now covers all five control combinations,
including all five controls together, and the actual Check Team touch callback.

Party/summary level and met-level display fields now preserve values such as
65,536 instead of truncating to 16 bits. Summary ability uses the base ability
(`getAbility(true)`, pinned summary handler line 922), and shiny variant is kept.
This does not close the broader Endless generation/EXP/save/identity work.

## Validation

The core owner retains its independent pinned upstream proofs: 122,112 menu
lists, 66,560 acquisitions and 74,880 mechanical form actions. This app follow-up
uses that owner; it does not substitute UI tests for those native observations.

Focused app tests exercise the actual party widgets, summary bridge, shop layout
and touch handling. Cases include a fainted bench owner under permuted party
order, 30 entries beyond the summary row, all 2,048 entries, a last ordinal beyond
move/All codes, native names found under the other form factory, read-only access
outside rewards, level/met-level 65,536, base-vs-active ability and shiny tier.
The party bridge test passes with AddressSanitizer/UBSan/LeakSanitizer enabled;
the shop and summary tests use their existing host build targets.

Two additional 300-seed run-driver corpora preserve the normal 10,000-run and
app/custom/save suites. One toggles canonical stock without a matching form;
the other starts with Mewtwo and actually changes into Mega Mewtwo X. Both check
activation, the unchanged reward screen and waiting queue, inventory/money,
three RNG streams, return to Check Team and then the shop. The active case also
requires exactly one form-change event before returning. Both pass with zero
screen/run mismatches, stuck runs or model violations. Their existing coverage
floors remain enabled, including cancellation; each also runs the retained
directed trainer-music fixture.

Normal app/custom/save, core/football and actual ARM build/stack CI are required
before merge. No builder or release is included. These checks do not establish
complete battle-core parity, graphical timing on hardware, arbitrary mixed form
histories or all fusion/Transform interactions.
