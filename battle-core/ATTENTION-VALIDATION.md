# Attention tags and redirection

Pinned upstream `8555c08c823b856cbec4eb99ca84ea52a955836d`, locales
`270ed0a2938b2c3e2e34e7de940e84aecadf548b`, Node 22.23.3.
Tracked upstream, previous producers/comparators/coverage contracts and run-core
are unchanged. The dedicated custom-only Task63 check is skipped; normal
app/custom/save and 3DS CI remain required.

## Implemented source paths

CenterOfAttentionTag constructs with TURN_END lapse, one turn, supplied source
move and no source ID. Its powder property is determined by RAGE_POWDER. canAdd
checks the side's raw field slots for an existing attention tag, including
fainted, switching and off-scene members. Overlap keeps the tag unchanged. onAdd
queues battlerTags:centerOfAttentionOnAdd; ordinary base lapse/removal applies.

MovePhase.resolveRedirectTarget retains the existing speed-ordered ability
checks, then visits active on-field opponents in field order. Attention overrides
ability redirection. Rage Powder refuses Grass users or active Overcoat, including
passives and suppression/Gas behavior. Other attention sources bypass those
powder checks. Attention clears redirectedByAbility even when an ability also
redirected, so the Pledge moves' abilitiesOnly bypass does not cancel it. Snipe
Shot's general bypass and Stalwart/Propeller Tail remain in the existing path.
The owning phase and native replay call the same internal C helper.

Raw reserve-user controls exposed one shared constructor correction:
ShowAbilityPhase has no cached ability name when getPokemon() finds no field
slot. C now records UINT16_MAX as that absent cache value; the typed transport
renders that sentinel as -1, matching the source's undefined-name observation.
Valid cached ability IDs remain compared exactly.

## Actual native observations

`tools/attention-diff.sh UPSTREAM` runs the additive native producer/replay.
Native BattleScene.prototype.randBattleSeedInt is captured before GameManager
construction and restored after natural doubles startup. Actual registered
Follow Me, Rage Powder and Spotlight attributes apply, the native protected
MovePhase method resolves targets, and Pokemon.lapseTags runs.

The final matrix has 16,768 complete cases, all exact, with zero unsupported,
deferred or overflow cases. The original 16,384-case capture also remains exact
when replayed separately. After a priority fault survived the initial matrix,
384 additional Pledge controls were added; the final matrix catches that fault.
Startup-generated Pokemon IDs can vary between captures; neither runtime replay
normalizes IDs or any other state.

Contexts cover twelve Rattata actors on both sides, party lengths 1..6,
singles/doubles and raw reserve callers; fainted, switching and absent scene
members; primary/passive Lightning Rod, Storm Drain, Overcoat, Stalwart and
Propeller Tail; suppression and Gas; Grass typing, speed ties and Trick Room;
Protect, existing/overlapping tags, and zero/single/multiple targets. Some raw
contexts deliberately construct two attention holders on one team to observe
native ordering; those contexts are not naturally creatable via canAdd.

The established 125-field actor projection and attention/protection metadata
are compared before and after the leaf, after redirection and after lapse.
Queues compare exact levels, dynamic order, text and typed constructor data;
target arrays and both RNG streams are compared. The methods run natively;
asset/info/frame-rate/animation/UI and phase advancement boundaries are mocked.
Queued phases are observed here, not drained. Their earlier applicable execution
proofs are retained. Source leaf booleans are recorded; C's void effect API does
not establish boolean-return parity. Custom mutation of the native tag's powder
property, whole species/form/fusion/modifier space and every natural command
arrangement are outside this augmented matrix.

The proof guard catches 17 compiled runtime faults and rejects 29 malformed
captures. Faults cover missing tags, constructor fields, team overlap (including
fainted/switching members), text, expiry, spread/inactive/ally targeting, Grass
and Overcoat, powder distinctions, ability priority and absent cached abilities.
Malformed checks retain the complete stream and reject invalid numeric ranges,
nonfinite values, queue integer overflow and missing/incorrect footers.

## Retained and integration evidence

Three unchanged doubles captures compare 1,962 / 1,949 / 1,965 turns exactly:
21 gained IDs, none lost. Their 124 unsupported cases remain explicit. Four mixed
captures compare 1,976 / 1,970 / 1,991 / 1,995 exactly, gaining three IDs and losing
none. These are bounded counts, not completion percentages.

The 12,800-case partner-cure proof remains exact, as do prior Acupressure,
After You, suppression, wide numeric and Flame Burst proofs and 8,194 Acupressure
owning turns. The full game-core suite passes; 99 new host checks exercise actual
owning commands, tag overlap and expiry, and supported first-command saves with
and without augmented starting tags. No later-turn save capability is introduced.
Paired 200-run saved/unsaved digest remains 5e19bd8a.

Commander/Order Up, other inventory-discovered paths, wide-level progression and
combat, and the final source/interaction reconciliation remain open.
