# Final-boss victory dialogue before faint

Source: `8555c08c823b856cbec4eb99ca84ea52a955836d`, locales
`270ed0a2938b2c3e2e34e7de940e84aecadf548b`, Node 22.

`FaintPhase.handleFinalBossFaint` calls
`ui.showDialogue("battleSpecDialogue:secondStageWin", enemy.species.name, null,
() => this.doFaint())` when the active enemy has form index greater than zero.
Admission is the source Classic/Challenge final-wave predicate, wave 200. The
line is `…Magnificent.` in pinned English. The speaker is the base species name,
without the Eternamax, fusion or foe naming wrappers. Positive-HP early returns,
player faints, other modes/waves and first-form cancellation keep their separate
paths. The earlier first-form proof and all of its 480 cases remain unchanged.

The port now appends `PR_EV_DIALOGUE` without renumbering existing events or
changing their structure. The event records its locale key, primary speaker
species ID and field snapshot before ordinary faint bookkeeping. The app copies
its translated line and name into playback storage, creates a portrait-free
`BEV_DIALOGUE`, then plays the faint. It uses the existing prompted message box:
typing must finish and A dismisses each page. Existing portrait dialogue behavior
is retained. The small playback state machine was extracted without changing its
steps so both paths can be tested directly.

As with other engine presentation events, the C kernel produces an ordered
snapshot stream; it does not block on host UI input. The app blocks advancement
to the faint until the dialogue is dismissed. Native callback observations and
app playback tests establish those respective boundaries, not equivalence of
Phaser and Citro2D timing.

The converter's text source also now treats a missing locale file as an empty
entry while fallback languages remain. Returning NULL there incorrectly ended
the entire chain before English could be tried. NULL remains the end-of-chain
signal. A missing-French-file control verifies the actual English fallback.

## Evidence

- `final-boss-dialogue.producer.test.ts` invokes actual pinned FaintPhase.start in
  640 prepared contexts: five modes, waves 199/200/201/5850, both sides, forms
  0/1, HP 0/1, move queues of length 0/4 and charging tags present/absent. Native
  battle RNG is captured before GameManager construction and restored after the
  battle starts. The native UI callback is held, checked, then invoked; ordinary
  doFaint is intercepted at entry. Instant revives are prevented and no fatal
  source is supplied. This is not a downstream ordinary-faint comparison.
- The strict C replay checks all 640 contexts in fixed order: HP/form/field,
  tags/queued move IDs, participants, faint counters/history lengths, both RNG
  states, callback admission, queue levels/payloads, dialogue key, base speaker,
  localized English text and frozen field metadata. Eight dialogue callbacks;
  30,537 checks. Fourteen compiled behavior faults are caught, one compiled
  pristine control passes, and twelve malformed/forged captures are rejected.
- Fresh pinned-source reproduction is byte-identical after gzip decompression;
  the English JSON fixture is byte-identical to the pinned locale file. These
  observations are not regenerated from the C output.
- The app phase/converter test checks four positive conversions (English and
  missing-file fallback), 118 negative contexts, field replacement/name scratch
  reuse and batches split between dialogue and faint. Thirty paused-before-faint
  contexts retain ordinary catch-up while deferring final-boss disappearance
  until its explicit faint event (or first-form cancellation). Twenty-four additional
  prepared Classic fights execute actual attack turns through FaintPhase and
  the app converter, require dialogue before faint, stop at Victory and reject
  all unported/deferred/overflow paths. These are C integration cases, not native
  full-fight parity or legal-start final-boss clears.
- The real playback state machine and message-box typing/input code keep the
  name and sprite until A, do not auto-dismiss, and require A for every page.
  Retained portrait/expression/fade transitions are checked. Ten separately
  compiled converter/player faults are caught; both pristine controls pass.
- Scoped ASan/UBSan/LeakSanitizer passes for the changed faint unit and native
  replay, and for the converter/actual-turn app test; supporting archives remain
  unsanitized. The isolated playback state machine plus real message-box code
  passes UBSan. These are scoped checks, not whole-program sanitizer coverage.
- The existing first-form 480-context replay and its fourteen compiled faults,
  pristine control and eleven capture guards pass against the changed phase.
  Normal core/app/custom/save/football and actual 3DS/ARM stack CI remain merge
  requirements; only dedicated Task63 is excluded.

Reproduce with Node 22:

```
bash battle-core/tools/reproduce-final-boss-dialogue.sh PINNED_UPSTREAM OUTPUT_DIR
python3 battle-core/tools/check-final-boss-dialogue-proof.py
make -C game/tests final_boss_dialogue_events_test final_boss_dialogue_playback_test
python3 game/tests/check_final_boss_dialogue.py
```

This closes the explicitly guarded second-form victory-dialogue path. It does
not certify all battle-core mechanics, full Endless/token journeys, final-boss
encounter/first-stage dialogue, or on-device visual/performance acceptance. No
Windows builder or release is produced.
