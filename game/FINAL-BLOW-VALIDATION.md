# Final-blow provenance in the run report

The end-run report used to print "Final blow: source not recorded" unconditionally.
It now names the source Pokémon and move when the battle engine actually records
both at the terminal faint. Ordinary and custom runs use the same read-only
observer; custom callbacks still run only when installed.

The existing battle-core FaintPhase note requires the phase's explicit source to
match the newest positive-damage attacksReceived record. Poison, weather, recoil
and other unattributed faints do not inherit an earlier attack. This uses the
already implemented engine observer, not an inference from animation/event order.
The pinned native FaintPhase has a source only for move-triggered faints, and its
PostVictory path requires that source; its more permissive PostFaint fallback is
not used to invent report attribution. Source pin:
8555c08c823b856cbec4eb99ca84ea52a955836d.

Game-core copies the note before its pointers expire, retaining current source
species/form (including Transform), source side, move, turn, wave and remaining
party count. Each encounter clears the records. Every faint replaces its side's
record, including unattributed faints, so an earlier direct hit cannot survive a
later residual knockout. The report accepts only a current-wave terminal faint
and owns its snapshot. Finishing a report twice does not modify it.

The report header uses this explicit provenance independently of the retained
raw-event history. Raw event source bits remain unknown: a truncated history is
still reported as incomplete and is never retroactively attributed. Unknown
sources keep their existing explicit label. No save version or battle rule
changes; these transient battle notes are not persisted in encounter saves.

Validation includes actual ordinary-run and custom-hook faint execution, residual
faints, custom-hook removal, source/move/remaining-party/wave checks, report
ownership and idempotence, stale/nonterminal rejection, and rendered known versus
unknown report headers. Full core and normal app/custom/save suites and the
actual ARM build are required before merge. The app run driver also checks every
terminal snapshot against the resulting report while retaining its existing
coverage floors and gameplay digests.
