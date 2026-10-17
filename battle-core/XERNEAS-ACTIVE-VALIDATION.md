# Xerneas active form lifecycle

Upstream `8555c08c823b856cbec4eb99ca84ea52a955836d`, locales
`270ed0a2938b2c3e2e34e7de940e84aecadf548b`, Node22.23.3.

The generation-six registration has quiet neutral-to-active and active-to-neutral
changes, each with an ActiveTrigger and no extra conditions. SpeciesFormChange
matches the permanent species/form key. ActiveTrigger reads isActive(true): HP,
field membership and switch-out status. Ability suppression does not gate it.
The scene queues a QuietFormChangePhase and leaves mutation to that phase.

SummonPhase.onEnd and SwitchSummonPhase.onEnd request delayed active triggers.
Both callbacks can enqueue a transition before either phase executes. Preserve
that duplication/order: the later quiet phase sees the already-selected form
and ends without another mutation. ReturnPhase's own trigger remains immediate.
The port previously queued summon transitions immediately; enabling Xerneas
exposed four trace mismatches in previously unsupported broad cases. The source
ordering fixes those cases without changing fixtures or comparator rules.

## Observed checks

- Native scene-trigger/QuietFormChangePhase execution: 8,192 cases each at seeds
  777 and 2034, all 16,384 exact. Both sides/forms, active/off-field/switch-out and
  zero HP, immediate/delayed queues, suppression, native/custom/passive/temporary
  abilities, weather controls, Tera and stages are varied. Both transition
  directions must occur for immediate and delayed requests.
- The producer captures native BattleScene battle RNG before GameManager and
  restores it after startup. No gameplay method is replaced. Invisible actors
  avoid animations; UI/assets/phase transport are stubbed. Exact comparisons
  cover initial/pre-phase/post-phase state, ordered queued forms/levels, form
  ability history, English text and both RNG states.
- All 2,050 original berry-move doubles turns now match, including the original
  unsupported case1782. No cases were removed or normalized.
- Retained broad doubles: 1,969/1,955/1,970 exact; mixed turns:
  1,978/1,973/1,993/1,996 exact. Every previously covered ID remains covered;
  all phase/text/state/RNG comparisons and coverage floors are unchanged.
- Five compiled trigger faults caught; twenty malformed fixtures rejected.
  The optional --turns guard also catches the parent summon ordering fault
  against a complete native doubles capture. Changing only the second callback
  to immediate was not detected by this corpus; that exploratory control is
  retained in the execution log and is not counted as caught. Both source
  callbacks explicitly request delayed dispatch. The source snapshot's isOnField already
  folds in switchOutStatus, so ignoring that second redundant projected gate
  is not counted as an independently caught fault.

Reproduce `tools/xerneas-active-diff.sh UPSTREAM 777` and repeat with 2034.
For caller faults, run `TURNTEST_LOCALES=UPSTREAM/locales/en python3
 tools/check-xerneas-active-proof.py TRACE --turns DOUBLES_TRACE` with an original
2,000-turn seed777 capture from the unchanged doubles producer. The execution
workspace retains the four original failing traces and their corrected replays.

This closes the registered Xerneas active trigger, not all species forms,
fusion/Transform combinations, scene inventory families, or all battle-core.
Existing quiet-phase invisible-enemy behavior remains source-defined. No run-core
changes or save schema changes are included.
