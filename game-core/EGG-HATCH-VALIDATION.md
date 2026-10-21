# Ready eggs and hatch rewards

The port decremented egg counters but never completed a hatch. EggLapsePhase now
runs after BattleEndPhase and trainer victory, before wave rewards, on the same
non-final winning waves as upstream. It hatches every ready egg, including eggs
left at zero by older builds. Common/Rare/Epic/Legendary eggs normally need
10/25/50/100 winning waves; Manaphy eggs need 50.

Each hatch uses the native egg-ID seed independently of the run and battle RNG.
It generates the hidden/regular ability, ID, gender, egg-phase form, nature, tera
type and doubled IV roll in source order. The constructor receives shiny=false;
the egg applies its stored shiny/variant afterward. The profile receives the
caught attributes, nature, ability, improved IVs, hatch count, candy and egg move.
The egg is removed only after successful generation. The existing encounter
save persists the updated profile. Hatches unlock starters rather than joining
the current party. Text announces the hatch and newly unlocked egg move; the
animated hatch/egg-summary presentation is separate UI work.

Reference: upstream 8555c08c823b856cbec4eb99ca84ea52a955836d, locales
270ed0a2938b2c3e2e34e7de940e84aecadf548b. Native tests restore battle RNG after
natural GameManager startup and disable the framework's IV/nature overrides.

- 2,284 actual EggLapsePhase.generatePokemon observations, four modes for every
  one of the 571 factory species. All ID/species/form/gender/ability/nature/IV/
  shiny/variant/tera comparisons are exact. All 25 natures occur, and every case
  has nonperfect IVs; the framework's perfect-IV defaults are not an oracle.
  Native and C checks preserve both surrounding RNG streams.
- The strict reader requires all 571 species exactly four times, sequential case
  IDs, fixed complete records and footer. Factory and egg-move table orders
  differ; coverage is checked by actual species IDs without discarding cases.
- Real victory commands test two and 99 simultaneous hatches, recovery of eggs
  already at zero, announced results without event overflow, profile rewards,
  future-egg ordering/countdown, saves and no repeat reward after reload.
- Eight compiled generation/owner faults and fourteen malformed captures are
  caught. Controls cover the egg seed, hidden chance/override, IV boost, egg
  forms, old-zero recovery, egg reward classification and owning callback.

Reproduce with Node 22:

```sh
game-core/tools/egg-hatch-diff.sh /path/to/pinned-upstream
python3 game-core/tools/check-egg-hatch-proof.py game-core/build/egg-hatch.txt
make -C game-core egg-hatch-host-test
```

These bounded observations establish the current factory-generated eggs, not
all legacy/custom eggs or full battle-core completion. Legacy entries with no
stored species remain intact: the old-species reroll path is not ported here.
Unknown species or invalid metadata are not consumed. No save schema field is
added; the new run phase is appended so existing phase IDs stay unchanged.
Normal game-core/app/custom/save tests and the actual 3DS stack/frame guard
remain merge requirements.
