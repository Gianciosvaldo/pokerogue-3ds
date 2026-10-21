# Trainer and rival visibility

The battle view had no trainer drawing path. It now draws the trainer on the
enemy platform during the encounter intro, including all six rival encounters,
the male/female variants, and paired trainers. The trainer follows the platform's
slide and leaves when the opponent sends out its first Pokémon. Trainer textures
are released at the next wave alongside the other encounter art.

Sprite keys come from actual upstream Trainer instances, rather than guesses
from trainer names or encounter IDs. This preserves derived types (all six
rivals share rival_m/rival_f) and uses the actual sprite count to distinguish a
paired trainer from a double-only portrait that already contains both people.
The baseline capture freezes the clock at 2024-01-01, without seasonal sprite
replacements. Source: 8555c08c823b856cbec4eb99ca84ea52a955836d.

- All 272 native configs, three variants and two sprite positions: 1,632 C
  lookups match the native observations. Six invalid lookup requests are refused.
- The pinned asset commit 87426a79611f9d212c4dc8af58e2834a05b93725 contains both
  rival atlases and their visible first-frame sprites; the actual files were
  fetched and inspected. Atlas frames use the existing renderer and animation
  ordering, with native trainer coordinates, origins and pair offsets.
- Real app encounters require trainer keys, check rival variant keys and exclude
  trainer art on wild encounters. The full normal app/scanner/custom/save suite
  passes without lowering coverage floors.
- The actual 3DS app builds with its strong 512 KiB stack override. All 2,260
  compiler frame reports pass; the largest frame remains 45,056 bytes.

To regenerate the table, copy tests/trainer-art.producer.test.ts into the pinned
upstream's test/tests/zz-dump/, run it there under Node 22 with TRAINER_ART_OUT set
to an absolute JSON output path, then run:

```sh
python3 game/tools/build_trainer_art.py observations.json game/gen/gm_trainer_art.c
JOBS=2 game/tests/run_host_tests.sh
```

This restores battlefield visibility. Larger dialogue portraits, expressions,
seasonal replacements and the full trainer throw/return animation remain separate
presentation work. No battle logic or save schema changes, and no full
battle-core completion claim.
