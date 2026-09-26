# Headless battle benchmark: results so far (2026-09-26)

Upstream `pagefaultgames/pokerogue` @ `8555c08`, locales `270ed0a`, data JSONs from assets `beta` @ `056a1f4` (the audit used `87426a7`; only JSON data is used here).
QuickJS (bellard) 2026-06-04 @ `04be246`. The same bytecode file runs on every QuickJS host below.

## What runs
Real upstream game code: `initializeGame()` (all species, moves, abilities, items), then a Classic run driven by upstream's own test harness (`GameManager`, Phaser in HEADLESS mode, upstream's sprite/text mocks). Enemy AI, damage, abilities, items, rewards screen logic, wave generation, trainer battles all execute. English locales bundled.
- Party: Garchomp, Blaziken, Greninja, Gardevoir, Corviknight, Toxapex at level 100 with normal generated movesets; the bench picks the best-effectiveness attack each turn and skips rewards.
- Scenario A: 25 turns of single battles from wave 21. Scenario B: 17 turns of double battles from wave 51.
- `Math.random` is seeded, so every host plays exactly the same battles (verified: identical phase sequence in Node and QuickJS).

Not included (so the real game will cost more): rendering, audio decode, move-animation data (stubbed like upstream tests), per-frame tween/UI updates at 30-60 fps, save I/O. Tweens and timers complete instantly.

## Numbers
"Turn" = from choosing moves to the next command menu. "Wave-end turn" also includes victory, rewards phase and generating + summoning the next encounter.

| Host | Turn median | Wave-end turn median | Worst turn | All turns median / p90 | JS heap peak |
|---|---|---|---|---|---|
| Node 22 (V8 JIT), Xeon 2.1 GHz | 10 ms | 18 ms | 46 ms | 15 / 26 ms | n/a (GC'd heap ~75 MB) |
| QuickJS x86-64, same PC | 30 ms | 39 ms | 139 ms | 37 / 80 ms | 61.3 MB |
| QuickJS x86 32-bit, same PC | 40 ms | 50 ms | 179 ms | 46 / 104 ms | **41.6 MB** |
| .3dsx in Azahar emulator, Old 3DS mode | 715 ms | 1.14 s | 4.15 s | 960 ms / 2.21 s | 41.6 MB |
| **Real Old 3DS (lorenzo's console)** | **2.70 s** | **3.31 s** | **11.89 s** | **2.98 s / 7.03 s** | 41.6 MB |

Split by battle type on the emulator: singles 375 ms per turn and 849 ms per wave-end turn (median); doubles 1.35 s and 2.18 s.
Startup on the emulator: 0.5 s to load bytecode, 4.9 s module init, 9.6 s `initializeGame`, 23 s to reach the first battle.
Memory (32-bit, i.e. what the 3DS uses): 11.7 MB after loading bytecode, 25.5 MB after all game data, 32 MB in the first battle, then about +0.2 MB per turn with no drop after a full GC. At least part of that growth is the test mocks keeping destroyed sprites (seen: every Poke Ball particle stays in a mock container); I did not prove the rest isn't a real game leak.

## How much to trust the emulator
Azahar counts emulated CPU time from executed instructions at 268 MHz and does not model cache misses or slow memory, so real hardware should be **slower** than the emulator row, not faster. By how much, I don't know; the real-console run answers it.

## Real console (added 2026-09-26)
The real Old 3DS played exactly the same battles as the emulator and PC (identical phase sequence) and was about 3.1x slower than the emulator. Singles took 1.21 s per turn and 2.64 s per wave-end turn (median); doubles took 4.82 s and 6.76 s, with a worst case of 11.9 s. Startup: 13.7 s module init, 32 s game data, 68 s to the first battle. The heap peaked at 41.6 MB of a 57.1 MB app heap. Raw output: `results/real-old3ds.txt`.
**Final call: the QuickJS route fails the gate on an original 3DS by roughly 3-7x.** Removing the harness overhead (15-20%) and tuning JS (maybe 25-30%, inferred) would still leave doubles at several seconds per turn.
The .3dsx now also turns on New 3DS speed mode (804 MHz + L2 cache) when run on one, so the same file can measure a New 3DS. That has not been measured.

## Call against the audit gate (roughly 1 s per turn and memory fits)
Original provisional call, before the console run: **fails on CPU for the QuickJS route on an Old 3DS.** Even the optimistic emulator numbers put doubles at 1.4-2.2 s per turn and 4 s worst case, before any rendering or audio work. Singles are borderline (0.4-0.85 s). Memory fits (about 42 MB of an ~56 MB heap), but leaves only ~14 MB for textures and audio.
If the real console confirms this, the audit's fallback applies: native port of the battle core (or a New 3DS target, which is out of scope for this user's hardware).

## Rebuild
`src/bundle` goes into `pokerogue/test/bench-3ds/` of an upstream checkout (plus `locales` and the JSONs listed in `shims/files.ts` from the assets repo), then `src/host/bundle.sh` builds `bench.mjs` with upstream's Vite config. `src/host/build.sh` builds the PC hosts; `compile32 bench.mjs bench.qbc` writes bytecode. The 3DS build is devkitARM's standard 3DS Makefile (`src/3ds/Makefile`) with QuickJS sources + `quickjs-3ds.patch`, `host.c` in `source/` and `bench.qbc` in `romfs/` (built here with the `devkitpro/devkitarm` Docker image).

## Where the turn time goes (profile, added after the first report)
The profile comes from Node in `--jitless` mode, which is an interpreter like QuickJS. It uses a source-mapped build and covers only the 42 benchmark turns; raw output is in `results/profile-node-jitless.txt`. The harness share was then checked directly in QuickJS 32-bit with an A/B run that played identical battles.
- **Test harness and mocks: about 15-20% of turn time.** The biggest part is upstream's `MockSprite.destroy()` calling into a real Phaser sprite, whose listener lists keep growing, so each destroy gets slower. Skipping it in QuickJS moved the all-turns median from 46 to 39 ms and the p90 from 107 to 86 ms. A real build would not pay this.
- **The remaining ~80% is real game code, and it is spread thin.** The hottest single game function takes about 2% self time. The biggest inclusive clusters (they overlap):
  - ability-trigger machinery (`applyAbAttrs`, ~13%)
  - i18next text lookups (~11% self)
  - stat calculation (`getEffectiveStat`, ~11%)
  - speed ordering (~10%)
  - enemy AI move choice (~10%)
  - damage calculation (only ~6%)
- **Native-C hotspots are not a useful third option.** No small, self-contained kernel dominates. The costly parts walk the JS object graph (Pokemon, 320 ability classes, modifiers), so moving them to C means porting that graph, which is the battle-core port under another name.
- **Pure-JS fixes could recover perhaps 25-30%.** That means no harness plus cached i18n lookups (inferred, not measured). On the emulator's optimistic numbers, that brings doubles to roughly 1.0-1.6 s per turn, which still misses the gate.
