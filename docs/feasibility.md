# PokeRogue on an original 3DS: feasibility audit

Source audited: `pagefaultgames/pokerogue` at commit `8555c08` (2026-09-23), assets submodule `87426a7`, locales `270ed0a`.
Every number below is tagged **[measured]** (I ran it), **[spec]** (published 3DS hardware facts from my knowledge, not re-verified this session) or **[unknown]**.

## Verdict

**Conditional go, with a hard gate.** A faithful, full, offline port to an *original* 3DS is not ruled out by memory or storage, but nobody knows yet whether the old 3DS CPU can run PokeRogue's game logic fast enough. That single unknown decides the whole project, and it can be measured cheaply before any real porting starts.

- **Native C/C++ rewrite: no-go as the primary route.** ~180k lines of non-table code (game logic plus UI; the ~122k lines of species tables could be converted by script) would have to be re-implemented and then kept in sync with an upstream that changes weekly. For one hobbyist this is years of work, and "faithful" would decay the moment upstream patches a move.
- **Recommended: keep the game logic in JavaScript, run it in an embedded engine (QuickJS), and replace Phaser with a small native 3DS renderer.** This keeps faithfulness almost for free (you re-bundle upstream), and concentrates native work on rendering, audio and input.
- **Gate:** before building any graphics, run a headless PokeRogue battle inside QuickJS on your actual 3DS and measure turn time and memory. If a turn takes more than ~1-2 s or the heap does not fit, fall back to the native rewrite of the battle core only (or target a New 3DS). Details in "Next step".

## What the game is made of

| Item | Number | Tag |
|---|---|---|
| TypeScript source | 641 files, 300,601 lines | measured |
| of which `src/data` (species, moves, abilities, trainers…) | 193,537 lines | measured |
| of which pure data tables (`src/data/balance/species/*`) | ~122,000 lines | measured |
| UI code (`src/ui`) | 39,081 lines | measured |
| Phases / field / modifiers | 13.7k / 10.3k / 7.8k lines | measured |
| Moves / abilities / species IDs | 952 / 320 / 1,084 | measured |
| Mystery encounters | 31 | measured |
| Production JS bundle (Vite build, Phaser included) | 6.2 MB minified, 19 chunks | measured |
| Assets total | 817 MB | measured |
| Audio | 443 MB (BGM 380 MB, 170 tracks, ~6.7 h at 128 kbps MP3; 1,200 cries 18 MB) | measured |
| Images | 316 MB, 16,862 PNGs (Pokemon sprites 276 MB) | measured |
| Battle animation data | 929 JSON files, 53 MB | measured |
| Logical resolution | 320x180 (rendered at 1920x1080 = 6x) | measured (`src/scene-base.ts:16`) |

## Original 3DS limits vs. the game

| Constraint | 3DS | PokeRogue | Assessment |
|---|---|---|---|
| Screen | 400x240 top, 320x240 bottom [spec] | 320x180 logical | **Good fit.** Battle at 1:1 on top screen with borders; menus fit the bottom screen width exactly. |
| App RAM | 64 MB default; a .cia can request extended memory modes (80/96 MB) [spec, verify on device] | QuickJS just *loading* the bundle: 13.9 MB (stripped) to 37 MB (with debug info), before any game objects exist [measured, x86-64 build] | Tight but plausible. Runtime heap after data init is **[unknown]** (my Node measurement was polluted by the test harness, so I'm not quoting it). 32-bit ARM should be somewhat smaller than the x86-64 figure. |
| CPU | ARM11 @ 268 MHz, one core usable, no JIT in QuickJS [spec] | QuickJS parses the bundle in 0.57 s on a modern x86 core [measured] | **The big unknown.** Rough extrapolation (20-40x slower per core) gives ~10-25 s to parse; avoidable by shipping precompiled bytecode. Per-turn logic cost on the 3DS is **[unknown]**. |
| GPU | PICA200, fixed-function fragment pipeline (no pixel shaders), 6 MB VRAM, max texture 1024x1024 [spec] | 4 custom GLSL shaders (day/night tint, shadows, tera shimmer, **per-pixel palette swap for variants and fusions**) | Tints: easy with TEV combiners. Palette swaps and fusions: must be done on the CPU when a Pokemon is loaded (write recolored texture). Tera shimmer: approximate. A few sheets exceed 1024 px (e.g. Landorus-Therian 1175x1175) and need repacking. |
| Sprite memory | — | All Pokemon sprites decoded RGBA: 2.19 GB total; per front sheet median 0.08 MB, p90 0.64 MB, frame median 65x65, up to 360 frames [measured] | **Fine with streaming.** A double battle's working set is a few MB; halve it with RGBA5551. |
| Audio | DSP mixing, decoding in software on the ARM11 [spec] | 6.7 h of MP3 BGM with loop points in `biome-bgm-loop-points.json` | Fits on SD easily. Decode cost of MP3/Vorbis on the old 3DS CPU competes with the JS engine: **[unknown]** how much. |
| Storage / load speed | SD card; read throughput **[unknown]** for this workload | Load-on-demand design already exists in the web build | Pipeline should pack per-species files so one encounter = a few small reads. |

## How tangled is the game logic with Phaser?

- Phaser is referenced in 140 files; 285 files use the global `globalScene`. [measured]
- 159 files in data/field/modifier/phases touch `globalScene`; 34 of them reference Phaser directly; 141 tween calls sit in phases/data/field. [measured]
- The Phaser surface actually used is narrow: mostly `Sprite`, `Container`, `Text`, `Image`, `NineSlice`, `Rectangle`, tweens, timers, keyboard/gamepad input, sound. Plus `phaser3-rex-plugins` (BBCode text). [measured]
- Upstream already runs full battles headless (Phaser HEADLESS + jsdom) in its test suite; I ran one successfully. That is evidence the logic can run without a real renderer. [measured]

So: logic and presentation are **not** cleanly separated (a native rewrite can't just lift a "core" folder), but the rendering API it depends on is small enough to re-implement as a shim. This is the main argument for the QuickJS route.

## Offline

Mostly already there. `VITE_BYPASS_LOGIN=1` (`src/constants/app-constants.ts:21`) runs the game without an account and saves locally; the server API lives in `src/api/` (account, admin, daily, savedata). Work needed: replace `localStorage` (85 call sites) with SD-card file I/O, and generate the daily-run seed locally from the date instead of fetching it. Leaderboards simply go away.

## License and IP (not legal advice)

- **Code: AGPL-3.0-only.** A port is a derivative work, so it must also be AGPL. If you ever share a build, you must share its full source. The "network use" clause doesn't matter for an offline game. Keeping it for yourself triggers no obligations.
- **PokeRogue logos and intro video: "NO-REUSE"** (`assets/REUSE.toml`). Don't use them in the port's branding.
- **Pokemon sprites, music and cries** are Nintendo / Creatures / GAME FREAK IP; upstream only claims fair use for its own website and explicitly doesn't relicense them. That claim does not transfer to you. Safest practice: never distribute a build or pack containing assets; have the asset pipeline convert them on your own PC from the upstream repo.

## Rough effort (inferred, not measured)

- QuickJS route: renderer + Phaser shim, audio, input, save I/O, asset pipeline. Many months of hobby time; rebasing on upstream stays cheap.
- Native rewrite: all of the above plus ~180k lines of logic and UI (species tables can be auto-generated). Plausibly years, and perpetually behind upstream.

## Next step (the gate)

1. Bundle upstream's game logic with a Phaser/DOM stub (upstream's headless test setup is a template) and run a scripted battle in QuickJS on a PC; record heap after data init and per-turn CPU time.
2. Build the same thing as a .3dsx and run it on your 3DS; record the same numbers plus startup time from precompiled bytecode.
3. Decide: turn under ~1 s and heap leaves room for ~15-20 MB of textures/audio → proceed with the QuickJS route. Otherwise → native battle core, or accept a New 3DS as the target.

## What I don't know

- QuickJS heap size once all species/moves/abilities are instantiated.
- Per-turn and per-frame CPU cost on a 268 MHz ARM11.
- Whether a .3dsx launched from the Homebrew Launcher on your setup gets 64 MB or less, and whether a .cia build can reliably use the 80/96 MB modes on your firmware.
- Real SD read throughput and audio decode cost on the old 3DS.
