# PokeRogue 3DS: plan to 10 October 2026

Written 2026-09-28 (copy of /mnt/project-files/beta-plan/beta-plan.md, kept here for sessions that can't read the project files) from repo main `4195d37` (no open PRs) and upstream `pagefaultgames/pokerogue` at the pin `8555c08`, checked out to size what's left.
Tags: **[measured]** means I counted or ran it. **[inferred]** means it's my estimate. The time estimates are all inferred.

Goal (lorenzo, 2026-09-28): the whole game, getting as far as we can by 10 October. The work is ordered so there is always a build lorenzo can play, and each milestone adds to the one before.

## 1. The honest verdict

| By 10 October | My estimate [inferred] |
|---|---|
| **M0, battle sandbox on the console**: pick a team, fight wild and trainer singles battles with real sprites, text, menus, music and enemy AI | Likely. Target: 3 to 5 October |
| **M1, playable Classic singles run**: title, starter select, waves with wild and trainer battles, catching, rewards and shop, exp, level ups, new moves, evolution, biome changes, save and resume, game over | Possible, but I'd put it at roughly one chance in three to a coin flip. It depends mostly on how much the usage limit pauses the threads |
| **Everything**: doubles, form changes, Mega and Tera, mystery encounters, eggs, Pokédex, achievements, challenges, daily and endless modes, your custom content | **No.** This comes after the 10th, and it's weeks of work, not days |

Why "everything" can't make it. Upstream still has large parts with no C port at all [measured, line counts at `8555c08`]:
- UI: 39,081 lines. Starter select alone is 3,311, party 2,341, summary 1,413, reward select 1,062, and the Pokédex 5,326.
- Mystery encounters: 17,454 lines (31 encounters).
- Modifiers: 7,767 lines (the item effects and reward pools).
- Save data: `game-data.ts`, 2,071 lines.
- Eggs 623 and challenges 1,702.

The battle port also still lacks form changes, held items outside the damage calculation, about 40 ability and 36 move effects, doubles, catching and fleeing (from its own README and to-do list).

What the date depends on, bluntly:
1. **The usage limit is the real ceiling, not the number of threads.** Every thread, and lorenzo's own outside sessions, draws on one Claude plan. It paused all work for about 2 hours on 26 September and about 4 hours on 28 September. More threads remove waiting between steps, but they don't add hours to the day. With 6 or 7 sessions running, the pauses will probably get longer [inferred]. The only thing that raises the ceiling is a bigger plan. I can't see which plan lorenzo is on.
2. **The UI has no diff test.** The logic threads prove each piece against upstream, with 0 mismatches. Screens can only be checked by eye in Azahar and on the console, so bugs will surface in play.
3. **Everything meets in one app for the first time around 4 to 6 October.** Integration is where schedules slip.
4. **Console testing needs lorenzo, and it needs the packs on his SD card** (section 8). That's a new step for him, and it could fail the first time.

## 2. What exists and what's missing for M1

| Piece | Exists (repo main) | Missing for M1 |
|---|---|---|
| Battle logic (`battle-core/`) | Damage path verified. Singles turn engine replays 96% of test turns exactly [measured by that thread]. It has 87 of 95 battler tags, switching, weather and terrain, and hazards | **Message text and animations**: every message phase is a bare placeholder with no text or arguments (`pr_queue_message`, `src/engine.c:477`), so the game has nothing to show. Also battle start (summon, entry abilities), boss HP segments (`PR_FEAT_BOSS_SEGMENTS` is unported), a trainer sending its next Pokémon, catching, fleeing, held items and berries on the turn path, and form changes |
| Enemy AI | None | `EnemyPokemon.getNextMove` and its targets, 51 move benefit-score methods, and trainers' switch decisions (about 1,500 upstream lines [measured, approx.]) |
| Run logic (`run-core/`, lorenzo's outside sessions) | Waves, biomes, trainer and wild rolls, enemy rolls up to the moveset, enemy held items, shop prices, exp curves, Pokerus, luck, daily seeds | **The enemy's moveset** (`ai-moveset-gen.ts`, 1,378 lines). Until it lands, no enemy is complete. Also a wild battle's later enemies, trainers' `partyMemberFuncs`, and fixed Classic battles (rivals, bosses) |
| Rewards | Shop prices and reroll cost (run-core) | The reward options after each wave (player item pools, weights, luck upgrades) and what each item does when picked or used |
| The run loop | None | Everything between turns: new run, starters, encounter, commands, victory, exp, level up, learning moves, evolution, money, rewards, next wave or biome, game over, and saving and resuming a run and the unlocked starters |
| The 3DS game (`assets-pipeline/app-src/`) | Renderer (sprites, recolours, 9-slice windows, fonts), pack reader, music and sound effects, SD saves with backup, and a demo scene | Every game screen: title, save slots, starter select, the battle screen that plays turns back (text box, HP and EXP bars, sprite animation, info boxes), fight, bag and ball, party, summary, reward select, learn move, evolution, biome map, game over. Also battle animations, per-biome music and loading between scenes |
| Packs on lorenzo's SD | The builder exists (`assets-pipeline/build_sd.py`, 848 MB output) | lorenzo can run it only if it becomes a one-click download (section 8). There's also a known pack bug: battle-animation backgrounds are sliced into 96x96 cells (`bench-gfx/README.md`) |
| Speed on the console | 1.51 ms median singles turn; busiest battle scene 8.8 ms of 16.7 ms [measured on lorenzo's Old 3DS] | Pack loading and audio cost on the console (needs packs on the SD) |

## 3. Milestones (each one is a build lorenzo can play)

- **M0, battle sandbox (target 3 to 5 October).** Choose up to 6 Pokémon from a list, then fight random wild or trainer singles battles with enemy AI, real text, sprites and music. Effects that aren't ported yet do nothing and are written to `sd:/3ds/pokerogue/unported.log`.
- **M1, Classic singles run (target 8 to 10 October, at risk).** The list in section 1. Doubles are forced off (a double roll becomes a single battle; this is a known deviation). Mystery encounters are off, and so are eggs. Fixed Classic battles come in if run-core has them in time; otherwise they're regular trainer battles.
- **M2 (after the 10th, about 1 to 2 weeks [inferred]).** Doubles, form changes, Mega, Tera, Transform, the remaining ability and move effects, fixed battles, and Endless and Spliced Endless modes.
- **M3 (weeks after M2 [inferred]).** The 31 mystery encounters, eggs (gacha, hatching, egg moves), Pokédex, achievements and vouchers, run history, game stats, challenges, the daily run, settings, and other languages (their text and CJK fonts are already in the packs).
- **M4.** lorenzo's custom content (reserved ids from 30000, a small pack loaded at startup; see `battle-core/README.md`).

## 4. Workstreams (one thread each, separate folders)

A thread that finishes a slice goes straight on to the next one, with no "ready?" checks. Each thread reads other folders but never edits them, and cross-folder requests go through the coordinator.

| # | Thread | Owns | First | Then | Needs from others |
|---|---|---|---|---|---|
| 1 | **Battle logic** (existing) | `battle-core/` | Section 5 order | Section 5 order | none |
| 2 | **3DS game app** (new) | `game/` (new), `assets-pipeline/app-src/` (the rr, pak, snd and save libraries; bench-gfx and bench-native keep reading them) | Framework: screen stack, input, windows and menus, text through the real fonts, English strings from `data.pak`. A Makefile linking battle-core, run-core, ai-core, reward-core and game-core. The battle screen playing back a fake event stream | Battle screen on the real event stream, the fight, ball and party menus, then M0. Then title, save slots, starter select, reward select, summary, learn move, evolution, biome map, game over, audio per biome, loading between scenes, and the battle animation interpreter | Event header (thread 1, day 1), run API header (thread 3, day 1) |
| 2b | **Menus** (new, from about 1 October) | `game/menus/` | Starts once thread 2 says the framework is in. Takes starter select, party, summary, reward select, learn move and the biome map, so the UI isn't one serial queue | | Thread 2's framework |
| 3 | **Run loop** (new) | `game-core/` (new) | A headless run: run state, starters (costs, default unlocks, starting moves), encounter (run-core), commands, enemy commands (ai-core, with a random-move stub until it lands), the turn (battle-core), victory, exp (run-core), level up, learning moves, evolution checks, money, rewards (reward-core, with a stub until it lands), next wave or biome, and game over. A PC text driver and **random soak runs** (thousands of seeded runs, looking for crashes and for which unported effects real runs hit most) | Save and resume (session plus system data, versioned; saves only between battles, never during animations). The dex and starter unlocks when catching. Optional: import lorenzo's browser save, if he has one | Battle API (thread 1), moveset (run-core), AI (4), rewards (5) |
| 4 | **Enemy AI** (new, about 2 to 3 days) | `ai-core/` (new) | `getNextMove` (RANDOM, SMART_RANDOM and SMART), `getNextTargets`, the move benefit scores, `getMatchupScore`, and trainers' switch decisions in `EnemyCommandPhase`. Diff-tested against upstream the way run-core is | Ends; its slot goes to thread 2b | battle-core read-only |
| 5 | **Rewards** (new) | `reward-core/` (new) | The player's reward pools and weights, luck upgrades and `getNewModifierTypeOption`'s player branch, reward options per wave, reroll and lock (prices are already in run-core), and trainers' `genModifiersFunc`. Diff-tested | What each reward does when picked or used: healing, revives, PP items, TMs, evolution items, vitamins, Rare Candy, held items into the battle's modifier list, lures, charms, exp items. Rare items (form-change items, DNA Splicers, Mega and Tera) wait for M2 | battle-core and run-core read-only |
| 6 | **SD card builder** (new, about 1 to 2 days) | `sd-builder/` (new), `assets-pipeline/*.py` | The one-click builder in section 8, then fixing the battle-animation background slicing in `gfx.py` | lorenzo builds his packs and runs bench-gfx once more with them (load times and audio cost). Then the thread ends | none |
| 7 | **run-core** (lorenzo's outside session, not a project thread) | `run-core/` | Enemy moveset (critical path) | A wild battle's later enemies, trainers' `partyMemberFuncs`, fixed Classic battles, then which mystery encounter spawns. **It skips the reward options**, because thread 5 takes them | none |

Headers first. On day 1, threads 1, 3, 4 and 5 each publish the header the game will call, with stubs behind it. That way the app can be built against them before the logic is done. The shapes:
- battle-core: a battle-event struct (message key and arguments, animation ids, HP, status and stat changes, ability pop-ups, faints, switches).
- game-core: the run API (current screen, the choices on offer, apply a choice).
- ai-core: choose an enemy command.
- reward-core: the options for this wave, apply a choice.

Suggested start order: 1 (already running), then 2, 3, 6, 4 and 5 now, then 2b once thread 2 says the framework is in. That makes 6 project threads plus lorenzo's outside session. If the usage limit keeps pausing everything, follow the pause order in section 7.

## 5. What the battle thread should do, in order

1. **Battle events for the screen.** Each message phase gets its locale key and arguments. The turn test checks them by comparing the English text upstream actually shows, so it stays diff-tested. Also add animation events (move animation, common animations, user and target), plus HP, status and stat changes, ability pop-ups, faints, and switches in and out. Publish the event header on day 1. Without this, the game can only show numbers.
2. **A battle the game can drive.** Start a battle from run-core's enemies: summon, the entry abilities at battle start, and the phases before a turn. Then take both sides' commands, run a turn, pause for switch prompts (this exists), handle a forced switch after a faint, send a trainer's next Pokémon, apply boss HP segments and their stat boosts, and report the outcome (won, lost, fled, caught). Unported effects do nothing and are reported to the caller.
3. **Catching** (`AttemptCapturePhase`: ball multipliers, catch rate, shakes, critical capture) and **fleeing** (`AttemptRunPhase`).
4. **Held items and berries on the turn path**: Leftovers, Shell Bell, the berries, Focus Band and Sash, King's Rock, Quick Claw, the Orbs, White Herb. Rewards hand these out every few waves.
5. **Form changes and Transform/Imposter.** Do the common in-battle ones first.
6. **The remaining ability and move effects**, ordered by how often thread 3's soak runs hit them, not by the test modes.
7. **Doubles and the last 8 lingering effects** (M2).

To save the shared budget, I suggest running the planted-bug (mutation) checks only as spot checks until after 10 October, while keeping every 0-mismatch diff run. That's a quality trade-off, and it's for the battle thread and lorenzo to decide.

## 6. Critical path, day by day [inferred; it assumes the threads start 29 September]

| Dates | What lands |
|---|---|
| 29 to 30 Sep | The headers and stubs (battle events, run API, AI, rewards). The app framework. The SD builder, which lorenzo then runs. The battle event stream is under way. The run-core enemy moveset (outside session) |
| 1 to 2 Oct | The battle screen plays real turns. The battle API and battle start. AI move choice. The run loop plays whole seeded runs on the PC with stubs. The reward pools. **Console check 1:** bench-gfx with the packs |
| 3 to 5 Oct | Catching and fleeing, held items and berries, trainer battles. **M0 build on the console (console check 2).** Starter select and the reward screen. Save and resume |
| 6 to 8 Oct | M1 put together: exp, level ups, learning moves, evolution, biome map, game over, music per biome. Soak runs and crash fixes. **M1 candidate on the console (console check 3)** |
| 9 to 10 Oct | Fixes from lorenzo's play, the battle animation interpreter if there's time, and the beta build |

The longest chain is the game app: about 13 thread-days of screens [inferred], split over threads 2 and 2b, plus about 2 days of integration.

## 7. When the usage limit bites

**What to pause first, in this order:**
1. Mutation checks everywhere.
2. The battle effects past what the soak runs hit.
3. Diff-test depth in AI and rewards (keep the ports).
4. Rare reward items.
5. The battle-animation interpreter.

**Never pause** the game app, the run loop, or the battle events and API. They're the critical path.

**The cut list for M1, if it's late (first cut first):**
1. Missing effects stay skipped and logged.
2. Move animations become simple flashes and shakes.
3. The evolution scene becomes a plain "X evolved into Y!" message.
4. Fixed Classic battles become regular trainer battles.
5. The summary screen shows stats and moves only.
6. Starter select becomes a plain list with the cost budget, with no sorting or filters.
7. Italian and the other languages wait.

**Not cut from M1:** saving and resuming, catching, rewards, exp and levels, trainer battles, music.

## 8. The packs: the simplest path for lorenzo

The problem: the game can't draw or play anything without about 850 MB of converted art, sound, music and fonts on the SD card (the "packs"). The project's rule is that these are never shared or uploaded (Nintendo's and PokeRogue's content, `README.md`), so each person builds their own from PokeRogue's public source. Today that means installing Python, four Python libraries, ffmpeg and a C compiler, then running a command against three downloads. That's too much to ask.

The fix (thread 6) is **one download and one double-click, with nothing to install**. It's a small zip for Windows. The zip holds no Pokémon content, only the tool:
- a portable Python with its libraries
- ffmpeg
- the pipeline scripts
- the audio encoder, prebuilt

Running it downloads PokeRogue's public files at the pinned versions, builds the packs, writes them straight to the SD card, and checks for the sound file the 3DS needs. It should also offer to put the game's `.3dsx` in place. A Mac version comes second if lorenzo needs one. Thread 6 tests the Windows zip under Wine before handing it over. It should also settle which assets version to pin: the packs were built from `056a1f4`, while the game's `8555c08` points its assets at `87426a7`.

Fallback if the zip fails on his PC: Remote Control. lorenzo adds a folder in the Claude desktop app, and a Claude session on his own PC does the build while he approves each step.

### The steps for lorenzo (draft: thread 6 fills in the real link and file names)
1. You need your Windows PC, your 3DS's SD card in the PC (a card reader, or the PC's own card slot), about 3 GB free on the PC and 1 GB free on the SD card, and about an hour, mostly waiting.
2. Download **PokeRogue-SD-Builder.zip** from the link I'll post in the thread.
3. Right-click the zip, choose **Extract All**, then **Extract**.
4. Open **This PC** in File Explorer and note the SD card's drive letter (for example `E:`).
5. Open the extracted folder and double-click **BUILD-SD-CARD**. If a blue "Windows protected your PC" box appears, click **More info**, then **Run anyway**. It appears because the tool isn't signed.
6. When it asks, type the SD card's letter (for example `E`) and press Enter.
7. Wait. It downloads about 1 GB of PokeRogue's public files, converts them (roughly 20 to 60 minutes, depending on the PC), and copies them to the SD card's `3ds\pokerogue` folder. It says **Done** when finished. You can use the PC meanwhile.
8. If it says the sound file is missing (`dspfirm.cdc`), follow the three steps it shows. They run a small tool called DSP1 once on the 3DS, from the Homebrew Launcher. Many modded consoles already have this file.
9. Eject the SD card safely, put it back in the 3DS, and open the game from the Homebrew Launcher.
10. Keep these files to yourself. They're only for your own console.

## 9. What lorenzo is asked to do, and when

| When | What | Time it takes him |
|---|---|---|
| About 30 Sep | Run the SD builder (section 8) | About 1 hour, mostly waiting |
| About 1 to 2 Oct | Run bench-gfx once more, now with the packs | 10 minutes |
| About 3 to 5 Oct | Play the M0 battle sandbox and report anything wrong. `unported.log` on the SD card helps | As long as he likes |
| About 8 Oct | Play the M1 candidate | Same |

Nothing else needs him, unless a thread asks a question only he can answer.

## 10. What I don't know
- Which plan lorenzo is on, and how long the pauses will get with 6 or 7 sessions running.
- Whether lorenzo's PC runs Windows. Section 8 assumes it does, and I've asked him.
- Pack load times and audio cost on the real console (bench-gfx measured them only in Azahar).
- How many unported effects a real run hits. The soak runs will show it on about 2 October.
- Whether lorenzo has a browser save worth importing (it would carry his unlocked starters).
