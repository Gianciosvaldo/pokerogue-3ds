# Tera shard consumption and Memory Mushroom menu

Pinned upstream: `8555c08c823b856cbec4eb99ca84ea52a955836d`; locales:
`270ed0a2938b2c3e2e34e7de940e84aecadf548b`.

Tera shards previously reached the generic reward fallback: an inert inventory
entry was added, while the selected Pokemon's Tera type stayed unchanged. The
consumable now sets that type directly and creates no held item. Matching the
native `TerastallizeModifier.shouldApply` and type selection filter, Terapagos,
Ogerpon and Shedinja are excluded in either species slot. Fainted recipients and
same-type applications remain allowed. Tera Orb ownership gates shard generation,
not raw consumable application upstream.

The native producer runs actual `TerastallizeModifierType.selectFilter`,
`shouldApply` and `BattleScene.addModifier`. It captures native battle RNG before
GameManager construction and restores it after startup. All 1,520 cases compare
four primary species, five fusion shapes, fainted/live state, all 19 types and
same/different starting type: 24,323 checks, no mismatches. The other party member,
modifier inventory and random streams remain unchanged. Six compiled faults are
caught. Fresh Node22 records reproduce byte for byte; focused handler/runner
ASan/UBSan/LSan passes.

Memory Mushroom's core learning owner already existed, but the app opened a
generic Apply menu without learnable moves and discarded their selection ordinal.
The shop now dispatches to the remember-move menu. A shared public query supplies
the same ordered list that reward application uses, preserving storage-party
identity after display reordering. The UI carries the full current 486-row core
budget (160 level rows, 322 TM history entries, four egg moves); a compile-time
check rejects exceeding its 512-row storage or colliding with option codes.
Counts are 16-bit, and a short output buffer is rejected rather than truncated.

The real `party_screen.c` → `party.c` sanitizer test selects the last item past
index255, checks its displayed move, verifies the returned storage slot and
ordinal, and backs out to the shop. It also checks Tera exclusions and fainted
recipients alongside all retained TM, cancellation and form-item scenarios. The
retained native TM-history test adds the public query at every frame: all 313
callbacks and 314 save roundtrips pass, now with 822,890 checks. Its original
capture and prior comparisons remain unchanged.

```sh
python3 game-core/tools/check-tera-shards-proof.py
bash game-core/tools/reproduce-tera-shards.sh PINNED_UPSTREAM OUTPUT_DIR
TERA_SHARDS_SANITIZE=1 python3 game-core/tools/check-tera-shards-proof.py
make -C game-core tm-history-host-test
make -C game/tests party_item_test
# Run game/tests/build/party_item_test outside ptrace with leak detection enabled.
```

Changing Tera type does **not** activate Terastallization. Player Tera activation
controls are still absent from the app and are a separate open caller boundary.
This follow-up does not claim full Tera command/recharge or battle-core completion.
Normal core/app/custom/save/football and actual ARM/stack checks remain required;
only dedicated Task63 is excluded. No Windows builder is generated.
