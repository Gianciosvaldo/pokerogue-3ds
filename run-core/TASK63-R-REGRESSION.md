# Task 63: R regression checkpoint

Source baseline: main `a3f44d3403c1da26bf4438dc2f598a0b389be017`. Candidate: PR #112, `outside/enemy-movesets` (implementation plus producer repairs). Upstream pin: `8555c08c823b856cbec4eb99ca84ea52a955836d`; Node 22.23.3, GCC 13.3.0, Vitest 4.1.11.

This is an incomplete checkpoint. Commands listed below have completed; the remaining commands are still pending, so no complete R pass is claimed. The target is 78 R commands plus four challenge checks (82 total). Completed final exits are 0; counts and explicit existing not-covered categories are preserved below. Daily actually produces five fixed runs / 250 waves / 775 checks regardless of the command N/seed.

## Before/after scope

The before column is the unchanged main C library replayed against each identical, archived final trace. It is a paired C comparison, **not** a second fresh run of all unchanged-main producers. The fresh unchanged-main producer attempts and their failures are recorded in TASK63-VALIDATION.md. Daily, gym/E4, rival and trainer-party producers needed authorized repairs; their corrected traces must not be represented as output of the old producers. All other producer files, existing C comparators, tolerances, seeds and requested counts remain unchanged.

After the initial matrix began, the candidate incorporated unchanged Task 73 main files from `68a60c94527064440b33f3beb321dc6748ef5123`. Every archived upstream trace was then replayed against a clean rebuilt integrated candidate; those final replays are shown below. There is no game-core moveset wiring in this PR.

Working directory for final scripts: `/workspace/scratch/809689da2702/task63-resume`. Each upstream checkout is an isolated harness copy outside the repo at the pinned commit. PATH begins `/root/.npm/_npx/d18f28baf1132559/node_modules/node/bin`. BUILD and DIFF_FOCUS are shown explicitly. Traces are archived per seed/suite before focused runs overwrite producer output names.

## Build and host checks

```sh
make -C ../task63-baseline/run-core
make -C run-core
make -C ../task63-baseline/game-core test
make -C game-core test
make -C run-core test-daily-rng
git diff --check
```

All exit 0. Game-core before/after have saved/unsaved digests `8decbf9d` / `8decbf9d`; candidate daily RNG host regression: 27 checks, 27 passed, 0 failed. See TASK63-VALIDATION.md for additional host counts.

## Every command and printed C summary

### 777-wave

```sh
BUILD=/workspace/scratch/809689da2702/validation/r63-final/777 sh run-core/tools/difftest.sh /workspace/scratch/809689da2702/upstream-r63-a 20000 777 wave
```
Final exit 0. Printed summary:

```text
cases 20000
  game-mode rules (15 per case)        pass 300000  fail 0  not covered 0
  getMaxExpLevel                       pass  20000  fail 0  not covered 0
  getLevelForWave (+ RNG state)        pass  20000  fail 0  not covered 0
  isWaveTrainer (+ RNG state)          pass  20000  fail 0  not covered 0
  resetSeed + Battle seeding chain     pass  20000  fail 0  not covered 0
  getEncounterBossSegments             pass  20000  fail 0  not covered 0
  getDoubleBattleChance                pass  20000  fail 0  not covered 0
```

Paired unchanged-main C replay:

```sh
/workspace/scratch/809689da2702/task63-baseline/run-core/build/wavetest /workspace/scratch/809689da2702/validation/r63-final/777/wave-cases.txt
```
Before replay exit 0. Printed summary:

```text
cases 20000
  game-mode rules (15 per case)        pass 300000  fail 0  not covered 0
  getMaxExpLevel                       pass  20000  fail 0  not covered 0
  getLevelForWave (+ RNG state)        pass  20000  fail 0  not covered 0
  isWaveTrainer (+ RNG state)          pass  20000  fail 0  not covered 0
  resetSeed + Battle seeding chain     pass  20000  fail 0  not covered 0
  getEncounterBossSegments             pass  20000  fail 0  not covered 0
  getDoubleBattleChance                pass  20000  fail 0  not covered 0
```

Integrated final candidate replay:

```sh
/workspace/scratch/809689da2702/task63-resume/run-core/build/wavetest /workspace/scratch/809689da2702/validation/r63-final/777-wave-cases.txt
```
Integrated final exit 0. Printed summary:

```text
cases 20000
  game-mode rules (15 per case)        pass 300000  fail 0  not covered 0
  getMaxExpLevel                       pass  20000  fail 0  not covered 0
  getLevelForWave (+ RNG state)        pass  20000  fail 0  not covered 0
  isWaveTrainer (+ RNG state)          pass  20000  fail 0  not covered 0
  resetSeed + Battle seeding chain     pass  20000  fail 0  not covered 0
  getEncounterBossSegments             pass  20000  fail 0  not covered 0
  getDoubleBattleChance                pass  20000  fail 0  not covered 0
```

### 777-shop

```sh
BUILD=/workspace/scratch/809689da2702/validation/r63-final/777 sh run-core/tools/difftest.sh /workspace/scratch/809689da2702/upstream-r63-a 20000 777 shop
```
Final exit 0. Printed summary:

```text
cases 20000
  getWaveMoneyAmount                       pass  20000  fail 0  not covered 0
  money reward (Amulet Coin, Happy Hour)   pass  20000  fail 0  not covered 0
  getShopStatus                            pass  20000  fail 0  not covered 0
  shop options as shown (Black Sludge)     pass  20000  fail 0  not covered 0
  shop options as charged                  pass  20000  fail 0  not covered 0
  getRerollCost                            pass  20000  fail 0  not covered 0
```

Paired unchanged-main C replay:

```sh
/workspace/scratch/809689da2702/task63-baseline/run-core/build/shoptest /workspace/scratch/809689da2702/validation/r63-final/777/shop-cases.txt
```
Before replay exit 0. Printed summary:

```text
cases 20000
  getWaveMoneyAmount                       pass  20000  fail 0  not covered 0
  money reward (Amulet Coin, Happy Hour)   pass  20000  fail 0  not covered 0
  getShopStatus                            pass  20000  fail 0  not covered 0
  shop options as shown (Black Sludge)     pass  20000  fail 0  not covered 0
  shop options as charged                  pass  20000  fail 0  not covered 0
  getRerollCost                            pass  20000  fail 0  not covered 0
```

Integrated final candidate replay:

```sh
/workspace/scratch/809689da2702/task63-resume/run-core/build/shoptest /workspace/scratch/809689da2702/validation/r63-final/777-shop-cases.txt
```
Integrated final exit 0. Printed summary:

```text
cases 20000
  getWaveMoneyAmount                       pass  20000  fail 0  not covered 0
  money reward (Amulet Coin, Happy Hour)   pass  20000  fail 0  not covered 0
  getShopStatus                            pass  20000  fail 0  not covered 0
  shop options as shown (Black Sludge)     pass  20000  fail 0  not covered 0
  shop options as charged                  pass  20000  fail 0  not covered 0
  getRerollCost                            pass  20000  fail 0  not covered 0
```

### 777-biome

```sh
BUILD=/workspace/scratch/809689da2702/validation/r63-final/777 sh run-core/tools/difftest.sh /workspace/scratch/809689da2702/upstream-r63-a 20000 777 biome
```
Final exit 0. Printed summary:

```text
cases 20000
  initBiomeDepths                      pass  20000  fail 0  not covered 0
  generateRandomBiome (+ RNG state)    pass  20000  fail 0  not covered 0
  SelectBiomePhase (+ RNG state)       pass  20000  fail 0  not covered 0
```

Paired unchanged-main C replay:

```sh
/workspace/scratch/809689da2702/task63-baseline/run-core/build/biometest /workspace/scratch/809689da2702/validation/r63-final/777/biome-cases.txt
```
Before replay exit 0. Printed summary:

```text
cases 20000
  initBiomeDepths                      pass  20000  fail 0  not covered 0
  generateRandomBiome (+ RNG state)    pass  20000  fail 0  not covered 0
  SelectBiomePhase (+ RNG state)       pass  20000  fail 0  not covered 0
```

Integrated final candidate replay:

```sh
/workspace/scratch/809689da2702/task63-resume/run-core/build/biometest /workspace/scratch/809689da2702/validation/r63-final/777-biome-cases.txt
```
Integrated final exit 0. Printed summary:

```text
cases 20000
  initBiomeDepths                      pass  20000  fail 0  not covered 0
  generateRandomBiome (+ RNG state)    pass  20000  fail 0  not covered 0
  SelectBiomePhase (+ RNG state)       pass  20000  fail 0  not covered 0
```

### 777-encounter

```sh
BUILD=/workspace/scratch/809689da2702/validation/r63-final/777 sh run-core/tools/difftest.sh /workspace/scratch/809689da2702/upstream-r63-a 20000 777 encounter
```
Final exit 0. Printed summary:

```text
cases 20000 (daily final-boss overrides: 15)
  wave-cycle and gym offsets           pass  20000  fail 0  not covered 0
  getTimeOfDay                         pass  20000  fail 0  not covered 0
  randomSpecies to the evolution step (+ RNG) pass  20000  fail 0  not covered 0
  randomSpecies with evolution (+ RNG) pass  20000  fail 0  not covered 0
  determineEnemySpecies (+ RNG)        pass  20000  fail 0  not covered 0
  randomTrainerType (+ RNG state)      pass  20000  fail 0  not covered 0
```

Paired unchanged-main C replay:

```sh
/workspace/scratch/809689da2702/task63-baseline/run-core/build/encountertest /workspace/scratch/809689da2702/validation/r63-final/777/encounter-cases.txt
```
Before replay exit 0. Printed summary:

```text
cases 20000 (daily final-boss overrides: 15)
  wave-cycle and gym offsets           pass  20000  fail 0  not covered 0
  getTimeOfDay                         pass  20000  fail 0  not covered 0
  randomSpecies to the evolution step (+ RNG) pass  20000  fail 0  not covered 0
  randomSpecies with evolution (+ RNG) pass  20000  fail 0  not covered 0
  determineEnemySpecies (+ RNG)        pass  20000  fail 0  not covered 0
  randomTrainerType (+ RNG state)      pass  20000  fail 0  not covered 0
```

Integrated final candidate replay:

```sh
/workspace/scratch/809689da2702/task63-resume/run-core/build/encountertest /workspace/scratch/809689da2702/validation/r63-final/777-encounter-cases.txt
```
Integrated final exit 0. Printed summary:

```text
cases 20000 (daily final-boss overrides: 15)
  wave-cycle and gym offsets           pass  20000  fail 0  not covered 0
  getTimeOfDay                         pass  20000  fail 0  not covered 0
  randomSpecies to the evolution step (+ RNG) pass  20000  fail 0  not covered 0
  randomSpecies with evolution (+ RNG) pass  20000  fail 0  not covered 0
  determineEnemySpecies (+ RNG)        pass  20000  fail 0  not covered 0
  randomTrainerType (+ RNG state)      pass  20000  fail 0  not covered 0
```

### 777-newbattle

```sh
BUILD=/workspace/scratch/809689da2702/validation/r63-final/777 sh run-core/tools/difftest.sh /workspace/scratch/809689da2702/upstream-r63-a 20000 777 newbattle
```
Final exit 0. Printed summary:

```text
cases 20000 (trainers that couldn't be built upstream: 0)
  isMysteryEncounterValidForWave       pass  20000  fail 0  not covered 0
  isWaveMysteryEncounter               pass  20000  fail 0  not covered 0
  Trainer.getPartyLevels               pass  20000  fail 0  not covered 0
```

Paired unchanged-main C replay:

```sh
/workspace/scratch/809689da2702/task63-baseline/run-core/build/newbattletest /workspace/scratch/809689da2702/validation/r63-final/777/newbattle-cases.txt
```
Before replay exit 0. Printed summary:

```text
cases 20000 (trainers that couldn't be built upstream: 0)
  isMysteryEncounterValidForWave       pass  20000  fail 0  not covered 0
  isWaveMysteryEncounter               pass  20000  fail 0  not covered 0
  Trainer.getPartyLevels               pass  20000  fail 0  not covered 0
```

Integrated final candidate replay:

```sh
/workspace/scratch/809689da2702/task63-resume/run-core/build/newbattletest /workspace/scratch/809689da2702/validation/r63-final/777-newbattle-cases.txt
```
Integrated final exit 0. Printed summary:

```text
cases 20000 (trainers that couldn't be built upstream: 0)
  isMysteryEncounterValidForWave       pass  20000  fail 0  not covered 0
  isWaveMysteryEncounter               pass  20000  fail 0  not covered 0
  Trainer.getPartyLevels               pass  20000  fail 0  not covered 0
```

### 777-exp

```sh
BUILD=/workspace/scratch/809689da2702/validation/r63-final/777 sh run-core/tools/difftest.sh /workspace/scratch/809689da2702/upstream-r63-a 20000 777 exp
```
Final exit 0. Printed summary:

```text
getLevelTotalExp + getLevelRelExp: pass 138006  fail 0
```

Paired unchanged-main C replay:

```sh
/workspace/scratch/809689da2702/task63-baseline/run-core/build/exptest /workspace/scratch/809689da2702/validation/r63-final/777/exp-cases.txt
```
Before replay exit 0. Printed summary:

```text
getLevelTotalExp + getLevelRelExp: pass 138006  fail 0
```

Integrated final candidate replay:

```sh
/workspace/scratch/809689da2702/task63-resume/run-core/build/exptest /workspace/scratch/809689da2702/validation/r63-final/777-exp-cases.txt
```
Integrated final exit 0. Printed summary:

```text
getLevelTotalExp + getLevelRelExp: pass 138006  fail 0
```

### 777-partyexp

```sh
BUILD=/workspace/scratch/809689da2702/validation/r63-final/777 sh run-core/tools/difftest.sh /workspace/scratch/809689da2702/upstream-r63-a 20000 777 partyexp
```
Final exit 0. Printed summary:

```text
applyPartyExp: pass 20000  fail 0 (experience given to 31343 party members)
```

Paired unchanged-main C replay:

```sh
/workspace/scratch/809689da2702/task63-baseline/run-core/build/partyexptest /workspace/scratch/809689da2702/validation/r63-final/777/partyexp-cases.txt
```
Before replay exit 0. Printed summary:

```text
applyPartyExp: pass 20000  fail 0 (experience given to 31343 party members)
```

Integrated final candidate replay:

```sh
/workspace/scratch/809689da2702/task63-resume/run-core/build/partyexptest /workspace/scratch/809689da2702/validation/r63-final/777-partyexp-cases.txt
```
Integrated final exit 0. Printed summary:

```text
applyPartyExp: pass 20000  fail 0 (experience given to 31343 party members)
```

### 777-upkeep

```sh
BUILD=/workspace/scratch/809689da2702/validation/r63-final/777 sh run-core/tools/difftest.sh /workspace/scratch/809689da2702/upstream-r63-a 20000 777 upkeep
```
Final exit 0. Printed summary:

```text
trySpreadPokerus: pass 20000  fail 0 (2174 Pokemon caught it)
Coin Case interest: pass 20000  fail 0
```

Paired unchanged-main C replay:

```sh
/workspace/scratch/809689da2702/task63-baseline/run-core/build/upkeeptest /workspace/scratch/809689da2702/validation/r63-final/777/upkeep-cases.txt
```
Before replay exit 0. Printed summary:

```text
trySpreadPokerus: pass 20000  fail 0 (2174 Pokemon caught it)
Coin Case interest: pass 20000  fail 0
```

Integrated final candidate replay:

```sh
/workspace/scratch/809689da2702/task63-resume/run-core/build/upkeeptest /workspace/scratch/809689da2702/validation/r63-final/777-upkeep-cases.txt
```
Integrated final exit 0. Printed summary:

```text
trySpreadPokerus: pass 20000  fail 0 (2174 Pokemon caught it)
Coin Case interest: pass 20000  fail 0
```

### 777-trainerspecies

```sh
BUILD=/workspace/scratch/809689da2702/validation/r63-final/777 sh run-core/tools/difftest.sh /workspace/scratch/809689da2702/upstream-r63-a 20000 777 trainerspecies
```
Final exit 0. Printed summary:

```text
cases 20000 (12332 from species pools; upstream threw: 0)
  genNewPartyMemberSpecies (+ RNG)     pass  20000  fail 0  not covered 0
```

Paired unchanged-main C replay:

```sh
/workspace/scratch/809689da2702/task63-baseline/run-core/build/trainerspeciestest /workspace/scratch/809689da2702/validation/r63-final/777/trainerspecies-cases.txt
```
Before replay exit 0. Printed summary:

```text
cases 20000 (12332 from species pools; upstream threw: 0)
  genNewPartyMemberSpecies (+ RNG)     pass  20000  fail 0  not covered 0
```

Integrated final candidate replay:

```sh
/workspace/scratch/809689da2702/task63-resume/run-core/build/trainerspeciestest /workspace/scratch/809689da2702/validation/r63-final/777-trainerspecies-cases.txt
```
Integrated final exit 0. Printed summary:

```text
cases 20000 (12332 from species pools; upstream threw: 0)
  genNewPartyMemberSpecies (+ RNG)     pass  20000  fail 0  not covered 0
```

### 777-luck

```sh
BUILD=/workspace/scratch/809689da2702/validation/r63-final/777 sh run-core/tools/difftest.sh /workspace/scratch/809689da2702/upstream-r63-a 20000 777 luck
```
Final exit 0. Printed summary:

```text
cases 20000 (daily 4037, at the cap of 14: 1220)
  getPartyLuckValue (+ RNG)            pass  20000  fail 0  not covered 0
```

Paired unchanged-main C replay:

```sh
/workspace/scratch/809689da2702/task63-baseline/run-core/build/lucktest /workspace/scratch/809689da2702/validation/r63-final/777/luck-cases.txt
```
Before replay exit 0. Printed summary:

```text
cases 20000 (daily 4037, at the cap of 14: 1220)
  getPartyLuckValue (+ RNG)            pass  20000  fail 0  not covered 0
```

Integrated final candidate replay:

```sh
/workspace/scratch/809689da2702/task63-resume/run-core/build/lucktest /workspace/scratch/809689da2702/validation/r63-final/777-luck-cases.txt
```
Integrated final exit 0. Printed summary:

```text
cases 20000 (daily 4037, at the cap of 14: 1220)
  getPartyLuckValue (+ RNG)            pass  20000  fail 0  not covered 0
```

### 777-daily

```sh
BUILD=/workspace/scratch/809689da2702/validation/r63-final/777 sh run-core/tools/difftest.sh /workspace/scratch/809689da2702/upstream-r63-a 20000 777 daily
```
Final exit 0. Printed summary:

```text
dailytest summary: 5 daily runs (250 waves), 775 checks passed, 0 mismatches
```

Paired unchanged-main C replay:

```sh
/workspace/scratch/809689da2702/task63-baseline/run-core/build/dailytest /workspace/scratch/809689da2702/validation/r63-final/777/daily-cases.txt
```
Before replay exit 1. Printed summary:

```text
line 3: FAIL starter 1 got sp 8128 ab 1 fem 0 f 0 sh 0 v 0 n 4 exp sp 8128 ab 1 fem 0 f 0 sh 0 v 0 n 22
line 4: FAIL starter 2 got sp 519 ab 1 fem 0 f 0 sh 0 v 0 n 8 exp sp 351 ab 0 fem 1 f 0 sh 0 v 0 n 8
dailytest summary: 5 daily runs (250 waves), 773 checks passed, 2 mismatches
```

Integrated final candidate replay:

```sh
/workspace/scratch/809689da2702/task63-resume/run-core/build/dailytest /workspace/scratch/809689da2702/validation/r63-final/777-daily-cases.txt
```
Integrated final exit 0. Printed summary:

```text
dailytest summary: 5 daily runs (250 waves), 775 checks passed, 0 mismatches
```

### 777-battlegen

```sh
BUILD=/workspace/scratch/809689da2702/validation/r63-final/777 sh run-core/tools/difftest.sh /workspace/scratch/809689da2702/upstream-r63-a 10000 777 battlegen
```
Final exit 0. Printed summary:

```text
newBattle: pass 9703  fail 0  not covered 297 (wild 8475, trainer 695, mystery encounter 533; doubles 2238)
```

Paired unchanged-main C replay:

```sh
/workspace/scratch/809689da2702/task63-baseline/run-core/build/battlegentest /workspace/scratch/809689da2702/validation/r63-final/777/battlegen-cases.txt
```
Before replay exit 0. Printed summary:

```text
newBattle: pass 9703  fail 0  not covered 297 (wild 8475, trainer 695, mystery encounter 533; doubles 2238)
```

Integrated final candidate replay:

```sh
/workspace/scratch/809689da2702/task63-resume/run-core/build/battlegentest /workspace/scratch/809689da2702/validation/r63-final/777-battlegen-cases.txt
```
Integrated final exit 0. Printed summary:

```text
newBattle: pass 9703  fail 0  not covered 297 (wild 8475, trainer 695, mystery encounter 533; doubles 2238)
```

### 777-pokemongen

```sh
BUILD=/workspace/scratch/809689da2702/validation/r63-final/777 sh run-core/tools/difftest.sh /workspace/scratch/809689da2702/upstream-r63-a 8000 777 pokemongen
```
Final exit 0. Printed summary:

```text
cases 8000 (addEnemyPokemon threw upstream: 0); checked: 598 shiny, 154 rare/epic variants, 2695 bosses, 3078 trainer Pokemon, 1420 fusions
  constructor up to the moveset (+ RNG)            pass   8000  fail 0  not covered 0
  constructor after it, addEnemyPokemon (+ RNG)    pass   8000  fail 0  not covered 0
```

Paired unchanged-main C replay:

```sh
/workspace/scratch/809689da2702/task63-baseline/run-core/build/pokemongentest /workspace/scratch/809689da2702/validation/r63-final/777-pokemongen-cases.txt
```
Before replay exit 0. Printed summary:

```text
cases 8000 (addEnemyPokemon threw upstream: 0); checked: 598 shiny, 154 rare/epic variants, 2695 bosses, 3078 trainer Pokemon, 1420 fusions
  constructor up to the moveset (+ RNG)            pass   8000  fail 0  not covered 0
  constructor after it, addEnemyPokemon (+ RNG)    pass   8000  fail 0  not covered 0
```

Integrated final candidate replay:

```sh
/workspace/scratch/809689da2702/task63-resume/run-core/build/pokemongentest /workspace/scratch/809689da2702/validation/r63-final/777-pokemongen-cases.txt
```
Integrated final exit 0. Printed summary:

```text
cases 8000 (addEnemyPokemon threw upstream: 0); checked: 598 shiny, 154 rare/epic variants, 2695 bosses, 3078 trainer Pokemon, 1420 fusions
  constructor up to the moveset (+ RNG)            pass   8000  fail 0  not covered 0
  constructor after it, addEnemyPokemon (+ RNG)    pass   8000  fail 0  not covered 0
```

### 777-trainerparty

```sh
BUILD=/workspace/scratch/809689da2702/validation/r63-final/777 sh run-core/tools/difftest.sh /workspace/scratch/809689da2702/upstream-r63-a 3000 777 trainerparty
```
Final exit 0. Printed summary:

```text
cases 3000 (1630 double; whole parties matched: 2525; members upstream threw on: 106; fused members: 1793)
  genPartyMember (species, rolls)      pass   9662  fail 0  not covered 1792
```

Paired unchanged-main C replay:

```sh
/workspace/scratch/809689da2702/task63-baseline/run-core/build/trainerpartytest /workspace/scratch/809689da2702/validation/r63-final/777-trainerparty-cases.txt
```
Before replay exit 0. Printed summary:

```text
cases 3000 (1630 double; whole parties matched: 2525; members upstream threw on: 106; fused members: 1793)
  genPartyMember (species, rolls)      pass   9662  fail 0  not covered 1792
```

Integrated final candidate replay:

```sh
/workspace/scratch/809689da2702/task63-resume/run-core/build/trainerpartytest /workspace/scratch/809689da2702/validation/r63-final/777-trainerparty-cases.txt
```
Integrated final exit 0. Printed summary:

```text
cases 3000 (1630 double; whole parties matched: 2525; members upstream threw on: 106; fused members: 1793)
  genPartyMember (species, rolls)      pass   9662  fail 0  not covered 1792
```

### 9001-wave

```sh
BUILD=/workspace/scratch/809689da2702/validation/r63-final/9001 sh run-core/tools/difftest.sh /workspace/scratch/809689da2702/upstream-r63-c 20000 9001 wave
```
Final exit 0. Printed summary:

```text
cases 20000
  game-mode rules (15 per case)        pass 300000  fail 0  not covered 0
  getMaxExpLevel                       pass  20000  fail 0  not covered 0
  getLevelForWave (+ RNG state)        pass  20000  fail 0  not covered 0
  isWaveTrainer (+ RNG state)          pass  20000  fail 0  not covered 0
  resetSeed + Battle seeding chain     pass  20000  fail 0  not covered 0
  getEncounterBossSegments             pass  20000  fail 0  not covered 0
  getDoubleBattleChance                pass  20000  fail 0  not covered 0
```

Paired unchanged-main C replay:

```sh
/workspace/scratch/809689da2702/task63-baseline/run-core/build/wavetest /workspace/scratch/809689da2702/validation/r63-final/9001-wave-cases.txt
```
Before replay exit 0. Printed summary:

```text
cases 20000
  game-mode rules (15 per case)        pass 300000  fail 0  not covered 0
  getMaxExpLevel                       pass  20000  fail 0  not covered 0
  getLevelForWave (+ RNG state)        pass  20000  fail 0  not covered 0
  isWaveTrainer (+ RNG state)          pass  20000  fail 0  not covered 0
  resetSeed + Battle seeding chain     pass  20000  fail 0  not covered 0
  getEncounterBossSegments             pass  20000  fail 0  not covered 0
  getDoubleBattleChance                pass  20000  fail 0  not covered 0
```

Integrated final candidate replay:

```sh
/workspace/scratch/809689da2702/task63-resume/run-core/build/wavetest /workspace/scratch/809689da2702/validation/r63-final/9001-wave-cases.txt
```
Integrated final exit 0. Printed summary:

```text
cases 20000
  game-mode rules (15 per case)        pass 300000  fail 0  not covered 0
  getMaxExpLevel                       pass  20000  fail 0  not covered 0
  getLevelForWave (+ RNG state)        pass  20000  fail 0  not covered 0
  isWaveTrainer (+ RNG state)          pass  20000  fail 0  not covered 0
  resetSeed + Battle seeding chain     pass  20000  fail 0  not covered 0
  getEncounterBossSegments             pass  20000  fail 0  not covered 0
  getDoubleBattleChance                pass  20000  fail 0  not covered 0
```

### 9001-shop

```sh
BUILD=/workspace/scratch/809689da2702/validation/r63-final/9001 sh run-core/tools/difftest.sh /workspace/scratch/809689da2702/upstream-r63-c 20000 9001 shop
```
Final exit 0. Printed summary:

```text
cases 20000
  getWaveMoneyAmount                       pass  20000  fail 0  not covered 0
  money reward (Amulet Coin, Happy Hour)   pass  20000  fail 0  not covered 0
  getShopStatus                            pass  20000  fail 0  not covered 0
  shop options as shown (Black Sludge)     pass  20000  fail 0  not covered 0
  shop options as charged                  pass  20000  fail 0  not covered 0
  getRerollCost                            pass  20000  fail 0  not covered 0
```

Paired unchanged-main C replay:

```sh
/workspace/scratch/809689da2702/task63-baseline/run-core/build/shoptest /workspace/scratch/809689da2702/validation/r63-final/9001-shop-cases.txt
```
Before replay exit 0. Printed summary:

```text
cases 20000
  getWaveMoneyAmount                       pass  20000  fail 0  not covered 0
  money reward (Amulet Coin, Happy Hour)   pass  20000  fail 0  not covered 0
  getShopStatus                            pass  20000  fail 0  not covered 0
  shop options as shown (Black Sludge)     pass  20000  fail 0  not covered 0
  shop options as charged                  pass  20000  fail 0  not covered 0
  getRerollCost                            pass  20000  fail 0  not covered 0
```

Integrated final candidate replay:

```sh
/workspace/scratch/809689da2702/task63-resume/run-core/build/shoptest /workspace/scratch/809689da2702/validation/r63-final/9001-shop-cases.txt
```
Integrated final exit 0. Printed summary:

```text
cases 20000
  getWaveMoneyAmount                       pass  20000  fail 0  not covered 0
  money reward (Amulet Coin, Happy Hour)   pass  20000  fail 0  not covered 0
  getShopStatus                            pass  20000  fail 0  not covered 0
  shop options as shown (Black Sludge)     pass  20000  fail 0  not covered 0
  shop options as charged                  pass  20000  fail 0  not covered 0
  getRerollCost                            pass  20000  fail 0  not covered 0
```

### 9001-biome

```sh
BUILD=/workspace/scratch/809689da2702/validation/r63-final/9001 sh run-core/tools/difftest.sh /workspace/scratch/809689da2702/upstream-r63-c 20000 9001 biome
```
Final exit 0. Printed summary:

```text
cases 20000
  initBiomeDepths                      pass  20000  fail 0  not covered 0
  generateRandomBiome (+ RNG state)    pass  20000  fail 0  not covered 0
  SelectBiomePhase (+ RNG state)       pass  20000  fail 0  not covered 0
```

Paired unchanged-main C replay:

```sh
/workspace/scratch/809689da2702/task63-baseline/run-core/build/biometest /workspace/scratch/809689da2702/validation/r63-final/9001-biome-cases.txt
```
Before replay exit 0. Printed summary:

```text
cases 20000
  initBiomeDepths                      pass  20000  fail 0  not covered 0
  generateRandomBiome (+ RNG state)    pass  20000  fail 0  not covered 0
  SelectBiomePhase (+ RNG state)       pass  20000  fail 0  not covered 0
```

Integrated final candidate replay:

```sh
/workspace/scratch/809689da2702/task63-resume/run-core/build/biometest /workspace/scratch/809689da2702/validation/r63-final/9001-biome-cases.txt
```
Integrated final exit 0. Printed summary:

```text
cases 20000
  initBiomeDepths                      pass  20000  fail 0  not covered 0
  generateRandomBiome (+ RNG state)    pass  20000  fail 0  not covered 0
  SelectBiomePhase (+ RNG state)       pass  20000  fail 0  not covered 0
```

### 9001-encounter

```sh
BUILD=/workspace/scratch/809689da2702/validation/r63-final/9001 sh run-core/tools/difftest.sh /workspace/scratch/809689da2702/upstream-r63-c 20000 9001 encounter
```
Final exit 0. Printed summary:

```text
cases 20000 (daily final-boss overrides: 14)
  wave-cycle and gym offsets           pass  20000  fail 0  not covered 0
  getTimeOfDay                         pass  20000  fail 0  not covered 0
  randomSpecies to the evolution step (+ RNG) pass  20000  fail 0  not covered 0
  randomSpecies with evolution (+ RNG) pass  20000  fail 0  not covered 0
  determineEnemySpecies (+ RNG)        pass  20000  fail 0  not covered 0
  randomTrainerType (+ RNG state)      pass  20000  fail 0  not covered 0
```

Paired unchanged-main C replay:

```sh
/workspace/scratch/809689da2702/task63-baseline/run-core/build/encountertest /workspace/scratch/809689da2702/validation/r63-final/9001-encounter-cases.txt
```
Before replay exit 0. Printed summary:

```text
cases 20000 (daily final-boss overrides: 14)
  wave-cycle and gym offsets           pass  20000  fail 0  not covered 0
  getTimeOfDay                         pass  20000  fail 0  not covered 0
  randomSpecies to the evolution step (+ RNG) pass  20000  fail 0  not covered 0
  randomSpecies with evolution (+ RNG) pass  20000  fail 0  not covered 0
  determineEnemySpecies (+ RNG)        pass  20000  fail 0  not covered 0
  randomTrainerType (+ RNG state)      pass  20000  fail 0  not covered 0
```

Integrated final candidate replay:

```sh
/workspace/scratch/809689da2702/task63-resume/run-core/build/encountertest /workspace/scratch/809689da2702/validation/r63-final/9001-encounter-cases.txt
```
Integrated final exit 0. Printed summary:

```text
cases 20000 (daily final-boss overrides: 14)
  wave-cycle and gym offsets           pass  20000  fail 0  not covered 0
  getTimeOfDay                         pass  20000  fail 0  not covered 0
  randomSpecies to the evolution step (+ RNG) pass  20000  fail 0  not covered 0
  randomSpecies with evolution (+ RNG) pass  20000  fail 0  not covered 0
  determineEnemySpecies (+ RNG)        pass  20000  fail 0  not covered 0
  randomTrainerType (+ RNG state)      pass  20000  fail 0  not covered 0
```

### 9001-newbattle

```sh
BUILD=/workspace/scratch/809689da2702/validation/r63-final/9001 sh run-core/tools/difftest.sh /workspace/scratch/809689da2702/upstream-r63-c 20000 9001 newbattle
```
Final exit 0. Printed summary:

```text
cases 20000 (trainers that couldn't be built upstream: 0)
  isMysteryEncounterValidForWave       pass  20000  fail 0  not covered 0
  isWaveMysteryEncounter               pass  20000  fail 0  not covered 0
  Trainer.getPartyLevels               pass  20000  fail 0  not covered 0
```

Paired unchanged-main C replay:

```sh
/workspace/scratch/809689da2702/task63-baseline/run-core/build/newbattletest /workspace/scratch/809689da2702/validation/r63-final/9001-newbattle-cases.txt
```
Before replay exit 0. Printed summary:

```text
cases 20000 (trainers that couldn't be built upstream: 0)
  isMysteryEncounterValidForWave       pass  20000  fail 0  not covered 0
  isWaveMysteryEncounter               pass  20000  fail 0  not covered 0
  Trainer.getPartyLevels               pass  20000  fail 0  not covered 0
```

Integrated final candidate replay:

```sh
/workspace/scratch/809689da2702/task63-resume/run-core/build/newbattletest /workspace/scratch/809689da2702/validation/r63-final/9001-newbattle-cases.txt
```
Integrated final exit 0. Printed summary:

```text
cases 20000 (trainers that couldn't be built upstream: 0)
  isMysteryEncounterValidForWave       pass  20000  fail 0  not covered 0
  isWaveMysteryEncounter               pass  20000  fail 0  not covered 0
  Trainer.getPartyLevels               pass  20000  fail 0  not covered 0
```

### 9001-exp

```sh
BUILD=/workspace/scratch/809689da2702/validation/r63-final/9001 sh run-core/tools/difftest.sh /workspace/scratch/809689da2702/upstream-r63-c 20000 9001 exp
```
Final exit 0. Printed summary:

```text
getLevelTotalExp + getLevelRelExp: pass 138006  fail 0
```

Paired unchanged-main C replay:

```sh
/workspace/scratch/809689da2702/task63-baseline/run-core/build/exptest /workspace/scratch/809689da2702/validation/r63-final/9001-exp-cases.txt
```
Before replay exit 0. Printed summary:

```text
getLevelTotalExp + getLevelRelExp: pass 138006  fail 0
```

Integrated final candidate replay:

```sh
/workspace/scratch/809689da2702/task63-resume/run-core/build/exptest /workspace/scratch/809689da2702/validation/r63-final/9001-exp-cases.txt
```
Integrated final exit 0. Printed summary:

```text
getLevelTotalExp + getLevelRelExp: pass 138006  fail 0
```

### 9001-partyexp

```sh
BUILD=/workspace/scratch/809689da2702/validation/r63-final/9001 sh run-core/tools/difftest.sh /workspace/scratch/809689da2702/upstream-r63-c 20000 9001 partyexp
```
Final exit 0. Printed summary:

```text
applyPartyExp: pass 20000  fail 0 (experience given to 31407 party members)
```

Paired unchanged-main C replay:

```sh
/workspace/scratch/809689da2702/task63-baseline/run-core/build/partyexptest /workspace/scratch/809689da2702/validation/r63-final/9001-partyexp-cases.txt
```
Before replay exit 0. Printed summary:

```text
applyPartyExp: pass 20000  fail 0 (experience given to 31407 party members)
```

Integrated final candidate replay:

```sh
/workspace/scratch/809689da2702/task63-resume/run-core/build/partyexptest /workspace/scratch/809689da2702/validation/r63-final/9001-partyexp-cases.txt
```
Integrated final exit 0. Printed summary:

```text
applyPartyExp: pass 20000  fail 0 (experience given to 31407 party members)
```

### 9001-upkeep

```sh
BUILD=/workspace/scratch/809689da2702/validation/r63-final/9001 sh run-core/tools/difftest.sh /workspace/scratch/809689da2702/upstream-r63-c 20000 9001 upkeep
```
Final exit 0. Printed summary:

```text
trySpreadPokerus: pass 20000  fail 0 (2212 Pokemon caught it)
Coin Case interest: pass 20000  fail 0
```

Paired unchanged-main C replay:

```sh
/workspace/scratch/809689da2702/task63-baseline/run-core/build/upkeeptest /workspace/scratch/809689da2702/validation/r63-final/9001-upkeep-cases.txt
```
Before replay exit 0. Printed summary:

```text
trySpreadPokerus: pass 20000  fail 0 (2212 Pokemon caught it)
Coin Case interest: pass 20000  fail 0
```

Integrated final candidate replay:

```sh
/workspace/scratch/809689da2702/task63-resume/run-core/build/upkeeptest /workspace/scratch/809689da2702/validation/r63-final/9001-upkeep-cases.txt
```
Integrated final exit 0. Printed summary:

```text
trySpreadPokerus: pass 20000  fail 0 (2212 Pokemon caught it)
Coin Case interest: pass 20000  fail 0
```

### 9001-trainerspecies

```sh
BUILD=/workspace/scratch/809689da2702/validation/r63-final/9001 sh run-core/tools/difftest.sh /workspace/scratch/809689da2702/upstream-r63-c 20000 9001 trainerspecies
```
Final exit 0. Printed summary:

```text
cases 20000 (12180 from species pools; upstream threw: 0)
  genNewPartyMemberSpecies (+ RNG)     pass  20000  fail 0  not covered 0
```

Paired unchanged-main C replay:

```sh
/workspace/scratch/809689da2702/task63-baseline/run-core/build/trainerspeciestest /workspace/scratch/809689da2702/validation/r63-final/9001-trainerspecies-cases.txt
```
Before replay exit 0. Printed summary:

```text
cases 20000 (12180 from species pools; upstream threw: 0)
  genNewPartyMemberSpecies (+ RNG)     pass  20000  fail 0  not covered 0
```

Integrated final candidate replay:

```sh
/workspace/scratch/809689da2702/task63-resume/run-core/build/trainerspeciestest /workspace/scratch/809689da2702/validation/r63-final/9001-trainerspecies-cases.txt
```
Integrated final exit 0. Printed summary:

```text
cases 20000 (12180 from species pools; upstream threw: 0)
  genNewPartyMemberSpecies (+ RNG)     pass  20000  fail 0  not covered 0
```

### 9001-luck

```sh
BUILD=/workspace/scratch/809689da2702/validation/r63-final/9001 sh run-core/tools/difftest.sh /workspace/scratch/809689da2702/upstream-r63-c 20000 9001 luck
```
Final exit 0. Printed summary:

```text
cases 20000 (daily 4012, at the cap of 14: 1247)
  getPartyLuckValue (+ RNG)            pass  20000  fail 0  not covered 0
```

Paired unchanged-main C replay:

```sh
/workspace/scratch/809689da2702/task63-baseline/run-core/build/lucktest /workspace/scratch/809689da2702/validation/r63-final/9001-luck-cases.txt
```
Before replay exit 0. Printed summary:

```text
cases 20000 (daily 4012, at the cap of 14: 1247)
  getPartyLuckValue (+ RNG)            pass  20000  fail 0  not covered 0
```

Integrated final candidate replay:

```sh
/workspace/scratch/809689da2702/task63-resume/run-core/build/lucktest /workspace/scratch/809689da2702/validation/r63-final/9001-luck-cases.txt
```
Integrated final exit 0. Printed summary:

```text
cases 20000 (daily 4012, at the cap of 14: 1247)
  getPartyLuckValue (+ RNG)            pass  20000  fail 0  not covered 0
```

### 9001-daily

```sh
BUILD=/workspace/scratch/809689da2702/validation/r63-final/9001 sh run-core/tools/difftest.sh /workspace/scratch/809689da2702/upstream-r63-c 20000 9001 daily
```
Final exit 0. Printed summary:

```text
dailytest summary: 5 daily runs (250 waves), 775 checks passed, 0 mismatches
```

Paired unchanged-main C replay:

```sh
/workspace/scratch/809689da2702/task63-baseline/run-core/build/dailytest /workspace/scratch/809689da2702/validation/r63-final/9001-daily-cases.txt
```
Before replay exit 1. Printed summary:

```text
line 3: FAIL starter 1 got sp 8128 ab 1 fem 0 f 0 sh 0 v 0 n 4 exp sp 8128 ab 1 fem 0 f 0 sh 0 v 0 n 22
line 4: FAIL starter 2 got sp 519 ab 1 fem 0 f 0 sh 0 v 0 n 8 exp sp 351 ab 0 fem 1 f 0 sh 0 v 0 n 8
dailytest summary: 5 daily runs (250 waves), 773 checks passed, 2 mismatches
```

Integrated final candidate replay:

```sh
/workspace/scratch/809689da2702/task63-resume/run-core/build/dailytest /workspace/scratch/809689da2702/validation/r63-final/9001-daily-cases.txt
```
Integrated final exit 0. Printed summary:

```text
dailytest summary: 5 daily runs (250 waves), 775 checks passed, 0 mismatches
```

### 9001-battlegen

```sh
BUILD=/workspace/scratch/809689da2702/validation/r63-final/9001 sh run-core/tools/difftest.sh /workspace/scratch/809689da2702/upstream-r63-c 10000 9001 battlegen
```
Final exit 0. Printed summary:

```text
newBattle: pass 9655  fail 0  not covered 345 (wild 8398, trainer 674, mystery encounter 583; doubles 2254)
```

Paired unchanged-main C replay:

```sh
/workspace/scratch/809689da2702/task63-baseline/run-core/build/battlegentest /workspace/scratch/809689da2702/validation/r63-final/9001-battlegen-cases.txt
```
Before replay exit 0. Printed summary:

```text
newBattle: pass 9655  fail 0  not covered 345 (wild 8398, trainer 674, mystery encounter 583; doubles 2254)
```

Integrated final candidate replay:

```sh
/workspace/scratch/809689da2702/task63-resume/run-core/build/battlegentest /workspace/scratch/809689da2702/validation/r63-final/9001-battlegen-cases.txt
```
Integrated final exit 0. Printed summary:

```text
newBattle: pass 9655  fail 0  not covered 345 (wild 8398, trainer 674, mystery encounter 583; doubles 2254)
```

### 9001-pokemongen

```sh
BUILD=/workspace/scratch/809689da2702/validation/r63-final/9001 sh run-core/tools/difftest.sh /workspace/scratch/809689da2702/upstream-r63-c 8000 9001 pokemongen
```
Final exit 0. Printed summary:

```text
cases 8000 (addEnemyPokemon threw upstream: 0); checked: 635 shiny, 173 rare/epic variants, 2720 bosses, 3046 trainer Pokemon, 1381 fusions
  constructor up to the moveset (+ RNG)            pass   8000  fail 0  not covered 0
  constructor after it, addEnemyPokemon (+ RNG)    pass   8000  fail 0  not covered 0
```

Paired unchanged-main C replay:

```sh
/workspace/scratch/809689da2702/task63-baseline/run-core/build/pokemongentest /workspace/scratch/809689da2702/validation/r63-final/9001-pokemongen-cases.txt
```
Before replay exit 0. Printed summary:

```text
cases 8000 (addEnemyPokemon threw upstream: 0); checked: 635 shiny, 173 rare/epic variants, 2720 bosses, 3046 trainer Pokemon, 1381 fusions
  constructor up to the moveset (+ RNG)            pass   8000  fail 0  not covered 0
  constructor after it, addEnemyPokemon (+ RNG)    pass   8000  fail 0  not covered 0
```

Integrated final candidate replay:

```sh
/workspace/scratch/809689da2702/task63-resume/run-core/build/pokemongentest /workspace/scratch/809689da2702/validation/r63-final/9001-pokemongen-cases.txt
```
Integrated final exit 0. Printed summary:

```text
cases 8000 (addEnemyPokemon threw upstream: 0); checked: 635 shiny, 173 rare/epic variants, 2720 bosses, 3046 trainer Pokemon, 1381 fusions
  constructor up to the moveset (+ RNG)            pass   8000  fail 0  not covered 0
  constructor after it, addEnemyPokemon (+ RNG)    pass   8000  fail 0  not covered 0
```

### 4242-wave

```sh
BUILD=/workspace/scratch/809689da2702/validation/r63-final/4242 sh run-core/tools/difftest.sh /workspace/scratch/809689da2702/upstream-r63-b 20000 4242 wave
```
Final exit 0. Printed summary:

```text
cases 20000
  game-mode rules (15 per case)        pass 300000  fail 0  not covered 0
  getMaxExpLevel                       pass  20000  fail 0  not covered 0
  getLevelForWave (+ RNG state)        pass  20000  fail 0  not covered 0
  isWaveTrainer (+ RNG state)          pass  20000  fail 0  not covered 0
  resetSeed + Battle seeding chain     pass  20000  fail 0  not covered 0
  getEncounterBossSegments             pass  20000  fail 0  not covered 0
  getDoubleBattleChance                pass  20000  fail 0  not covered 0
```

Paired unchanged-main C replay:

```sh
/workspace/scratch/809689da2702/task63-baseline/run-core/build/wavetest /workspace/scratch/809689da2702/validation/r63-final/4242/wave-cases.txt
```
Before replay exit 0. Printed summary:

```text
cases 20000
  game-mode rules (15 per case)        pass 300000  fail 0  not covered 0
  getMaxExpLevel                       pass  20000  fail 0  not covered 0
  getLevelForWave (+ RNG state)        pass  20000  fail 0  not covered 0
  isWaveTrainer (+ RNG state)          pass  20000  fail 0  not covered 0
  resetSeed + Battle seeding chain     pass  20000  fail 0  not covered 0
  getEncounterBossSegments             pass  20000  fail 0  not covered 0
  getDoubleBattleChance                pass  20000  fail 0  not covered 0
```

Integrated final candidate replay:

```sh
/workspace/scratch/809689da2702/task63-resume/run-core/build/wavetest /workspace/scratch/809689da2702/validation/r63-final/4242-wave-cases.txt
```
Integrated final exit 0. Printed summary:

```text
cases 20000
  game-mode rules (15 per case)        pass 300000  fail 0  not covered 0
  getMaxExpLevel                       pass  20000  fail 0  not covered 0
  getLevelForWave (+ RNG state)        pass  20000  fail 0  not covered 0
  isWaveTrainer (+ RNG state)          pass  20000  fail 0  not covered 0
  resetSeed + Battle seeding chain     pass  20000  fail 0  not covered 0
  getEncounterBossSegments             pass  20000  fail 0  not covered 0
  getDoubleBattleChance                pass  20000  fail 0  not covered 0
```

### 4242-shop

```sh
BUILD=/workspace/scratch/809689da2702/validation/r63-final/4242 sh run-core/tools/difftest.sh /workspace/scratch/809689da2702/upstream-r63-b 20000 4242 shop
```
Final exit 0. Printed summary:

```text
cases 20000
  getWaveMoneyAmount                       pass  20000  fail 0  not covered 0
  money reward (Amulet Coin, Happy Hour)   pass  20000  fail 0  not covered 0
  getShopStatus                            pass  20000  fail 0  not covered 0
  shop options as shown (Black Sludge)     pass  20000  fail 0  not covered 0
  shop options as charged                  pass  20000  fail 0  not covered 0
  getRerollCost                            pass  20000  fail 0  not covered 0
```

Paired unchanged-main C replay:

```sh
/workspace/scratch/809689da2702/task63-baseline/run-core/build/shoptest /workspace/scratch/809689da2702/validation/r63-final/4242/shop-cases.txt
```
Before replay exit 0. Printed summary:

```text
cases 20000
  getWaveMoneyAmount                       pass  20000  fail 0  not covered 0
  money reward (Amulet Coin, Happy Hour)   pass  20000  fail 0  not covered 0
  getShopStatus                            pass  20000  fail 0  not covered 0
  shop options as shown (Black Sludge)     pass  20000  fail 0  not covered 0
  shop options as charged                  pass  20000  fail 0  not covered 0
  getRerollCost                            pass  20000  fail 0  not covered 0
```

Integrated final candidate replay:

```sh
/workspace/scratch/809689da2702/task63-resume/run-core/build/shoptest /workspace/scratch/809689da2702/validation/r63-final/4242-shop-cases.txt
```
Integrated final exit 0. Printed summary:

```text
cases 20000
  getWaveMoneyAmount                       pass  20000  fail 0  not covered 0
  money reward (Amulet Coin, Happy Hour)   pass  20000  fail 0  not covered 0
  getShopStatus                            pass  20000  fail 0  not covered 0
  shop options as shown (Black Sludge)     pass  20000  fail 0  not covered 0
  shop options as charged                  pass  20000  fail 0  not covered 0
  getRerollCost                            pass  20000  fail 0  not covered 0
```

### 4242-biome

```sh
BUILD=/workspace/scratch/809689da2702/validation/r63-final/4242 sh run-core/tools/difftest.sh /workspace/scratch/809689da2702/upstream-r63-b 20000 4242 biome
```
Final exit 0. Printed summary:

```text
cases 20000
  initBiomeDepths                      pass  20000  fail 0  not covered 0
  generateRandomBiome (+ RNG state)    pass  20000  fail 0  not covered 0
  SelectBiomePhase (+ RNG state)       pass  20000  fail 0  not covered 0
```

Paired unchanged-main C replay:

```sh
/workspace/scratch/809689da2702/task63-baseline/run-core/build/biometest /workspace/scratch/809689da2702/validation/r63-final/4242/biome-cases.txt
```
Before replay exit 0. Printed summary:

```text
cases 20000
  initBiomeDepths                      pass  20000  fail 0  not covered 0
  generateRandomBiome (+ RNG state)    pass  20000  fail 0  not covered 0
  SelectBiomePhase (+ RNG state)       pass  20000  fail 0  not covered 0
```

Integrated final candidate replay:

```sh
/workspace/scratch/809689da2702/task63-resume/run-core/build/biometest /workspace/scratch/809689da2702/validation/r63-final/4242-biome-cases.txt
```
Integrated final exit 0. Printed summary:

```text
cases 20000
  initBiomeDepths                      pass  20000  fail 0  not covered 0
  generateRandomBiome (+ RNG state)    pass  20000  fail 0  not covered 0
  SelectBiomePhase (+ RNG state)       pass  20000  fail 0  not covered 0
```

### 4242-encounter

```sh
BUILD=/workspace/scratch/809689da2702/validation/r63-final/4242 sh run-core/tools/difftest.sh /workspace/scratch/809689da2702/upstream-r63-b 20000 4242 encounter
```
Final exit 0. Printed summary:

```text
cases 20000 (daily final-boss overrides: 10)
  wave-cycle and gym offsets           pass  20000  fail 0  not covered 0
  getTimeOfDay                         pass  20000  fail 0  not covered 0
  randomSpecies to the evolution step (+ RNG) pass  20000  fail 0  not covered 0
  randomSpecies with evolution (+ RNG) pass  20000  fail 0  not covered 0
  determineEnemySpecies (+ RNG)        pass  20000  fail 0  not covered 0
  randomTrainerType (+ RNG state)      pass  20000  fail 0  not covered 0
```

Paired unchanged-main C replay:

```sh
/workspace/scratch/809689da2702/task63-baseline/run-core/build/encountertest /workspace/scratch/809689da2702/validation/r63-final/4242/encounter-cases.txt
```
Before replay exit 0. Printed summary:

```text
cases 20000 (daily final-boss overrides: 10)
  wave-cycle and gym offsets           pass  20000  fail 0  not covered 0
  getTimeOfDay                         pass  20000  fail 0  not covered 0
  randomSpecies to the evolution step (+ RNG) pass  20000  fail 0  not covered 0
  randomSpecies with evolution (+ RNG) pass  20000  fail 0  not covered 0
  determineEnemySpecies (+ RNG)        pass  20000  fail 0  not covered 0
  randomTrainerType (+ RNG state)      pass  20000  fail 0  not covered 0
```

Integrated final candidate replay:

```sh
/workspace/scratch/809689da2702/task63-resume/run-core/build/encountertest /workspace/scratch/809689da2702/validation/r63-final/4242-encounter-cases.txt
```
Integrated final exit 0. Printed summary:

```text
cases 20000 (daily final-boss overrides: 10)
  wave-cycle and gym offsets           pass  20000  fail 0  not covered 0
  getTimeOfDay                         pass  20000  fail 0  not covered 0
  randomSpecies to the evolution step (+ RNG) pass  20000  fail 0  not covered 0
  randomSpecies with evolution (+ RNG) pass  20000  fail 0  not covered 0
  determineEnemySpecies (+ RNG)        pass  20000  fail 0  not covered 0
  randomTrainerType (+ RNG state)      pass  20000  fail 0  not covered 0
```

### 4242-newbattle

```sh
BUILD=/workspace/scratch/809689da2702/validation/r63-final/4242 sh run-core/tools/difftest.sh /workspace/scratch/809689da2702/upstream-r63-b 20000 4242 newbattle
```
Final exit 0. Printed summary:

```text
cases 20000 (trainers that couldn't be built upstream: 0)
  isMysteryEncounterValidForWave       pass  20000  fail 0  not covered 0
  isWaveMysteryEncounter               pass  20000  fail 0  not covered 0
  Trainer.getPartyLevels               pass  20000  fail 0  not covered 0
```

Paired unchanged-main C replay:

```sh
/workspace/scratch/809689da2702/task63-baseline/run-core/build/newbattletest /workspace/scratch/809689da2702/validation/r63-final/4242/newbattle-cases.txt
```
Before replay exit 0. Printed summary:

```text
cases 20000 (trainers that couldn't be built upstream: 0)
  isMysteryEncounterValidForWave       pass  20000  fail 0  not covered 0
  isWaveMysteryEncounter               pass  20000  fail 0  not covered 0
  Trainer.getPartyLevels               pass  20000  fail 0  not covered 0
```

Integrated final candidate replay:

```sh
/workspace/scratch/809689da2702/task63-resume/run-core/build/newbattletest /workspace/scratch/809689da2702/validation/r63-final/4242-newbattle-cases.txt
```
Integrated final exit 0. Printed summary:

```text
cases 20000 (trainers that couldn't be built upstream: 0)
  isMysteryEncounterValidForWave       pass  20000  fail 0  not covered 0
  isWaveMysteryEncounter               pass  20000  fail 0  not covered 0
  Trainer.getPartyLevels               pass  20000  fail 0  not covered 0
```

### 4242-exp

```sh
BUILD=/workspace/scratch/809689da2702/validation/r63-final/4242 sh run-core/tools/difftest.sh /workspace/scratch/809689da2702/upstream-r63-b 20000 4242 exp
```
Final exit 0. Printed summary:

```text
getLevelTotalExp + getLevelRelExp: pass 138006  fail 0
```

Paired unchanged-main C replay:

```sh
/workspace/scratch/809689da2702/task63-baseline/run-core/build/exptest /workspace/scratch/809689da2702/validation/r63-final/4242/exp-cases.txt
```
Before replay exit 0. Printed summary:

```text
getLevelTotalExp + getLevelRelExp: pass 138006  fail 0
```

Integrated final candidate replay:

```sh
/workspace/scratch/809689da2702/task63-resume/run-core/build/exptest /workspace/scratch/809689da2702/validation/r63-final/4242-exp-cases.txt
```
Integrated final exit 0. Printed summary:

```text
getLevelTotalExp + getLevelRelExp: pass 138006  fail 0
```

### 4242-partyexp

```sh
BUILD=/workspace/scratch/809689da2702/validation/r63-final/4242 sh run-core/tools/difftest.sh /workspace/scratch/809689da2702/upstream-r63-b 20000 4242 partyexp
```
Final exit 0. Printed summary:

```text
applyPartyExp: pass 20000  fail 0 (experience given to 31442 party members)
```

Paired unchanged-main C replay:

```sh
/workspace/scratch/809689da2702/task63-baseline/run-core/build/partyexptest /workspace/scratch/809689da2702/validation/r63-final/4242/partyexp-cases.txt
```
Before replay exit 0. Printed summary:

```text
applyPartyExp: pass 20000  fail 0 (experience given to 31442 party members)
```

Integrated final candidate replay:

```sh
/workspace/scratch/809689da2702/task63-resume/run-core/build/partyexptest /workspace/scratch/809689da2702/validation/r63-final/4242-partyexp-cases.txt
```
Integrated final exit 0. Printed summary:

```text
applyPartyExp: pass 20000  fail 0 (experience given to 31442 party members)
```

### 4242-upkeep

```sh
BUILD=/workspace/scratch/809689da2702/validation/r63-final/4242 sh run-core/tools/difftest.sh /workspace/scratch/809689da2702/upstream-r63-b 20000 4242 upkeep
```
Final exit 0. Printed summary:

```text
trySpreadPokerus: pass 20000  fail 0 (2212 Pokemon caught it)
Coin Case interest: pass 20000  fail 0
```

Paired unchanged-main C replay:

```sh
/workspace/scratch/809689da2702/task63-baseline/run-core/build/upkeeptest /workspace/scratch/809689da2702/validation/r63-final/4242/upkeep-cases.txt
```
Before replay exit 0. Printed summary:

```text
trySpreadPokerus: pass 20000  fail 0 (2212 Pokemon caught it)
Coin Case interest: pass 20000  fail 0
```

Integrated final candidate replay:

```sh
/workspace/scratch/809689da2702/task63-resume/run-core/build/upkeeptest /workspace/scratch/809689da2702/validation/r63-final/4242-upkeep-cases.txt
```
Integrated final exit 0. Printed summary:

```text
trySpreadPokerus: pass 20000  fail 0 (2212 Pokemon caught it)
Coin Case interest: pass 20000  fail 0
```

### 4242-trainerspecies

```sh
BUILD=/workspace/scratch/809689da2702/validation/r63-final/4242 sh run-core/tools/difftest.sh /workspace/scratch/809689da2702/upstream-r63-b 20000 4242 trainerspecies
```
Final exit 0. Printed summary:

```text
cases 20000 (12262 from species pools; upstream threw: 0)
  genNewPartyMemberSpecies (+ RNG)     pass  20000  fail 0  not covered 0
```

Paired unchanged-main C replay:

```sh
/workspace/scratch/809689da2702/task63-baseline/run-core/build/trainerspeciestest /workspace/scratch/809689da2702/validation/r63-final/4242/trainerspecies-cases.txt
```
Before replay exit 0. Printed summary:

```text
cases 20000 (12262 from species pools; upstream threw: 0)
  genNewPartyMemberSpecies (+ RNG)     pass  20000  fail 0  not covered 0
```

Integrated final candidate replay:

```sh
/workspace/scratch/809689da2702/task63-resume/run-core/build/trainerspeciestest /workspace/scratch/809689da2702/validation/r63-final/4242-trainerspecies-cases.txt
```
Integrated final exit 0. Printed summary:

```text
cases 20000 (12262 from species pools; upstream threw: 0)
  genNewPartyMemberSpecies (+ RNG)     pass  20000  fail 0  not covered 0
```

### 4242-luck

```sh
BUILD=/workspace/scratch/809689da2702/validation/r63-final/4242 sh run-core/tools/difftest.sh /workspace/scratch/809689da2702/upstream-r63-b 20000 4242 luck
```
Final exit 0. Printed summary:

```text
cases 20000 (daily 4019, at the cap of 14: 1199)
  getPartyLuckValue (+ RNG)            pass  20000  fail 0  not covered 0
```

Paired unchanged-main C replay:

```sh
/workspace/scratch/809689da2702/task63-baseline/run-core/build/lucktest /workspace/scratch/809689da2702/validation/r63-final/4242/luck-cases.txt
```
Before replay exit 0. Printed summary:

```text
cases 20000 (daily 4019, at the cap of 14: 1199)
  getPartyLuckValue (+ RNG)            pass  20000  fail 0  not covered 0
```

Integrated final candidate replay:

```sh
/workspace/scratch/809689da2702/task63-resume/run-core/build/lucktest /workspace/scratch/809689da2702/validation/r63-final/4242-luck-cases.txt
```
Integrated final exit 0. Printed summary:

```text
cases 20000 (daily 4019, at the cap of 14: 1199)
  getPartyLuckValue (+ RNG)            pass  20000  fail 0  not covered 0
```

### 4242-daily

```sh
BUILD=/workspace/scratch/809689da2702/validation/r63-final/4242 sh run-core/tools/difftest.sh /workspace/scratch/809689da2702/upstream-r63-b 20000 4242 daily
```
Final exit 0. Printed summary:

```text
dailytest summary: 5 daily runs (250 waves), 775 checks passed, 0 mismatches
```

Paired unchanged-main C replay:

```sh
/workspace/scratch/809689da2702/task63-baseline/run-core/build/dailytest /workspace/scratch/809689da2702/validation/r63-final/4242/daily-cases.txt
```
Before replay exit 1. Printed summary:

```text
line 3: FAIL starter 1 got sp 8128 ab 1 fem 0 f 0 sh 0 v 0 n 4 exp sp 8128 ab 1 fem 0 f 0 sh 0 v 0 n 22
line 4: FAIL starter 2 got sp 519 ab 1 fem 0 f 0 sh 0 v 0 n 8 exp sp 351 ab 0 fem 1 f 0 sh 0 v 0 n 8
dailytest summary: 5 daily runs (250 waves), 773 checks passed, 2 mismatches
```

Integrated final candidate replay:

```sh
/workspace/scratch/809689da2702/task63-resume/run-core/build/dailytest /workspace/scratch/809689da2702/validation/r63-final/4242-daily-cases.txt
```
Integrated final exit 0. Printed summary:

```text
dailytest summary: 5 daily runs (250 waves), 775 checks passed, 0 mismatches
```

### 4242-battlegen

```sh
BUILD=/workspace/scratch/809689da2702/validation/r63-final/4242 sh run-core/tools/difftest.sh /workspace/scratch/809689da2702/upstream-r63-b 10000 4242 battlegen
```
Final exit 0. Printed summary:

```text
newBattle: pass 9685  fail 0  not covered 315 (wild 8492, trainer 682, mystery encounter 511; doubles 2216)
```

Paired unchanged-main C replay:

```sh
/workspace/scratch/809689da2702/task63-baseline/run-core/build/battlegentest /workspace/scratch/809689da2702/validation/r63-final/4242/battlegen-cases.txt
```
Before replay exit 0. Printed summary:

```text
newBattle: pass 9685  fail 0  not covered 315 (wild 8492, trainer 682, mystery encounter 511; doubles 2216)
```

Integrated final candidate replay:

```sh
/workspace/scratch/809689da2702/task63-resume/run-core/build/battlegentest /workspace/scratch/809689da2702/validation/r63-final/4242-battlegen-cases.txt
```
Integrated final exit 0. Printed summary:

```text
newBattle: pass 9685  fail 0  not covered 315 (wild 8492, trainer 682, mystery encounter 511; doubles 2216)
```

### 4242-pokemongen

```sh
BUILD=/workspace/scratch/809689da2702/validation/r63-final/4242 sh run-core/tools/difftest.sh /workspace/scratch/809689da2702/upstream-r63-b 8000 4242 pokemongen
```
Final exit 0. Printed summary:

```text
cases 8000 (addEnemyPokemon threw upstream: 0); checked: 618 shiny, 188 rare/epic variants, 2570 bosses, 2959 trainer Pokemon, 1407 fusions
  constructor up to the moveset (+ RNG)            pass   8000  fail 0  not covered 0
  constructor after it, addEnemyPokemon (+ RNG)    pass   8000  fail 0  not covered 0
```

Paired unchanged-main C replay:

```sh
/workspace/scratch/809689da2702/task63-baseline/run-core/build/pokemongentest /workspace/scratch/809689da2702/validation/r63-final/4242-pokemongen-cases.txt
```
Before replay exit 0. Printed summary:

```text
cases 8000 (addEnemyPokemon threw upstream: 0); checked: 618 shiny, 188 rare/epic variants, 2570 bosses, 2959 trainer Pokemon, 1407 fusions
  constructor up to the moveset (+ RNG)            pass   8000  fail 0  not covered 0
  constructor after it, addEnemyPokemon (+ RNG)    pass   8000  fail 0  not covered 0
```

Integrated final candidate replay:

```sh
/workspace/scratch/809689da2702/task63-resume/run-core/build/pokemongentest /workspace/scratch/809689da2702/validation/r63-final/4242-pokemongen-cases.txt
```
Integrated final exit 0. Printed summary:

```text
cases 8000 (addEnemyPokemon threw upstream: 0); checked: 618 shiny, 188 rare/epic variants, 2570 bosses, 2959 trainer Pokemon, 1407 fusions
  constructor up to the moveset (+ RNG)            pass   8000  fail 0  not covered 0
  constructor after it, addEnemyPokemon (+ RNG)    pass   8000  fail 0  not covered 0
```

### 4242-trainerparty

```sh
BUILD=/workspace/scratch/809689da2702/validation/r63-final/4242 sh run-core/tools/difftest.sh /workspace/scratch/809689da2702/upstream-r63-b 3000 4242 trainerparty
```
Final exit 0. Printed summary:

```text
cases 3000 (1591 double; whole parties matched: 2536; members upstream threw on: 102; fused members: 1993)
  genPartyMember (species, rolls)      pass   9655  fail 0  not covered 1748
```

Paired unchanged-main C replay:

```sh
/workspace/scratch/809689da2702/task63-baseline/run-core/build/trainerpartytest /workspace/scratch/809689da2702/validation/r63-final/4242-trainerparty-cases.txt
```
Before replay exit 0. Printed summary:

```text
cases 3000 (1591 double; whole parties matched: 2536; members upstream threw on: 102; fused members: 1993)
  genPartyMember (species, rolls)      pass   9655  fail 0  not covered 1748
```

Integrated final candidate replay:

```sh
/workspace/scratch/809689da2702/task63-resume/run-core/build/trainerpartytest /workspace/scratch/809689da2702/validation/r63-final/4242-trainerparty-cases.txt
```
Integrated final exit 0. Printed summary:

```text
cases 3000 (1591 double; whole parties matched: 2536; members upstream threw on: 102; fused members: 1993)
  genPartyMember (species, rolls)      pass   9655  fail 0  not covered 1748
```

### 31337-wave

```sh
BUILD=/workspace/scratch/809689da2702/validation/r63-final/31337 sh run-core/tools/difftest.sh /workspace/scratch/809689da2702/upstream-r63-d 20000 31337 wave
```
Final exit 0. Printed summary:

```text
cases 20000
  game-mode rules (15 per case)        pass 300000  fail 0  not covered 0
  getMaxExpLevel                       pass  20000  fail 0  not covered 0
  getLevelForWave (+ RNG state)        pass  20000  fail 0  not covered 0
  isWaveTrainer (+ RNG state)          pass  20000  fail 0  not covered 0
  resetSeed + Battle seeding chain     pass  20000  fail 0  not covered 0
  getEncounterBossSegments             pass  20000  fail 0  not covered 0
  getDoubleBattleChance                pass  20000  fail 0  not covered 0
```

Paired unchanged-main C replay:

```sh
/workspace/scratch/809689da2702/task63-baseline/run-core/build/wavetest /workspace/scratch/809689da2702/validation/r63-final/31337-wave-cases.txt
```
Before replay exit 0. Printed summary:

```text
cases 20000
  game-mode rules (15 per case)        pass 300000  fail 0  not covered 0
  getMaxExpLevel                       pass  20000  fail 0  not covered 0
  getLevelForWave (+ RNG state)        pass  20000  fail 0  not covered 0
  isWaveTrainer (+ RNG state)          pass  20000  fail 0  not covered 0
  resetSeed + Battle seeding chain     pass  20000  fail 0  not covered 0
  getEncounterBossSegments             pass  20000  fail 0  not covered 0
  getDoubleBattleChance                pass  20000  fail 0  not covered 0
```

Integrated final candidate replay:

```sh
/workspace/scratch/809689da2702/task63-resume/run-core/build/wavetest /workspace/scratch/809689da2702/validation/r63-final/31337-wave-cases.txt
```
Integrated final exit 0. Printed summary:

```text
cases 20000
  game-mode rules (15 per case)        pass 300000  fail 0  not covered 0
  getMaxExpLevel                       pass  20000  fail 0  not covered 0
  getLevelForWave (+ RNG state)        pass  20000  fail 0  not covered 0
  isWaveTrainer (+ RNG state)          pass  20000  fail 0  not covered 0
  resetSeed + Battle seeding chain     pass  20000  fail 0  not covered 0
  getEncounterBossSegments             pass  20000  fail 0  not covered 0
  getDoubleBattleChance                pass  20000  fail 0  not covered 0
```

### 31337-shop

```sh
BUILD=/workspace/scratch/809689da2702/validation/r63-final/31337 sh run-core/tools/difftest.sh /workspace/scratch/809689da2702/upstream-r63-d 20000 31337 shop
```
Final exit 0. Printed summary:

```text
cases 20000
  getWaveMoneyAmount                       pass  20000  fail 0  not covered 0
  money reward (Amulet Coin, Happy Hour)   pass  20000  fail 0  not covered 0
  getShopStatus                            pass  20000  fail 0  not covered 0
  shop options as shown (Black Sludge)     pass  20000  fail 0  not covered 0
  shop options as charged                  pass  20000  fail 0  not covered 0
  getRerollCost                            pass  20000  fail 0  not covered 0
```

Paired unchanged-main C replay:

```sh
/workspace/scratch/809689da2702/task63-baseline/run-core/build/shoptest /workspace/scratch/809689da2702/validation/r63-final/31337-shop-cases.txt
```
Before replay exit 0. Printed summary:

```text
cases 20000
  getWaveMoneyAmount                       pass  20000  fail 0  not covered 0
  money reward (Amulet Coin, Happy Hour)   pass  20000  fail 0  not covered 0
  getShopStatus                            pass  20000  fail 0  not covered 0
  shop options as shown (Black Sludge)     pass  20000  fail 0  not covered 0
  shop options as charged                  pass  20000  fail 0  not covered 0
  getRerollCost                            pass  20000  fail 0  not covered 0
```

Integrated final candidate replay:

```sh
/workspace/scratch/809689da2702/task63-resume/run-core/build/shoptest /workspace/scratch/809689da2702/validation/r63-final/31337-shop-cases.txt
```
Integrated final exit 0. Printed summary:

```text
cases 20000
  getWaveMoneyAmount                       pass  20000  fail 0  not covered 0
  money reward (Amulet Coin, Happy Hour)   pass  20000  fail 0  not covered 0
  getShopStatus                            pass  20000  fail 0  not covered 0
  shop options as shown (Black Sludge)     pass  20000  fail 0  not covered 0
  shop options as charged                  pass  20000  fail 0  not covered 0
  getRerollCost                            pass  20000  fail 0  not covered 0
```

### 31337-biome

```sh
BUILD=/workspace/scratch/809689da2702/validation/r63-final/31337 sh run-core/tools/difftest.sh /workspace/scratch/809689da2702/upstream-r63-d 20000 31337 biome
```
Final exit 0. Printed summary:

```text
cases 20000
  initBiomeDepths                      pass  20000  fail 0  not covered 0
  generateRandomBiome (+ RNG state)    pass  20000  fail 0  not covered 0
  SelectBiomePhase (+ RNG state)       pass  20000  fail 0  not covered 0
```

Paired unchanged-main C replay:

```sh
/workspace/scratch/809689da2702/task63-baseline/run-core/build/biometest /workspace/scratch/809689da2702/validation/r63-final/31337-biome-cases.txt
```
Before replay exit 0. Printed summary:

```text
cases 20000
  initBiomeDepths                      pass  20000  fail 0  not covered 0
  generateRandomBiome (+ RNG state)    pass  20000  fail 0  not covered 0
  SelectBiomePhase (+ RNG state)       pass  20000  fail 0  not covered 0
```

Integrated final candidate replay:

```sh
/workspace/scratch/809689da2702/task63-resume/run-core/build/biometest /workspace/scratch/809689da2702/validation/r63-final/31337-biome-cases.txt
```
Integrated final exit 0. Printed summary:

```text
cases 20000
  initBiomeDepths                      pass  20000  fail 0  not covered 0
  generateRandomBiome (+ RNG state)    pass  20000  fail 0  not covered 0
  SelectBiomePhase (+ RNG state)       pass  20000  fail 0  not covered 0
```

### 31337-encounter

```sh
BUILD=/workspace/scratch/809689da2702/validation/r63-final/31337 sh run-core/tools/difftest.sh /workspace/scratch/809689da2702/upstream-r63-d 20000 31337 encounter
```
Final exit 0. Printed summary:

```text
cases 20000 (daily final-boss overrides: 17)
  wave-cycle and gym offsets           pass  20000  fail 0  not covered 0
  getTimeOfDay                         pass  20000  fail 0  not covered 0
  randomSpecies to the evolution step (+ RNG) pass  20000  fail 0  not covered 0
  randomSpecies with evolution (+ RNG) pass  20000  fail 0  not covered 0
  determineEnemySpecies (+ RNG)        pass  20000  fail 0  not covered 0
  randomTrainerType (+ RNG state)      pass  20000  fail 0  not covered 0
```

Paired unchanged-main C replay:

```sh
/workspace/scratch/809689da2702/task63-baseline/run-core/build/encountertest /workspace/scratch/809689da2702/validation/r63-final/31337-encounter-cases.txt
```
Before replay exit 0. Printed summary:

```text
cases 20000 (daily final-boss overrides: 17)
  wave-cycle and gym offsets           pass  20000  fail 0  not covered 0
  getTimeOfDay                         pass  20000  fail 0  not covered 0
  randomSpecies to the evolution step (+ RNG) pass  20000  fail 0  not covered 0
  randomSpecies with evolution (+ RNG) pass  20000  fail 0  not covered 0
  determineEnemySpecies (+ RNG)        pass  20000  fail 0  not covered 0
  randomTrainerType (+ RNG state)      pass  20000  fail 0  not covered 0
```

Integrated final candidate replay:

```sh
/workspace/scratch/809689da2702/task63-resume/run-core/build/encountertest /workspace/scratch/809689da2702/validation/r63-final/31337-encounter-cases.txt
```
Integrated final exit 0. Printed summary:

```text
cases 20000 (daily final-boss overrides: 17)
  wave-cycle and gym offsets           pass  20000  fail 0  not covered 0
  getTimeOfDay                         pass  20000  fail 0  not covered 0
  randomSpecies to the evolution step (+ RNG) pass  20000  fail 0  not covered 0
  randomSpecies with evolution (+ RNG) pass  20000  fail 0  not covered 0
  determineEnemySpecies (+ RNG)        pass  20000  fail 0  not covered 0
  randomTrainerType (+ RNG state)      pass  20000  fail 0  not covered 0
```

### 31337-newbattle

```sh
BUILD=/workspace/scratch/809689da2702/validation/r63-final/31337 sh run-core/tools/difftest.sh /workspace/scratch/809689da2702/upstream-r63-d 20000 31337 newbattle
```
Final exit 0. Printed summary:

```text
cases 20000 (trainers that couldn't be built upstream: 0)
  isMysteryEncounterValidForWave       pass  20000  fail 0  not covered 0
  isWaveMysteryEncounter               pass  20000  fail 0  not covered 0
  Trainer.getPartyLevels               pass  20000  fail 0  not covered 0
```

Paired unchanged-main C replay:

```sh
/workspace/scratch/809689da2702/task63-baseline/run-core/build/newbattletest /workspace/scratch/809689da2702/validation/r63-final/31337-newbattle-cases.txt
```
Before replay exit 0. Printed summary:

```text
cases 20000 (trainers that couldn't be built upstream: 0)
  isMysteryEncounterValidForWave       pass  20000  fail 0  not covered 0
  isWaveMysteryEncounter               pass  20000  fail 0  not covered 0
  Trainer.getPartyLevels               pass  20000  fail 0  not covered 0
```

Integrated final candidate replay:

```sh
/workspace/scratch/809689da2702/task63-resume/run-core/build/newbattletest /workspace/scratch/809689da2702/validation/r63-final/31337-newbattle-cases.txt
```
Integrated final exit 0. Printed summary:

```text
cases 20000 (trainers that couldn't be built upstream: 0)
  isMysteryEncounterValidForWave       pass  20000  fail 0  not covered 0
  isWaveMysteryEncounter               pass  20000  fail 0  not covered 0
  Trainer.getPartyLevels               pass  20000  fail 0  not covered 0
```

### 31337-exp

```sh
BUILD=/workspace/scratch/809689da2702/validation/r63-final/31337 sh run-core/tools/difftest.sh /workspace/scratch/809689da2702/upstream-r63-d 20000 31337 exp
```
Final exit 0. Printed summary:

```text
getLevelTotalExp + getLevelRelExp: pass 138006  fail 0
```

Paired unchanged-main C replay:

```sh
/workspace/scratch/809689da2702/task63-baseline/run-core/build/exptest /workspace/scratch/809689da2702/validation/r63-final/31337-exp-cases.txt
```
Before replay exit 0. Printed summary:

```text
getLevelTotalExp + getLevelRelExp: pass 138006  fail 0
```

Integrated final candidate replay:

```sh
/workspace/scratch/809689da2702/task63-resume/run-core/build/exptest /workspace/scratch/809689da2702/validation/r63-final/31337-exp-cases.txt
```
Integrated final exit 0. Printed summary:

```text
getLevelTotalExp + getLevelRelExp: pass 138006  fail 0
```

### 31337-partyexp

```sh
BUILD=/workspace/scratch/809689da2702/validation/r63-final/31337 sh run-core/tools/difftest.sh /workspace/scratch/809689da2702/upstream-r63-d 20000 31337 partyexp
```
Final exit 0. Printed summary:

```text
applyPartyExp: pass 20000  fail 0 (experience given to 31421 party members)
```

Paired unchanged-main C replay:

```sh
/workspace/scratch/809689da2702/task63-baseline/run-core/build/partyexptest /workspace/scratch/809689da2702/validation/r63-final/31337-partyexp-cases.txt
```
Before replay exit 0. Printed summary:

```text
applyPartyExp: pass 20000  fail 0 (experience given to 31421 party members)
```

Integrated final candidate replay:

```sh
/workspace/scratch/809689da2702/task63-resume/run-core/build/partyexptest /workspace/scratch/809689da2702/validation/r63-final/31337-partyexp-cases.txt
```
Integrated final exit 0. Printed summary:

```text
applyPartyExp: pass 20000  fail 0 (experience given to 31421 party members)
```

### 31337-upkeep

```sh
BUILD=/workspace/scratch/809689da2702/validation/r63-final/31337 sh run-core/tools/difftest.sh /workspace/scratch/809689da2702/upstream-r63-d 20000 31337 upkeep
```
Final exit 0. Printed summary:

```text
trySpreadPokerus: pass 20000  fail 0 (2230 Pokemon caught it)
Coin Case interest: pass 20000  fail 0
```

Paired unchanged-main C replay:

```sh
/workspace/scratch/809689da2702/task63-baseline/run-core/build/upkeeptest /workspace/scratch/809689da2702/validation/r63-final/31337-upkeep-cases.txt
```
Before replay exit 0. Printed summary:

```text
trySpreadPokerus: pass 20000  fail 0 (2230 Pokemon caught it)
Coin Case interest: pass 20000  fail 0
```

Integrated final candidate replay:

```sh
/workspace/scratch/809689da2702/task63-resume/run-core/build/upkeeptest /workspace/scratch/809689da2702/validation/r63-final/31337-upkeep-cases.txt
```
Integrated final exit 0. Printed summary:

```text
trySpreadPokerus: pass 20000  fail 0 (2230 Pokemon caught it)
Coin Case interest: pass 20000  fail 0
```

### 31337-trainerspecies

```sh
BUILD=/workspace/scratch/809689da2702/validation/r63-final/31337 sh run-core/tools/difftest.sh /workspace/scratch/809689da2702/upstream-r63-d 20000 31337 trainerspecies
```
Final exit 0. Printed summary:

```text
cases 20000 (12138 from species pools; upstream threw: 0)
  genNewPartyMemberSpecies (+ RNG)     pass  20000  fail 0  not covered 0
```

Paired unchanged-main C replay:

```sh
/workspace/scratch/809689da2702/task63-baseline/run-core/build/trainerspeciestest /workspace/scratch/809689da2702/validation/r63-final/31337-trainerspecies-cases.txt
```
Before replay exit 0. Printed summary:

```text
cases 20000 (12138 from species pools; upstream threw: 0)
  genNewPartyMemberSpecies (+ RNG)     pass  20000  fail 0  not covered 0
```

Integrated final candidate replay:

```sh
/workspace/scratch/809689da2702/task63-resume/run-core/build/trainerspeciestest /workspace/scratch/809689da2702/validation/r63-final/31337-trainerspecies-cases.txt
```
Integrated final exit 0. Printed summary:

```text
cases 20000 (12138 from species pools; upstream threw: 0)
  genNewPartyMemberSpecies (+ RNG)     pass  20000  fail 0  not covered 0
```

### 31337-luck

```sh
BUILD=/workspace/scratch/809689da2702/validation/r63-final/31337 sh run-core/tools/difftest.sh /workspace/scratch/809689da2702/upstream-r63-d 20000 31337 luck
```
Final exit 0. Printed summary:

```text
cases 20000 (daily 4097, at the cap of 14: 1248)
  getPartyLuckValue (+ RNG)            pass  20000  fail 0  not covered 0
```

Paired unchanged-main C replay:

```sh
/workspace/scratch/809689da2702/task63-baseline/run-core/build/lucktest /workspace/scratch/809689da2702/validation/r63-final/31337-luck-cases.txt
```
Before replay exit 0. Printed summary:

```text
cases 20000 (daily 4097, at the cap of 14: 1248)
  getPartyLuckValue (+ RNG)            pass  20000  fail 0  not covered 0
```

Integrated final candidate replay:

```sh
/workspace/scratch/809689da2702/task63-resume/run-core/build/lucktest /workspace/scratch/809689da2702/validation/r63-final/31337-luck-cases.txt
```
Integrated final exit 0. Printed summary:

```text
cases 20000 (daily 4097, at the cap of 14: 1248)
  getPartyLuckValue (+ RNG)            pass  20000  fail 0  not covered 0
```

### 31337-daily

```sh
BUILD=/workspace/scratch/809689da2702/validation/r63-final/31337 sh run-core/tools/difftest.sh /workspace/scratch/809689da2702/upstream-r63-d 20000 31337 daily
```
Final exit 0. Printed summary:

```text
dailytest summary: 5 daily runs (250 waves), 775 checks passed, 0 mismatches
```

Paired unchanged-main C replay:

```sh
/workspace/scratch/809689da2702/task63-baseline/run-core/build/dailytest /workspace/scratch/809689da2702/validation/r63-final/31337-daily-cases.txt
```
Before replay exit 1. Printed summary:

```text
line 3: FAIL starter 1 got sp 8128 ab 1 fem 0 f 0 sh 0 v 0 n 4 exp sp 8128 ab 1 fem 0 f 0 sh 0 v 0 n 22
line 4: FAIL starter 2 got sp 519 ab 1 fem 0 f 0 sh 0 v 0 n 8 exp sp 351 ab 0 fem 1 f 0 sh 0 v 0 n 8
dailytest summary: 5 daily runs (250 waves), 773 checks passed, 2 mismatches
```

Integrated final candidate replay:

```sh
/workspace/scratch/809689da2702/task63-resume/run-core/build/dailytest /workspace/scratch/809689da2702/validation/r63-final/31337-daily-cases.txt
```
Integrated final exit 0. Printed summary:

```text
dailytest summary: 5 daily runs (250 waves), 775 checks passed, 0 mismatches
```

