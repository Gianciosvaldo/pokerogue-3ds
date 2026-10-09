# PokeRogue 3DS Parity Ledger and Upstream Drift Matrix

**Port Target:** `pagefaultgames/pokerogue` pinned commit [`8555c08c823b856cbec4eb99ca84ea52a955836d`](https://github.com/pagefaultgames/pokerogue/commit/8555c08c823b856cbec4eb99ca84ea52a955836d) (short `8555c08`).
**Public Beta Baseline (Inspected Separately):** `pagefaultgames/pokerogue` commit [`4f0969875845fb412c5b0af8ed4bc006484a8f2a`](https://github.com/pagefaultgames/pokerogue/commit/4f0969875845fb412c5b0af8ed4bc006484a8f2a) (42 commits ahead of target `8555c08`). Public beta mechanics are outside the port target and are not mixed into pinned parity claims.
**Initial ledger baseline:** `58030d6` after Task 57 merged. **Current status refresh:** main `c3bfa8b33a4180b13d1385e8878f21fcff4cb94a` after [#183](https://github.com/Gianciosvaldo/pokerogue-3ds/pull/183), 2026-10-09. Merge/final-head CI metadata, owning source/READMEs and durable lane validation logs were inspected. Counts below are recorded implementation evidence; this documentation task did not rerun producers. Historical matrix counts remain dated evidence and do not describe a fresh exhaustive current-main comparison.

---

## 1. Subsystem Parity Matrix

| Subsystem | Function / Feature | Supported Scope | Source Path | Commit Target | Test Command | Count / Result | Exclusions / Excluded Scope |
|---|---|---|---|---|---|---|---|
| `battle-core` | Singles & Doubles Turn Engine & Damage Path | Damage path, 87/95 battler tags, move effects, stat calculation, JS math (`Math.pow` V8), weather/terrain, called moves, header moves, delayed moves, switching, Weather forms (Task 69), Item removal (Task 70), Special berries (Task 71), Dancer dispatch (Task 72), Doubles first slice (Task 73) | `battle-core/src/*.c`, `include/pr/*.h` | `8555c08` | `turntest`, `weatherformtest`, `itemremovaltest`, `berrytest`, `dancertest`, `doublestest` | Plain 1,534/1,534, RICH 3,640/3,640, STAT 3,762/3,762, STATUS 3,818/3,818, ONHIT 11,767/11,767, CHARGE 3,948/3,948, TAGS 7,876/7,876, HEAL 7,716/7,716, WEATHER 7,824/7,824, BOOST 7,859/7,859, TRAPS 7,873/7,873, PROTECT 7,630/7,630, ARENA 7,385/7,385, EFFECTS 7,794/7,794, CALLED 7,781/7,781, HEADER 7,692/7,692, DELAYED 7,610/7,610, SWITCH 7,913/7,913, SUMMON 7,678/7,678 (all 0 mismatches) | Natural Illusion ability dispatch, Tera precommands execution |
| `battle-core` | Command Screen Probes (`pr_cprobe`) | Command validations (`pr_fight_move`, `pr_move_usable`, `pr_move_selectable`, `pr_is_trapped`, `pr_can_switch`, `pr_can_run`) | `battle-core/src/commands.c` | `8555c08` | `TURN_CPROBE=1` with `turntest` | 56,155 probes, 0 mismatches | None for command probe API |
| `battle-core` | Battle Initialization (`pr_start_battle`) | Encounter setup, wild and trainer battle start sequences, Set/Switch style switch prompt checks | `battle-core/src/phases.c` | `8555c08` | `TURN_START=1` (wild & trainer) with `turntest` | Wild 7,646/7,646, Trainer 7,673/7,673, 0 mismatches | Doubles battle start |
| `battle-core` | Held Items & Berries (Slice 1) | Leftovers, Shell Bell, Focus Band, Quick Claw, King's Rock, White Herb, Sitrus Berry, Lum Berry, Leppa Berry | `battle-core/src/modifiers.c`, `src/phases.c` | `8555c08` | `TURN_ITEMS=1` with `turntest` | 2,000+ cases per seed (777, 9001, 4242, 31337), 0 mismatches | Broader held-item families; Enigma/Lansat/Starf landed separately in Task 71 / [#115](https://github.com/Gianciosvaldo/pokerogue-3ds/pull/115); pinch berries already have dispatch |
| `battle-core` | Mid-battle Form Changes | Aegislash Stance Change, Darmanitan Zen Mode, Minior Shields Down, Morpeko Hunger Switch | `battle-core/src/pokemon.c` | `8555c08` | `TURN_FORMS=1` with `turntest` | 2,000 cases per seed, 0 mismatches | Broader form families; Forecast/Flower Gift/Ice Face landed separately in Task 69 / [#110](https://github.com/Gianciosvaldo/pokerogue-3ds/pull/110) |
| `battle-core` | Mega Evolution & Dynamax | Mega Evolution and Dynamax/Gigantamax battle triggers, stat multipliers and move overrides | `battle-core/src/engine.c` | `8555c08` | `TURN_MEGA=1`, `TURN_DYNAMAX=1` with `turntest` | 2,000 cases per seed, 0 mismatches | Multi-target Max Move spread dispatch |
| `battle-core` | Fused Pokemon | Fused stats, typing, ability, and move execution in battle | `battle-core/src/pokemon.c` | `8555c08` | `TURN_FUSION=1` with `turntest` | 2,000 cases per seed, 0 mismatches | DNA Splicers reward lifecycle in game-core (Task 62) |
| `ai-core` | Enemy AI Move & Matchup Scoring | Recorded singles coverage and merged Stuff Cheeks restriction | `ai-core/src/*.c`; `ai-core/README.md` Task 88 | `8555c08` | Existing 5000-case producer per seed 777/9001/4242/31337, Node 22.23.3; exact commands in [#102](https://github.com/Gianciosvaldo/pokerogue-3ds/pull/102) | 4938/4946/4942/4936 covered/identical, 0 mismatches; total 19762, 238 uncovered | Node 24: historical 2/9/9/12 failures; declared upstream Node >=24.9.0 conflicts with HANDOFF Node 22. Natural Illusion and disguise-in-doubles remain; loaded helpers landed in Task 68 / [#117](https://github.com/Gianciosvaldo/pokerogue-3ds/pull/117), doubles command slice in Task 74 / [#127](https://github.com/Gianciosvaldo/pokerogue-3ds/pull/127) |
| `run-core` | Run Generation Suites | Waves, biomes, trainer species, wild encounters, shop prices, exp, party exp, upkeep, luck, daily run rules, gym/E4 fixed teams, rival teams (Task 48), Eternatus & Paradox fixed teams (Task 50 / PR #92) | `run-core/src/*.c`, `gen/out/*.c` | `8555c08` | `run-core/tools/difftest.sh <upstream> <N> <seed> <suite>` | Wave (20k), shop (20k), biome (20k), encounter (20k), newbattle (20k), exp (20k), partyexp (20k), upkeep (20k), trainerspecies (20k), luck (20k), daily (20k), battlegen (10k), pokemongen (8k), trainerparty (3k), rival (3k / 12k total in PR #75), gym_e4 (3k), wildenemy (3k), enemyitems (3k), enemybuffs (2k), challenge (2k) - historical scopes, not a fresh complete R pass; [Task 63 checkpoint](../run-core/TASK63-R-REGRESSION.md) explicitly retains pending commands | Broader moveset rules, trainer constructor RNG boundaries and full-enemy generation; Task 63 bounded generator merged in [#112](https://github.com/Gianciosvaldo/pokerogue-3ds/pull/112) |
| `reward-core` | Options, victory and application | Merged options, application/evolution-request repairs (Tasks 89/91), separate TM/relearner inputs, candy and status cures, trainer rewards (Card 97) | `reward-core/src/*.c`; [owning README](../reward-core/README.md) | `8555c08` | `tools/difftest.sh <upstream> 4000 99 all`; focused evolution/learnset/trainer suites in owning README | Recorded repair: options 3950 pass / 0 fail / 50 unported; victory/apply/Memory 4000 each, zero failures; bounded evolution 8000/8000. These are historical recorded results, with runtime versions in the owning README | Bounded Mint/vitamin/Lure APIs now landed (rows below); broader stat families, complete form/fusion execution and inventory/phase lifecycle; whole-run upstream parity remains unvalidated |
| `game-core` | Headless Classic Run Loop & Save System | Merged save/item bridge, reward options/healing, bounded evolution/form lifecycle, turn continuation, boss segments, catching/fleeing, TM/Memory, starter gates, candy, balls/Full Heal, bounded Mint, Orb inventory mapping and bounded wild movesets | `game-core/src/*.c`; [README](../game-core/README.md) | `8555c08` | `make -C game-core test`; `game-core/build/gcplay --auto 1000 --jobs 8` with/without `--save-test` | Task 64 / #145: 29,493 adapter checks; 1000 normal + 1000 saved ended, 0 cleared/stuck/crashed/timed out; 49,315 turns each; digests `5f646aee` / `5f646aee`. [Exact commands and scope](../game-core/TASK64-VALIDATION.md) | Remaining Task 96 items; broader Task 61 and fusion 62; full movesets 64, fixed trainers 65, final boss 66, doubles/weather 75/76, win progression 79, eggs 80/81, mystery comparator 82, mode entries 83 and metadata 84 |
| `game` | Dual-Screen UI & Battle Presentation | Run-driven battle/reward/shop/biome screens, title/save/results, catching/release flow and evolution prompt (Cards 110/112/113/114/116) | `game/source/*.c`, `game/menus/*.c`; [README](../game/README.md) | `8555c08` | `make -C game/tests runtest saves_test flowtest`; run the resulting binaries | Task 64 host: saves 68 checks / 0 failures, flow 45 / 0; 1000-run app check FAILED rare evolution coverage (1 completed / 1 post-evolution move); expanded 5000-run check passed with 15 completed / 31 cancelled / 15 move checks and zero model violations | Host tests do not show rendering; full starter/UI/audio/presentation and hardware acceptance remain. Keep the 1000-run coverage failure visible; [validation](../game-core/TASK64-VALIDATION.md) |

### October 7–8 bounded item slices (recorded Node 22.23.3 evidence)

Each focused differential uses seeds `777 9001 4242 31337`, 2000 cases each,
and pin `8555c08`. The scopes are distinct and their counts are not summed into
an overall completion percentage. Exact commands, transport and exclusions are
in the linked owning records.

| Owner / merged slice | Recorded comparison and guards | Remaining boundary |
| --- | --- | --- |
| Reward Mint #147 | 8000/8000 identical; 33 guards; original/custom/effective nature, six stats/HP and unlock requests | Guards exclude challenges and selected stat inventories; caller owns dex policy |
| Battle Orbs #148 | 8000/8000 full turns and texts identical; 14 guards | TurnEnd self-status only; broader acquisition/removal/theft excluded |
| Battle Wide Lens #149 | 8000/8000 full turns/texts and accuracy probes identical; 16 guards | Amount five, effective real/virtual stacks; acquisition/game bridge excluded |
| Reward vitamins #150 | 8000/8000 identical; 31 guards; IV cap, whole-party stats/HP, purchase flags | Paid addition reports error without money deduction at pin; caller lifecycle excluded |
| [Game Mint #151](../game-core/MINT-VALIDATION.md) | 8000 cases / 16000 free+bought comparisons, 0 mismatches; 107 host checks, 113 with pre-change save | All 54 modeled party fields, fixture inventory/profile and RNG; constructor-stat diagnostics and unsupported Mint stand-ins remain |
| Battle Reviver Seed #152 | 8000/8000 full turns/texts identical; 12 guards; 1400 consumptions per seed | Singles stack-one FaintPhase early return; Commander/doubles/acquisition/save bridge excluded |
| Reward Lures #153 | 8000/8000 modifier-state cases identical; 26 guards; add/refresh/full/lapse/chance | Canonical 10/15/30 durations; excludes scene/purchase/full BattleEnd, game/save and final chance clamp/RNG |
| [Game Orbs #154](../game-core/ORB-VALIDATION.md) | 8000/8000 full turns/texts identical, no skipped/uncovered; 235 host checks plus 14 battle guards | Stable inventory owner/status mapping and legal save boundary; shared Orb acquisition max-stack/replacement policy excluded |

Reward suites retained options 3950 identical / 50 unported, 8052 screens;
victory/application/Memory 4000 identical each. Battle B seed777 covered/identical
1942/1831/1890/1863 (plain/rich/switch/summon); ITEMS four-seed coverage
1922/1932/1929/1939 retains untranslated text sets 1/1/2/1; COND
1767/1756/1770/1766. Established checks/exclusions remained unchanged.
See [battle README](../battle-core/README.md), [held inventory](../battle-core/HELD-ITEM-INVENTORY.md)
and [reward README](../reward-core/README.md) for exact fixture scopes and mutation proof.

Game Mint 1000-run normal/save pairs: 49315 turns and `5f646aee` before,
49305 and `b465d096` after; all ended, zero hangs/crashes/timeouts. External
observation counted 14 actual Mint applications, one unsupported counted
application and one refusal, so no full Task 96 credit. Game Orbs subsequently
recorded 49293 turns and paired `b72f38a9`, all ended, zero hangs/crashes/timeouts;
32729 resume checks, existing 39 bridge / 641335 application checks pass.
The paired 200-run digest stays `a6662b64`. Changes in 1000-run behavior are
explained in the component records; chooser/digest code did not change. These
are host consistency checks, not upstream whole-run or console parity.

### Final eight-task checkpoint (2026-10-08)

The previous documentation refresh #155 and implementation #156–#158 are also
landed. #156 monetary-state API: 8000/8000 identical, 33 guards; #157 Multi Lens:
8000/8000 full turns/texts identical, 18 guards, Ball Fetch singles/form0/effective
stacks 1–2 only; #158 game vitamins: 8000 cases/16000 direct-public comparisons,
187 host checks. Its same-soak observation counted **135 actual effects / 49
unsupported stand-ins / two refusals**; 184 taken does not mean 184 real effects.
The paired 1000-run digest became `403b442f`, 49284 turns. Vitamin purchase error/
no deduction and Mint-after-vitamin guards remain. See [vitamin validation](../game-core/VITAMIN-VALIDATION.md).

The new batch comprises seven implementation PRs below and this final Card 149
documentation task. Each focused implementation suite uses the pinned Node 22
four seeds and 2000 cases per seed. These scopes and observations remain distinct.

| Slice | Recorded source / host evidence | Excluded boundary |
| --- | --- | --- |
| [Game money #159](../game-core/MONEY-VALIDATION.md) | 8000 cases / 16000 direct-public comparisons, 0 mismatches; 110 host checks; ordered Coin floors, cap, fractional and free/paid balances | Gimmighoul tracker/stat refresh, achievements, text/animation, guarded inputs; source monetary API is #156 |
| Reward temporary boosters #160 | 8000/8000 modifier-state/effect-projection cases identical; 25 guards; canonical five-battle X stats and Dire Hit add/refresh/full/lapse | Scene/purchase/tier fallback, full BattleEnd/game/save, final stat/accuracy/crit clamps |
| Battle Golden Punch #161 | 8000/8000 full turns, exact wallets and texts identical; 12 guards; post-hit money and ordered Coin bonus | Limited to Ball Fetch form0 singles, effective stacks1–5; excludes game wallet/inventory/save and achievements/presentation |
| [Game Reviver #162](../game-core/SEED-VALIDATION.md) | 8000/8000 full turns/texts identical; 126 host / 130 true-old-save checks plus 12 battle guards; consumption and detached provenance | Generic acquisition/stock/replacement/transfers/loot pickup; malformed duplicate stock stays opaque |
| Reward Soothe Bell #163 | 8000/8000 identical; 25 guards; held add/full/stack and first eligible singular raw floor boost | Full addFriendship/caps/candy/events/fusion/achievements; scene/purchases/game/save; raw wildcard records are not acquired |
| Battle Soul Dew #164 | 8000/8000 full turns/input stats/texts identical; 16 guards; existing nature-stat arithmetic proven and bounded class promoted | Existing helper, not newly invented effect; broad forms/abilities/doubles/acquisition/game/save remain unproved |
| [Game Wide Lens #165](../game-core/LENS-VALIDATION.md) | 8000/8000 full turns/texts identical; 195 host / 199 old-save / 259 with six real ModifierData rows; real+virtual <=3 | Transient creator lifecycle, full acquisition/stock/replacement/transfers; virtual stacks intentionally disappear on save/load |

Actual caller mutations remain meaningful: Golden Punch disconnect fails
1362/2000 turns although texts still match; Soul Dew stat-caller disconnect
fails 2208 input-stat checks although replayed turn/text state still matches
(the unchanged comparator restores oracle input stats). Neither is a passing
mutation. Wide Lens transport mutation produces 38 actual hit/miss text
differences in the separate observational replay; the committed boundary already
rejects its inventory mutation. Incomplete/malformed transports are refused.
See owning [battle](../battle-core/README.md) / [reward](../reward-core/README.md)
records for exact commands, supported scope, guards and mutation results.

Game money's same-soak observations: actual Nugget/Big/Relic applications
253/57/1 versus taken 294/66/2 (unsupported 41/9/1). Reviver observation:
nine real player consumptions, zero enemy, 213 player mapping visits and 25 taken.
Wide Lens observation: **60 actual player hit-check bonuses**, zero enemy,
229 mapping visits and 26 taken. Repeated visits, acquisitions and effects are
separate tallies; focused source fixtures establish enemy-side effects.

The coordinator independently reran on the merged `ee695e9` source:
`make -s -j2 -C game-core test`, `game-core/build/gcplay --auto 1000 --jobs 8`,
and the same command with `--save-test`; all underlying exits were zero.
Durable logs: `/workspace/validation/eight-batch/final-{host,normal,saved}.log`.
These combined host results agree with the final #165 checkpoint:
32697 resume, 39 existing bridge / 641335 existing
application checks; paired 200-run `79d84c65`. Both 1000-run modes end all runs,
zero cleared/stuck/crashed/timed out, 49206 turns and paired `c2937ca0`; 14291
save round trips, max11400 bytes. Earlier C checkpoints remain dated: money
`2473e509` / 49100 turns; Reviver `3492b259` / 49206. Chooser/digest code did
not change. Original healing/candy/ball/options outputs remain byte-identical
with exclusions and constructor-stat diagnostics uncredited. Mint/vitamin/money
retain 16000 modeled comparisons each, Orb/Seed 8000 full turns each on the
recorded integrated Lens source. These are host/source bounded results, not
full-run upstream, cleared Classic, complete Task 96/104, frontend or console
acceptance. Golden Punch has no game wallet bridge; Soul Dew remains a guarded
Mint/vitamin inventory. Task 64/65/66 and run-core/hardware gates remain.


### Ten-task checkpoint (2026-10-09)

Nine implementation PRs #167–#175 are merged; this Card 149 refresh is task ten.
Each new focused source suite uses Node 22.23.3, pin `8555c08`, seeds 777/9001/4242/
31337 and 2000 cases per seed. Counts describe their exact scopes, not overall
parity. Historical eight-task evidence above is retained, including statements
that were true at that checkpoint.

| Slice | Recorded evidence | Remaining boundary |
| --- | --- | --- |
| Reward Wide Lens #167 / Reviver #168 / Golden #171 | Each 8000 exact raw canonical acquisitions; 25/26/28 guards; actual source mutants fail; caps 3/1/5 | Full Scene.addModifier tier replacement/purchases/transfer and game callers are separate |
| Battle type proof #169 | 8000 exact full turns/input stats/texts; 20 guards; actual damage-caller mutant889/2000 failures | Existing20%/18types/cap99 helper; Hidden Power matches base move type at pin; acquisition/fusion/broader forms remain |
| [Battle critical + raw repair #173](../battle-core/CRITICAL-VALIDATION.md) | 8000 exact full turns/texts; 24 critical guards plus 3 fusion admission checks; actual critical caller mutant581/2000 failures | Scope Lens/Leek only, real 1/virtual 0; fusion Leek refused, raw primary-species behavior unclaimed as fusion parity |
| [Battle Eviolite/species-stat #175](../battle-core/STAT-ITEM-VALIDATION.md) | 8000 exact effective-stat snapshots/full turns/texts; 63 identity+4 fusion guards; Quick Powder caller mutant 198 SPD mismatches; summon orphan mutant 20 failures | Seven existing callers, no new math; generic StatBooster unpromoted; acquisitions/save/transfer and fused admission excluded |
| [Game Golden #170](../game-core/GOLDEN-VALIDATION.md) | 8000 exact full turns/wallets/texts; 330 host/454 with 10 real ModifierData answers; actual wallet-return omission fails at turn 0 | Ordered Coin dependency, double wallet, cap and separate Pay Day; source achievements/UI/full acquisition excluded |
| [Game Multi #172](../game-core/MULTI-VALIDATION.md) | 8000 exact full turns/texts; 201 host/253 with 4 real storage answers; actual mapping-source mutant fails | Effective 1–2; malformed stock generic opaque; acquisition/transfers and transient creator lifecycle excluded |
| [Game temporary caller #174](../game-core/TEMP-VALIDATION.md) | 8000 actual Scene/selection/BattleEnd callers plus 8000 unchanged raw cases; 90 host + 25 raw guards; lapse/save/projection mutants fail | Canonical X stats/Dire, five-battle timers, purchase/tier fallback; literal rendered messages/full-turn support/full BattleEnd excluded |

**Raw helper regression and repair:** #164 and #169 retained their measured
bounded full-turn results but introduced a regression outside those fixture
bounds. The unchanged legacy producer independently constructs real stacks up
to acquisition caps plus virtual stacks; upstream raw helpers still apply them.
Seed 777 / N=2000 pre-Soul had 2000 covered/pass and 0 stat differences. Post-Soul:
1990 covered/7 all-case stat differences/10 uncovered; post-type:1989/7/11;
unmerged strict critical:1940/7/60. This was a C-port regression, not a comparator
defect. #173 restores explicit raw `pr_project_*` arithmetic and flags while
retaining strict `pr_apply_*` leaf guards and full-turn inventory admission.
Four unchanged legacy seeds now match the pre-Soul baseline byte-for-byte:
2000/2000/2000/1999 covered/pass, 0 stat/carried-state differences/uncovered;
seed 31337 retains its existing upstream Fusion Flare throw exclusion. No test,
seed/count/tolerance/exclusion changed. #175 retains all 44 old before/after pairs.

Actual effects remain distinct from mapping/acquisition. Golden gain turns per
seed are 1362/1357/1358/1372; its stock soak observer saw seven credit calls,
total 33, versus eight Golden rewards taken. Multi count-helper gains are 300
per side per seed; damage-helper changes are 659/650,651/645,644/647,660/645.
Mapping visits (1000 per side) do not establish those effects. Real source
ModifierData answers prove transient stacks omitted/reset, not virtual-fixture
normal/save equality. Older Mint/Seed/Lens saves retain 113/130/199 checks.
Standalone virtual Coins remain opaque unless a valid represented-prefix Golden
enables the dependency; Golden itself has no Gimmighoul tracker side effect.

Temporary #174 persists named battle_count; older timerless stock stays opaque.
It lapses once after enemy cleanup at one culled BattleEnd, including flee,
not per trainer-member KO. At the pin a persistent temporary add mutates stock
but returns false: paid selection preserves money and errors; full-duration
replacement returns true and deducts. Source callers compare result/error
presence and state, not translated rendered messages. Raw temporary helpers
receive projected inputs, but full-turn unsupported flags remain unchanged.
This boundary is the next lane A prerequisite, not silently claimed complete.

Implementation H histories remain separate: type report-only digest shift
`c2937ca0`→5022ea67 removes 97 unsupported reports; critical's own earlier baseline
5022ea67→086babdb removes 14. Golden on50f279b shifts5022ea67→15d6d01d with 46
fewer reports. Multi before/after remains byte-identical15d6d01d. Digest hashes
unported counters, not wallet directly. Integrated pre-temporary/A3 H is
4d9f91a1; A3 leaves all rows unchanged. Temporary changes turns 49206→50394,
saves 14291→14679, max 11400→11472; its earlier C baseline15d6d01d→9e0f75df.
[Every changed temporary soak row](../game-core/TEMP-SOAK-DELTA.md) is retained:
counted post-battle refusals 3 runs/4 uses→167/1380 and reward stand-ins 139/169→
145/178. No zero-fallback or whole-run parity claim follows.

The coordinator independently reran on merged code `2470cb3`:
`make -s -j2 -C game-core test`, `game-core/build/gcplay --auto 1000 --jobs 8`,
and the same command with `--save-test`; all underlying exits are zero.
Logs: `/workspace/validation/ten-batch/final-{host,normal,saved}.log`.
Host checks include 33473 resume, 39 existing bridge / 641335 application checks;
new temporary/Multi/Golden host checks are 90/201/330. The 200-run pair is
`74adeffb`. Both 1000-run paths end all runs, zero cleared/stuck/crashed/timed
out, 50394 turns, paired digest `43f24f9f`; 14679 save/load round trips,
max11472 bytes. Compared with #174's `9e0f75df`, only digest and held unsupported
report differ: 983 runs/52586 occurrences → 982/52572,14 fewer from critical
promotion. Every other row is identical, including post-battle167/1380 and
reward stand-ins145/178. The report hash does not directly prove wallets or
timers. These are host consistency results, not whole-run upstream parity.

The separately authorized next five tasks begin only after this tenth task:
battle temporary full-turn proof, reward Multi Lens acquisition then Orb
acquisition, game Wide Lens caller acquisition, final docs refresh. Task 64
remains partial and 65/66 gated; reserved run-core, human hardware and broader
Task 96/104 boundaries remain. Source comparisons, host soaks and CI are distinct.


### Final five-task checkpoint (2026-10-09)

After #176 completed the ten-task batch, four further implementations #177–#180
merged. This final Card 149 docs refresh is fifth; stop after it. The proposed
Orb acquisition task was never started. Source audit first deferred temporary
promotion because an established game fixture explicitly required unsupported
status; independent Mystical Rock proceeded. The user then explicitly approved
a dedicated temporary proof with a narrow stronger fixture contract, replacing
the unstarted Orb slot. Historical scheduling/deferral records above and in
owning records remain dated history.

| Slice | Recorded source/guard evidence | Remaining boundary |
| --- | --- | --- |
| Reward Multi Lens #177 | 8000 exact raw canonical acquisitions; 29 guards; actual wildcard/duplicate/cap mutants fail 145/337/123 cases | Cap 2 first-exact-holder raw add; Scene tier fallback, purchase, transfer and full lifecycle excluded |
| [Mystical Rock #178](../battle-core/FIELD-EFFECT-VALIDATION.md) | 8000 exact full turns/texts; 19 guards; 1296 actual applications per seed; 4 actual caller/selection/cleanup/reset mutations fail | Existing weather/terrain callers, first success, real-only extension; cap 2 canonical admission; no acquisition/save proof |
| [Wide Lens caller #179](../game-core/WIDE-ACQUISITION-VALIDATION.md) | 8000 unchanged raw cases plus 8000 actual Scene/selection cases; 25 raw + 16 atomic contexts + 2 nonpositive Scene guards; 3 source mutations fail | Public full refusal differs from direct Scene tier replacement; paid persistent mutation/error/no-charge; literal rendered messages and broad opaque/fusion/stat contexts excluded |
| [Temporary full turns #180](../battle-core/TEMPORARY-BOOSTER-VALIDATION.md) | 8000 exact full turns/texts and fresh numerical snapshots; 105 guards + 12 mixed admission checks; 13 malformed traces rejected; 6 actual caller/clamp mutations fail | Canonical real 1–2/virtual 0/owner 0; once per record, not stack-scaled; broader blocked-critical abilities/doubles/fusion and lifecycle excluded |

All four focused source families use pin `8555c08`, Node 22.23.3 and seeds
777/9001/4242/31337, 2000 per seed. Mystical Rock adds 2×real stacks and ignores
virtual stacks, stopping at first matching success. Its summon proof adds scoped
orphan removal/virtual reset. Weather/arena/form baselines retain covered counts
1957/1960/1955/1963,1852/1842/1832/1846 and 2000 each, respectively. All 60 old
Field proof replay pairs are byte-identical; temporary retains 64 battle pairs.
Legacy direct damage/stat stays 2000/2000/2000/1999 with zero numerical failures
and its one existing upstream thrown case, preserving #173's raw-helper repair.

Temporary promotion is supported by actual stat, accuracy and critical callers,
not by suppressing unknown flags. A record contributes once even at stack 2;
duplicate records each contribute. Stat multiplier/accuracy-stage clamps and
ignored-stage early returns are tested numerically. Canonical admission is
positive; malformed/unknown/held/virtual contexts still refuse. The user-approved
`game-core/tests/temp_caller_test.c` update replaces the former unsupported
assertion with stronger identity/admission/helper outcomes and invalid refusals.
Existing source cases, comparisons, paid/full/BattleEnd results and exclusions
remain. Checks increase 6000 per seed: 107059→113059,105641→111641,
105718→111718,107224→113224; host 90→98, raw 25 unchanged. Of 48 retained game
outputs, 44 are byte-identical and four have only those approved stronger checks.
The authorization is narrow, not permission to relax other established checks.

Wide actual source accuracy changes occur in all 8000 focused cases, distinct
from acquisitions/mapping; Scene inventory changes 949/954/940/939 per seed.
Public first-full filter refusal and direct full tier-default ball substitution
are separately proved. Unsupported public contexts still take counted fallback.
Earlier game caller literal rendered-message exclusions remain; temporary's new
full-turn suite independently compares its messages exactly. Named timer saves,
transient resets, detached 0 versus wildcard -1 and old save 113/130/199 checks
remain bounded source contracts, not whole source session migration.

Multi and Field before/after 1000 H logs remain byte-identical 43f24f9f.
Wide changes only reward stand-in accounting 145 runs/178 uses→148/185 and digest
43f24f9f→30c8d270. Temporary changes only held unsupported reports 982 runs/52572
→924/46316 (6256 fewer) and digest 30c8d270→5b6c9ee2. Turns 50394, 14679 save/load
trips, max 11472 bytes and terminal/outcome/other counters remain unchanged.
These report hashes do not directly establish effect or wallet parity.

The coordinator independently reran merged code `b477e4e7d5f885fafd7d36423bb3ad04d13284a1`:
`make -s -j2 -C game-core test`, `game-core/build/gcplay --auto 1000 --jobs 8`,
and the same command with `--save-test`, all underlying exits 0. The 200-run pair
is `bc0fbf1c`. Both 1000-run paths end 1000, zero cleared/stuck/crashed/timed out,
50394 turns, paired digest `5b6c9ee2`; 14679 save/load round trips, max 11472 bytes.
Normal and saved logs are each byte-identical to the final temporary proof's
corresponding logs. Evidence: `/workspace/validation/five-batch/combined-results.json`
and `final-{host,normal,saved}.log` in that directory. This checks integration of
the four merges; it establishes host consistency, not whole-run upstream or
hardware parity.

The uploaded roadmap-after-parity-ledger-2026-10-04 maps these adapters and
prerequisites to **partial Card 96**, battle item proofs to **partial Card 104**,
and this status refresh to **Card 149 documentation**. It is planning context;
its old problem statements neither override user instructions nor imply
completion. No full 96/104, whole-run upstream, cleared Classic, frontend or
hardware result is claimed. Task 64 remains partial, 65/66 remain gated, run-core
owner scope and skipped Task 63 CI remain unchanged. Stop after this fifth task.


### Final three-task checkpoint (2026-10-09)

Following the prior five-task documentation checkpoint #181, two additional
implementations #182/#183 merged. This final Card 149 refresh is third; stop
without a fourth task. Each new source family uses pin `8555c08`, Node 22.23.3,
seeds 777/9001/4242/31337 and 2000 cases per seed. Historical ten/five-task evidence
above remains dated, including the Orb slot not started during that older batch.

| Slice | Recorded evidence | Remaining boundary |
| --- | --- | --- |
| Reward canonical Orbs #182 | 8000 exact raw acquisitions; 37 guards; 4 actual wildcard/class/cap/identity mutants fail 39/116/461/60 cases; 5 malformed traces + 2 requests refuse | Separate raw API; shared Toxic/Flame class cap 1; Scene/purchase/game acquisition/effects/transfer/save excluded |
| [Multi Lens game acquisition #183](../game-core/MULTI-ACQUISITION-VALIDATION.md) | 8000 unchanged raw + 8000 actual Scene/selection cases; 426 caller host + 29 raw guards; 5 actual caller mutations fail; 8 malformed traces + 4 request guards | Cap2 first-match, public-full refusal/direct-full tier-ball, paid mutation false/no charge; bounded Multi cleanup/recalc, literal rendered messages and wildcard source loading excluded |

Orb source matchType ignores status: an opposite Orb on the same exact holder
refuses FULL rather than creating a second family. Each seed has 1000 requests
per identity, 800 FULL/200 ADDED per identity, 800 opposite-Orb refusals total.
Wildcard stock is not an acquisition match. The new separate API leaves all
four existing canonical-held families' opaque-Orb behavior intact. Source and C
inventory/order/flags, party, wallet and RNG agree; no status effect is inferred
from raw acquisition. All 47 retained reward/host logs are byte-identical,
including ten older focused suites at 8000 each and the original four baselines.

Multi invokes actual factories/selectFilter/addModifier/updateModifiers/
calculateStats. Public first-full selection refuses before mutation; direct
Scene full replacement grants the tier-default ball. Persistent add mutates
but returns false: paid choices error without charging, free choices finish.
Only successful Scene additions prune Multi wildcard orphans and recalculate
within the guarded domain. Constructor args remain unchanged outside the shadow
context. Five actual mutations cover wrong target, missing wildcard cleanup,
incorrect paid result, disconnected projection and constructor arg clobber.

Multi actual count changes occur in 2000 cases per seed; numerical damage changes
are 12912/12897/12887/12906. These use seven raw helper positions, including
negative hitsLeft, and are **not seven executed strikes**. Full-turn hit/damage
proof remains the separate #157/#172 scope. The invariant compares all 54 modeled
party fields, inventory/holders/args, wallet/profile and RNG; transient turnData
changed by probes is outside PokemonData invariance. Canonical owned real
ModifierData reconstructions are compared; wildcard save cases establish game
consistency only. All 48 older bridge/temporary/Wide replays pass; 16 old apply/
candy/ball/options outputs remain byte-identical; older saves 113/130/199 load.
The user-approved stronger temporary checks from #180 remain unchanged.

Both implementations' before/after 1000 normal/saved logs stay byte-identical:
50394 turns, paired `5b6c9ee2`, 14679 save/load trips, max 11472 bytes, all 1000 ended,
zero cleared/stuck/crashed/timed out. Reward fallback 148 runs/185 uses and
post-battle 167/1380 remain; no zero-fallback claim follows.

The coordinator independently reran merged code `c3bfa8b33a4180b13d1385e8878f21fcff4cb94a`:
`make -s -j2 -C game-core test` exits 0, including 426 Multi acquisition host and
29 raw guards; the 200-run pair stays `bc0fbf1c`. Both
`game-core/build/gcplay --auto 1000 --jobs 8` and its `--save-test` variant exit 0,
end 1000, zero cleared/stuck/crashed/timed out, 50394 turns, paired `5b6c9ee2`;
14679 save/load round trips, max 11472 bytes. Each complete log is byte-identical
to the corresponding coordinator log from the preceding five-task checkpoint,
including every stand-in row. Evidence: `/workspace/validation/three-batch/combined-results.json`
and `final-{host,normal,saved}.log` there. This integration check establishes
host consistency, not whole-run upstream or hardware parity.

The roadmap mapping remains partial Card 96 adapters/prerequisites, partial
Card 104 item proofs and Card 149 status documentation. Orb game acquisition,
Lure/Soothe callers, wider transfers/lifecycle and reserved moveset prerequisites
remain. Task 64 is partial, 65/66 gated; no whole-run upstream, cleared Classic,
frontend/hardware or full 96/104 result is claimed. Conditional Task 63 jobs remain
skipped, not validation. Stop after this third task.

---

## 2. Inventory of Untested Move, Ability, and Item Classes

This inventory lists known unported or partially supported classes at pinned target `8555c08` without claiming an exhaustive audit:

### Move Classes
- **Transform & Sketch:** In-battle species/moveset copying.
- **Revival Blessing:** Party revival handling.
- **Stuff Cheeks:** Turn execution and berry consumption in `battle-core` (restriction in `ai-core` Task 58).
- **No Retreat:** Once-per-switch-in constraint.
- **Gigaton Hammer:** Consecutive turn usage restriction in turn history.
- **Spread moves in doubles:** Multi-target damage dispatch (first slice ported in Task 73).
- **Theft and Removal:** `Knock Off` / `Incinerate` item removal ported in Task 70; `Thief` / `Covet` transfers unported.

### Ability Classes

Entries describing landed slices are scope reminders, not blanket unported claims. See the linked PRs and owning README before scheduling another implementation.
- **Imposter:** Automatic Transform on entry.
- **Illusion:** Disguise state and apparent identity/type reads in `battle-core` (Task 67); natural ability dispatch unported.
- **Weather Form Triggers:** bounded `Forecast`, `Flower Gift`, `Ice Face` triggers landed (Task 69 / [#110](https://github.com/Gianciosvaldo/pokerogue-3ds/pull/110)); broader form coverage remains separate.
- **Passive Trace:** Passive ability copy on entry.
- **Neutralizing Gas:** Field-wide ability suppression.
- **Commander & Cramorant Gulp Missile:** Special form and battle triggers.
- **Dancer / Tera:** Dancer copied-move slice landed (Task 72 / [#119](https://github.com/Gianciosvaldo/pokerogue-3ds/pull/119)); Tera precommands remain unported.

### Item and Modifier Classes
- **Stat-pinch Berries:** `Liechi`, `Ganlon`, `Petaya`, `Apicot`, `Salac` already have trigger/dispatch paths (`battle-core/src/modifiers.c`); Task 71 explicitly calls for regression coverage, not duplicate implementation.
- **Special Berries:** bounded `Enigma`, `Lansat`, `Starf` effects landed (Task 71 / [#115](https://github.com/Gianciosvaldo/pokerogue-3ds/pull/115)); no exhaustive held-item parity claim.
- **Evolution, Form, and Fusion Items:** In-run item lifecycle and party mutation in `game-core` (Tasks 61, 62).
- **Mints:** bounded API #147 and game adapter #151 landed; original/custom nature, stats/HP and local unlocks are covered. Challenges/stat-inventory guards remain unsupported; Task 96 is partial.
- **Vitamins:** bounded API #150 landed; bounded game adapter #158 landed, including purchase-error/no-deduction behavior; unsupported inputs remain counted.
- **Lures:** canonical modifier primitives #153 landed; full caller/lifecycle remains pending. Canonical X/Dire Hit primitives #160 have a bounded caller/timer bridge #174, now complemented by bounded full-turn proof #180; raw Soothe Bell #163 does not establish full friendship integration.
- **Orbs / Wide Lens / Reviver Seed:** bounded dispatch #148/#149/#152 landed; game Orb/Reviver/Wide Lens bridges #154/#162/#165 landed; bounded Multi Lens/Golden Punch dispatch #157/#161 and Soul Dew proof #164 landed. Golden wallet #170 and Multi bridge #172 landed; raw Wide/Reviver/Golden acquisition #167/#168/#171 landed. Multi raw acquisition #177 and bounded Wide caller #179 also landed. Separate Orb raw acquisition #182 and bounded Multi caller #183 landed. Broader caller stock replacement/transfers remain separate. Use the [pinned class inventory](../battle-core/HELD-ITEM-INVENTORY.md) before scheduling further held work.
- **TMs / Memory Mushroom:** reward learn-move flow landed in [#128](https://github.com/Gianciosvaldo/pokerogue-3ds/pull/128), with follow-up Memory/save repair in [#131](https://github.com/Gianciosvaldo/pokerogue-3ds/pull/131).

---

## 3. Public Upstream Drift Inspection

**Inspected SHA:** `pagefaultgames/pokerogue` commit [`4f0969875845fb412c5b0af8ed4bc006484a8f2a`](https://github.com/pagefaultgames/pokerogue/commit/4f0969875845fb412c5b0af8ed4bc006484a8f2a) (42 commits ahead of target `8555c08`).

The public beta branch contains new upstream features and balance changes that differ from pinned `8555c08`. These changes are **outside the named port target** and are documented here for drift tracking:

1. **AI Move Selection & Weighting:** Priority move weighting and forcing two damage moves (`src/ai/ai-moveset-gen.ts`).
2. **Fixed Trainer Moveset Generation:** Updated trainer moveset selection functions (`src/data/balance/moves/moveset-generation.ts`).
3. **Passive Trace Behavior:** Modified passive ability interaction for Trace (`src/data/trainers/trainer-config.ts`).
4. **Trapping & Damage-Over-Time Tags:** Updated battler tags for trapping and DoT effects (`src/data/battler-tags.ts`).
5. **No Retreat Constraint:** Enforced once-per-switch-in usage limit (`src/data/moves/move.ts`).
6. **Revival Blessing Held Item Handling:** Adjusted held item interaction on revive (`src/field/pokemon.ts`).
7. **Uncommon Breed Balance Adjustments:** Modified stats/abilities for uncommon breeds.
8. **Summary Screen Move Deletion:** UI handler support for deleting moves in summary screen.
9. **Tutorial Speaker Boxes & Pokedex UI:** UI text box fixes and Pokedex display updates (`src/ui/handlers/`).
10. **Assets & Locales Pins:** Updated asset and localization pointers.

---

## 4. Landed PRs and Merged Task Index

- [PR #182](https://github.com/Gianciosvaldo/pokerogue-3ds/pull/182): separate canonical Toxic/Flame Orb raw acquisition; final head `15109f9b`, merge `838d5854`; [actual-head CI](https://github.com/Gianciosvaldo/pokerogue-3ds/actions/runs/37902972836) host `test` / `build-3ds` SUCCESS.
- [PR #183](https://github.com/Gianciosvaldo/pokerogue-3ds/pull/183): canonical Multi Lens game acquisition; final head `b5f95b3e`, merge `c3bfa8b3`; [actual-head CI](https://github.com/Gianciosvaldo/pokerogue-3ds/actions/runs/37903783568) host `test` / `build-3ds` SUCCESS.

Conditional Task63 jobs on #182/#183 were SKIPPED, not source passes.

- [PR #177](https://github.com/Gianciosvaldo/pokerogue-3ds/pull/177): Canonical Multi Lens raw acquisition; final head `c6e375aa`, merge `ba4f85cd`; [actual-head CI](https://github.com/Gianciosvaldo/pokerogue-3ds/actions/runs/37896617802) host `test` / `build-3ds` SUCCESS.
- [PR #178](https://github.com/Gianciosvaldo/pokerogue-3ds/pull/178): Mystical Rock weather/terrain caller proof; final head `eb32226b`, merge `f89a5195`; [actual-head CI](https://github.com/Gianciosvaldo/pokerogue-3ds/actions/runs/37898017752) host `test` / `build-3ds` SUCCESS.
- [PR #179](https://github.com/Gianciosvaldo/pokerogue-3ds/pull/179): Canonical Wide Lens game acquisition; final head `b31e59b0`, merge `c7f379ab`; [actual-head CI](https://github.com/Gianciosvaldo/pokerogue-3ds/actions/runs/37898178474) host `test` / `build-3ds` SUCCESS.
- [PR #180](https://github.com/Gianciosvaldo/pokerogue-3ds/pull/180): Temporary X/Dire full-turn proof and approved stronger fixture contract; final head `519e535a`, merge `b477e4e7`; [actual-head CI](https://github.com/Gianciosvaldo/pokerogue-3ds/actions/runs/37901092693) host `test` / `build-3ds` SUCCESS.

Conditional Task63 jobs on #177–#180 were SKIPPED, not source passes.

- [PR #176](https://github.com/Gianciosvaldo/pokerogue-3ds/pull/176): ten-task Card149 documentation checkpoint; final head `34cbcdab`, merge `766bd059`; [actual-head CI](https://github.com/Gianciosvaldo/pokerogue-3ds/actions/runs/37895561946) host `test` / `build-3ds` SUCCESS.

- [PR #167](https://github.com/Gianciosvaldo/pokerogue-3ds/pull/167): Wide Lens acquisition; final head `d0229167`, merge `2a63abd3`; [actual-head CI](https://github.com/Gianciosvaldo/pokerogue-3ds/actions/runs/37820972966) host `test` / `build-3ds` SUCCESS.
- [PR #168](https://github.com/Gianciosvaldo/pokerogue-3ds/pull/168): Reviver Seed acquisition; final head `db9041e6`, merge `492a0ead`; [actual-head CI](https://github.com/Gianciosvaldo/pokerogue-3ds/actions/runs/37821709822) host `test` / `build-3ds` SUCCESS.
- [PR #169](https://github.com/Gianciosvaldo/pokerogue-3ds/pull/169): Existing type-boost full-turn proof; final head `0b90cc7b`, merge `50f279b4`; [actual-head CI](https://github.com/Gianciosvaldo/pokerogue-3ds/actions/runs/37821908798) host `test` / `build-3ds` SUCCESS.
- [PR #170](https://github.com/Gianciosvaldo/pokerogue-3ds/pull/170): Golden Punch inventory/wallet bridge; final head `da744251`, merge `bde7ceea`; [actual-head CI](https://github.com/Gianciosvaldo/pokerogue-3ds/actions/runs/37822550964) host `test` / `build-3ds` SUCCESS.
- [PR #171](https://github.com/Gianciosvaldo/pokerogue-3ds/pull/171): Golden Punch acquisition; final head `05bcb097`, merge `8534067b`; [actual-head CI](https://github.com/Gianciosvaldo/pokerogue-3ds/actions/runs/37822445608) host `test` / `build-3ds` SUCCESS.
- [PR #172](https://github.com/Gianciosvaldo/pokerogue-3ds/pull/172): Multi Lens inventory bridge; final head `345a04c4`, merge `6b655754`; [actual-head CI](https://github.com/Gianciosvaldo/pokerogue-3ds/actions/runs/37830365249) host `test` / `build-3ds` SUCCESS.
- [PR #173](https://github.com/Gianciosvaldo/pokerogue-3ds/pull/173): Critical caller proof and raw-helper repair; final head `50b776e5`, merge `e99b5f9b`; [actual-head CI](https://github.com/Gianciosvaldo/pokerogue-3ds/actions/runs/37830945898) host `test` / `build-3ds` SUCCESS.
- [PR #174](https://github.com/Gianciosvaldo/pokerogue-3ds/pull/174): Canonical temporary caller/timer bridge; final head `79b5d31b`, merge `ae1e7240`; [actual-head CI](https://github.com/Gianciosvaldo/pokerogue-3ds/actions/runs/37894966596) host `test` / `build-3ds` SUCCESS.
- [PR #175](https://github.com/Gianciosvaldo/pokerogue-3ds/pull/175): Eviolite and six species-stat caller proofs; final head `a08eddf1`, merge `2470cb39`; [actual-head CI](https://github.com/Gianciosvaldo/pokerogue-3ds/actions/runs/37895060929) host `test` / `build-3ds` SUCCESS.

Conditional Task63 jobs on #167–#175 were SKIPPED; no source/whole-run or hardware credit.

- [PR #155](https://github.com/Gianciosvaldo/pokerogue-3ds/pull/155): docs: refresh bounded item parity and next-task prerequisites; final head `da8c0a8b`, merge `25cb8825`; [actual-head CI](https://github.com/Gianciosvaldo/pokerogue-3ds/actions/runs/37744930015) host `test` / `build-3ds` SUCCESS.
- [PR #156](https://github.com/Gianciosvaldo/pokerogue-3ds/pull/156): Add bounded money-item application with ordered Amulet bonuses; final head `4cdbf320`, merge `582f2018`; [actual-head CI](https://github.com/Gianciosvaldo/pokerogue-3ds/actions/runs/37745709255) host `test` / `build-3ds` SUCCESS.
- [PR #157](https://github.com/Gianciosvaldo/pokerogue-3ds/pull/157): Port bounded Multi Lens turn hit-count dispatch; final head `91e7a0c0`, merge `e3120dcd`; [actual-head CI](https://github.com/Gianciosvaldo/pokerogue-3ds/actions/runs/37746037624) host `test` / `build-3ds` SUCCESS.
- [PR #158](https://github.com/Gianciosvaldo/pokerogue-3ds/pull/158): Apply bounded vitamins through reward-core in game-core; final head `c4796d3b`, merge `1c984b7b`; [actual-head CI](https://github.com/Gianciosvaldo/pokerogue-3ds/actions/runs/37746830519) host `test` / `build-3ds` SUCCESS.
- [PR #159](https://github.com/Gianciosvaldo/pokerogue-3ds/pull/159): Apply bounded Nugget-family rewards to game run balances; final head `9358daa0`, merge `6de69105`; [actual-head CI](https://github.com/Gianciosvaldo/pokerogue-3ds/actions/runs/37757377857) host `test` / `build-3ds` SUCCESS.
- [PR #160](https://github.com/Gianciosvaldo/pokerogue-3ds/pull/160): Add canonical temporary booster acquisition and expiry APIs; final head `549ec245`, merge `63f629dc`; [actual-head CI](https://github.com/Gianciosvaldo/pokerogue-3ds/actions/runs/37758006596) host `test` / `build-3ds` SUCCESS.
- [PR #161](https://github.com/Gianciosvaldo/pokerogue-3ds/pull/161): Port bounded Golden Punch damage money dispatch; final head `9afda852`, merge `188f2d70`; [actual-head CI](https://github.com/Gianciosvaldo/pokerogue-3ds/actions/runs/37758922632) host `test` / `build-3ds` SUCCESS.
- [PR #162](https://github.com/Gianciosvaldo/pokerogue-3ds/pull/162): Bridge Reviver Seed inventory consumption and detached ownership; final head `9a67d44a`, merge `d0291d66`; [actual-head CI](https://github.com/Gianciosvaldo/pokerogue-3ds/actions/runs/37759188705) host `test` / `build-3ds` SUCCESS.
- [PR #163](https://github.com/Gianciosvaldo/pokerogue-3ds/pull/163): Add Soothe Bell acquisition and singular raw friendship boost; final head `c14ab617`, merge `6964ab4e`; [actual-head CI](https://github.com/Gianciosvaldo/pokerogue-3ds/actions/runs/37759885667) host `test` / `build-3ds` SUCCESS.
- [PR #164](https://github.com/Gianciosvaldo/pokerogue-3ds/pull/164): Prove bounded Soul Dew nature stats in full turns; final head `0dff532f`, merge `36f7636f`; [actual-head CI](https://github.com/Gianciosvaldo/pokerogue-3ds/actions/runs/37760819389) host `test` / `build-3ds` SUCCESS.
- [PR #165](https://github.com/Gianciosvaldo/pokerogue-3ds/pull/165): Bridge Wide Lens accuracy inventory and transient stack transport; final head `80e5f374`, merge `ee695e96`; [actual-head CI](https://github.com/Gianciosvaldo/pokerogue-3ds/actions/runs/37761588454) host `test` / `build-3ds` SUCCESS.

The separate conditional Task 63 jobs on #155–#165 were SKIPPED, not source
regression passes. Final-head build success does not replace the local scoped
source evidence or validate a complete R/whole-run/hardware contract.

- [PR #147](https://github.com/Gianciosvaldo/pokerogue-3ds/pull/147): Mint API; final head `c062346`, merge `ffc9eb0`; [actual-head CI](https://github.com/Gianciosvaldo/pokerogue-3ds/actions/runs/37675661126) host `test` and `build-3ds` succeeded.
- [PR #148](https://github.com/Gianciosvaldo/pokerogue-3ds/pull/148): Orb battle dispatch; final head `8a05f17`, merge `9cd7a03`; [actual-head CI](https://github.com/Gianciosvaldo/pokerogue-3ds/actions/runs/37675717768) host `test` and `build-3ds` succeeded.
- [PR #149](https://github.com/Gianciosvaldo/pokerogue-3ds/pull/149): Wide Lens dispatch; final head `d4aeedb`, merge `833aab0`; [actual-head CI](https://github.com/Gianciosvaldo/pokerogue-3ds/actions/runs/37686602896) host `test` and `build-3ds` succeeded.
- [PR #150](https://github.com/Gianciosvaldo/pokerogue-3ds/pull/150): IV-capped vitamin API; final head `322f5b3`, merge `ab94a38`; [actual-head CI](https://github.com/Gianciosvaldo/pokerogue-3ds/actions/runs/37687401559) host `test` and `build-3ds` succeeded.
- [PR #151](https://github.com/Gianciosvaldo/pokerogue-3ds/pull/151): bounded Mint game adapter; final head `2b71608`, merge `df13f09`; [actual-head CI](https://github.com/Gianciosvaldo/pokerogue-3ds/actions/runs/37687913896) host `test` and `build-3ds` succeeded.
- [PR #152](https://github.com/Gianciosvaldo/pokerogue-3ds/pull/152): bounded singles Reviver Seed; final head `c3574c1`, merge `7d438c2`; [actual-head CI](https://github.com/Gianciosvaldo/pokerogue-3ds/actions/runs/37688630414) host `test` and `build-3ds` succeeded.
- [PR #153](https://github.com/Gianciosvaldo/pokerogue-3ds/pull/153): canonical Lure primitives; final head `92b98af`, merge `88f448c`; [actual-head CI](https://github.com/Gianciosvaldo/pokerogue-3ds/actions/runs/37689153101) host `test` and `build-3ds` succeeded.
- [PR #154](https://github.com/Gianciosvaldo/pokerogue-3ds/pull/154): Orb inventory game bridge; final head `a811a24`, merge `0a10696`; [actual-head CI](https://github.com/Gianciosvaldo/pokerogue-3ds/actions/runs/37689340015) host `test` and `build-3ds` succeeded.

For #147–#154, the separate conditional Task 63 `differential` and `host` jobs
were **skipped**. They supply no Task 63/full R regression credit. PR numbers
are not card numbers: PR #149 is Wide Lens, while Card 149 is documentation.

- [PR #145](https://github.com/Gianciosvaldo/pokerogue-3ds/pull/145): Task 64 first slice, supported clean wild movesets; trainer RNG boundaries and broader pools remain deferred. Host CI and 3DS build passed on `3c762a3`; merge `92e56dc`.
- [PR #143](https://github.com/Gianciosvaldo/pokerogue-3ds/pull/143) and [#142](https://github.com/Gianciosvaldo/pokerogue-3ds/pull/142): Task 96 Full Heal integration and reward-core status/confusion/Nightmare handling. See the known bought-item message timing difference in the game-core README.
- [PR #141](https://github.com/Gianciosvaldo/pokerogue-3ds/pull/141): Task 96 Poké Ball rewards through reward-core.
- [PR #140](https://github.com/Gianciosvaldo/pokerogue-3ds/pull/140): earlier README/ledger refresh; historical entries below are not all current task gates.
- [PR #138](https://github.com/Gianciosvaldo/pokerogue-3ds/pull/138): Card 116 app evolution prompt; host tests do not establish graphical/console acceptance.
- [PR #121](https://github.com/Gianciosvaldo/pokerogue-3ds/pull/121): Task 92 victory continuation and enemy switch handling.
- [PR #120](https://github.com/Gianciosvaldo/pokerogue-3ds/pull/120): Card 110 app run driver.
- [PR #117](https://github.com/Gianciosvaldo/pokerogue-3ds/pull/117): Task 68 loaded Illusion AI helpers; natural activation remains disabled.
- [PR #116](https://github.com/Gianciosvaldo/pokerogue-3ds/pull/116): Task 61 first Fire Stone/Griseous Core lifecycle slice, not all evolution/form families.
- [PR #114](https://github.com/Gianciosvaldo/pokerogue-3ds/pull/114) and [#113](https://github.com/Gianciosvaldo/pokerogue-3ds/pull/113): Tasks 60/59 healing/PP application and real reward options.
- [PR #107](https://github.com/Gianciosvaldo/pokerogue-3ds/pull/107) and [#104](https://github.com/Gianciosvaldo/pokerogue-3ds/pull/104): merged learnset-input and evolution/application repairs; former draft blockers are historical.
- [PR #139](https://github.com/Gianciosvaldo/pokerogue-3ds/pull/139) (commit [`639a086`](https://github.com/Gianciosvaldo/pokerogue-3ds/commit/639a086)): Task 78 / Card 78 (Starter unlocks, genderless species, and cost enforcement in `game-core`).
- [PR #136](https://github.com/Gianciosvaldo/pokerogue-3ds/pull/136) (commit [`6fd4cff`](https://github.com/Gianciosvaldo/pokerogue-3ds/commit/6fd4cff)): Task 96, Rare Candy and Rarer Candy slice only (application through `reward-core` in `game-core`).
- [PR #135](https://github.com/Gianciosvaldo/pokerogue-3ds/pull/135) (commit [`102905a`](https://github.com/Gianciosvaldo/pokerogue-3ds/commit/102905a)): Card 114 (Catching animation, full-party release flow in `game/`).
- [PR #134](https://github.com/Gianciosvaldo/pokerogue-3ds/pull/134) (commit [`07b1cfe`](https://github.com/Gianciosvaldo/pokerogue-3ds/commit/07b1cfe)): Card 113 (Title, save slots, continue, game over, clear screens in `game/`).
- [PR #132](https://github.com/Gianciosvaldo/pokerogue-3ds/pull/132) (commit [`705a79e`](https://github.com/Gianciosvaldo/pokerogue-3ds/commit/705a79e)): Card 97 (Trainers' own rewards in `reward-core`).
- [PR #129](https://github.com/Gianciosvaldo/pokerogue-3ds/pull/129) (commit [`c70691b`](https://github.com/Gianciosvaldo/pokerogue-3ds/commit/c70691b)): Card 112 (Reward, shop, biome screens driven by the run in `game/`).
- [PR #128](https://github.com/Gianciosvaldo/pokerogue-3ds/pull/128) (commit [`0ac92b9`](https://github.com/Gianciosvaldo/pokerogue-3ds/commit/0ac92b9)): Task 95 (TM and Memory Mushroom reward application in `game-core`).
- [PR #112](https://github.com/Gianciosvaldo/pokerogue-3ds/pull/112): Task 63 bounded enemy moveset API; supported clean wild constructors are now wired by #145, broader pools and trainer integration remain.
- [PR #127](https://github.com/Gianciosvaldo/pokerogue-3ds/pull/127): Task 74 (Doubles AI command selection in `ai-core`).
- [PR #125](https://github.com/Gianciosvaldo/pokerogue-3ds/pull/125): Task 77 (Fleeing through the run loop in `game-core`).
- [PR #124](https://github.com/Gianciosvaldo/pokerogue-3ds/pull/124): Task 94 (Catching in the run audit against upstream `VictoryPhase` & `AttemptCapturePhase` in `game-core`).
- [PR #123](https://github.com/Gianciosvaldo/pokerogue-3ds/pull/123): Task 73 (Doubles turn engine first slice in `battle-core`).
- [PR #122](https://github.com/Gianciosvaldo/pokerogue-3ds/pull/122): Task 93 (Boss segments in battles in `game-core`).
- [PR #119](https://github.com/Gianciosvaldo/pokerogue-3ds/pull/119): Task 72 (Bounded Dancer dispatch in `battle-core`).
- [PR #115](https://github.com/Gianciosvaldo/pokerogue-3ds/pull/115): Task 71 (Enigma, Lansat, Starf berries in `battle-core`).
- [PR #111](https://github.com/Gianciosvaldo/pokerogue-3ds/pull/111): Task 70 (Held-item theft/removal in `battle-core`).
- [PR #110](https://github.com/Gianciosvaldo/pokerogue-3ds/pull/110): Task 69 (Weather-driven form changes in `battle-core`).
- [PR #108](https://github.com/Gianciosvaldo/pokerogue-3ds/pull/108): Task 67 (Dormant Illusion identity/type helpers in `battle-core`).
- [PR #106](https://github.com/Gianciosvaldo/pokerogue-3ds/pull/106): Task 90 (Battle baseline regression repairs in `battle-core`).
- [PR #102](https://github.com/Gianciosvaldo/pokerogue-3ds/pull/102): Task 88 (AI matchup differential diagnosis in `ai-core`).
- [PR #101](https://github.com/Gianciosvaldo/pokerogue-3ds/pull/101): Task 87 (Expanded save/no-save divergence repair in `game-core`).
- [PR #97](https://github.com/Gianciosvaldo/pokerogue-3ds/pull/97) (`outside/held-item-bridge`, commit [`58030d6`](https://github.com/Gianciosvaldo/pokerogue-3ds/commit/58030d6)): Task 57 (Held-item mapping and consumed-stack persistence in `game-core`).
- [PR #92](https://github.com/Gianciosvaldo/pokerogue-3ds/pull/92) (commit [`506d20e`](https://github.com/Gianciosvaldo/pokerogue-3ds/commit/506d20e)): Task 50 (Eternatus & Paradox Fixed Generation in `run-core`).
- [PR #86](https://github.com/Gianciosvaldo/pokerogue-3ds/pull/86) (final head [`e7f54b3`](https://github.com/Gianciosvaldo/pokerogue-3ds/commit/e7f54b3)): CI check & diagnostic cleanup audit.
- [Actions run #75](https://github.com/Gianciosvaldo/pokerogue-3ds/actions/runs/37114586055), head `3966f42930a8e0a399b808a47a786f328419e7f3`: historical Task 48 rival validation (12,000 fights, zero mismatches). This is a workflow run, not PR #75; no final-head/current-main rerun is claimed.
- Task 58 (commit [`836d10c`](https://github.com/Gianciosvaldo/pokerogue-3ds/commit/836d10c)): Stuff Cheeks berry restriction in `ai-core`.
- Task 55 (commit [`f5a7dff`](https://github.com/Gianciosvaldo/pokerogue-3ds/commit/f5a7dff)): Luck, Passives & Shiny Tier UI Displays.
- Task 51 (commit [`d603c37`](https://github.com/Gianciosvaldo/pokerogue-3ds/commit/d603c37)): Shop UI rendering and Lock Capsule visuals.
- Task 49 (commit [`cfd3bf3`](https://github.com/Gianciosvaldo/pokerogue-3ds/commit/cfd3bf3)): Differential runner closures opt-in.
- Task 46 (commit [`47b61df`](https://github.com/Gianciosvaldo/pokerogue-3ds/commit/47b61df)): Mystery Encounter choices and action logic tree.
- Task 41 (commit [`7a8806a`](https://github.com/Gianciosvaldo/pokerogue-3ds/commit/7a8806a)): Mid-battle form changes in `battle-core`.

---

## 5. Unavailable Checks and Recorded Limitations

1. **Task 85 (3DS Hardware / Emulator Acceptance):** Human/console gated; requires physical Old 3DS or Azahar emulator execution to evaluate UI rendering, frame rates, audio, and SD card persistence.
2. **Task 87 / merged [#101](https://github.com/Gianciosvaldo/pokerogue-3ds/pull/101):** Mystery history/current node, repeated summon effects, battle RNG and retained switch-out flag repaired. 36,471 resume checks pass. Independent strict checks observe terminal screens for 1000 normal and 1000 saved runs, maximum 450/472 steps of 200000. Original legacy check/chooser remains unchanged. Merged-head 1000-run digest 0cba4e49/0cba4e49; later Task 90/helper head fea61f28/fea61f28, 57,502 turns each, zero stuck/crashed/timed out. This establishes host consistency, not upstream full-run parity.
3. **Task 88 / merged [#102](https://github.com/Gianciosvaldo/pokerogue-3ds/pull/102):** Node 22.23.3 comparisons: 19762/19762 covered/identical out of 20000 generated, 238 uncovered, zero mismatches. Original Node 24.19.0 2/9/9/12 failures remain: Math.pow's last bits differ, propagating through Stealth Rock matchup scores. This is a runtime-contract conflict, not a tolerance waiver or complete AI parity.
4. **Task 89 / merged [#104](https://github.com/Gianciosvaldo/pokerogue-3ds/pull/104):** bounded evolution requests and application repairs landed. The former 135 application failures and unrecorded status draw were repaired with authorization; recorded application 12000/12000, bounded evolution state 8000/8000, and Memory 4000/4000. Runtime versions and exact scopes remain in [reward-core README](../reward-core/README.md); those historical Node 24 repair results are not new Node 22 evidence. Complete evolution/form/fusion execution, persisted ownership and broader inventory lifecycle remain separate.
5. **Task 67 / merged [#108](https://github.com/Gianciosvaldo/pokerogue-3ds/pull/108):** dormant identity/type helpers are on main (`cae69338`); recorded 8000/8000 manual/helper comparisons. Loaded AI integration subsequently landed in [#117](https://github.com/Gianciosvaldo/pokerogue-3ds/pull/117). Natural Illusion dispatch remains disabled and unvalidated; no natural-activation pass is implied.
6. **Task 90 / merged [#106](https://github.com/Gianciosvaldo/pokerogue-3ds/pull/106):** Before baseline plain/rich/switch/summon failures 1/7/2/30, after 0/0/0/0, covered 1940/1826/1885/1856. Removes form-based Max immunities absent from the pin, evaluates ability form callbacks and queues enabled Daily NONE passives. Existing producers/comparators/exclusions unchanged; no coverage loss. CI #102 host/3DS pass.
7. **Task 91 / merged [#107](https://github.com/Gianciosvaldo/pokerogue-3ds/pull/107):** actual TM exclusions and relearner inputs are distinct. Recorded dual-input 8000/8000 plus 24,000 TM/RNG comparisons; legacy Memory import repaired and 4000 cases replayed at zero mismatches. The old ValueHolder/no-cases blocker is historical. Reward options and consumable integration landed in #113/#114; they are not still waiting on this PR.
8. **Task 63 / merged [#112](https://github.com/Gianciosvaldo/pokerogue-3ds/pull/112):** bounded species/form/pool generator only. [Validation](../run-core/TASK63-VALIDATION.md) and the [incomplete R checkpoint](../run-core/TASK63-R-REGRESSION.md) distinguish completed commands from pending work; a merge is not evidence that every planned R command ran.
9. **Task 64 / merged [#145](https://github.com/Gianciosvaldo/pokerogue-3ds/pull/145):** first clean wild adapter slice only. All 1000 soaks still use unsupported movesets somewhere. Trainer member RNG boundaries and broader Task 63 pools remain real gates to full Task 64 and Tasks 65/66. The app's 1000-run rare-evolution coverage failure and expanded 5000-run passing check are both retained in [the validation record](../game-core/TASK64-VALIDATION.md).

Canonical Wide Lens caller acquisition #179 is merged; bounded Multi caller acquisition #183 is also merged; the three-task batch ends with this documentation refresh. Bounded Lure/friendship caller integration and remaining held boundaries remain future work, alongside the remaining Task 96 item families and the missing bounded prerequisites for full Task 64. Task 78 is already merged; Classic win progression 79 is still pending. Further Task 61 slices, fusion 62, doubles lifecycle 75, weather 76 and later modes/eggs/mystery/system work are not complete. Do not duplicate merged slices or infer whole-run upstream parity from host consistency. Run-core ownership, Node 22 reference policy and human hardware gates remain in force. Codex merge/continuation authorization is recorded in HANDOFF's October 7 update; it does not waive validation or change owner scope.
