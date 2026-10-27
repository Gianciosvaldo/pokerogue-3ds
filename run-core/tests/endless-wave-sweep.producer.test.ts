/* Native GameMode/Battle/BattleScene observations at every wave 1..5850 in both
 * Endless modes. One seed/RNG input per wave; not full-run or exhaustive-seed
 * acceptance. Capture native battle RNG before GameManager replaces it. */
import { BattleScene } from "#app/battle-scene";
import { getGameMode } from "#app/game-mode";
import { speciesDataRegistry } from "#app/globals/global-species-data-registry";
import { modifierTypes } from "#data/data-lists";
import { BiomeId } from "#enums/biome-id";
import { GameModes } from "#enums/game-modes";
import { MoveId } from "#enums/move-id";
import { SpeciesId } from "#enums/species-id";
import { generateModifierType } from "#mystery-encounters/encounter-phase-utils";
import { GameManager } from "#test/framework/game-manager";
import * as fs from "node:fs";
import Phaser from "phaser";
import { it } from "vitest";

it("Endless wave sweep producer", async () => {
  const N = 11700;
  const OUT = process.env.DIFF_OUT ?? "/tmp/pr-wave-cases.txt";
  const nativeBattleRng=BattleScene.prototype.randBattleSeedInt;
  const game = new GameManager(new Phaser.Game({ type: Phaser.HEADLESS }));
  game.override.battleStyle("single").startingWave(5).enemySpecies(SpeciesId.MAGIKARP).enemyMoveset(MoveId.SPLASH);
  await game.classicMode.startBattle(SpeciesId.BULBASAUR);
  BattleScene.prototype.randBattleSeedInt=nativeBattleRng;
  const scene = game.scene as any;
  const battle = scene.currentBattle as any;

  let seed = 777;
  const rnd = (n: number) => {
    seed = (Math.imul(seed, 1103515245) + 12345) >>> 0;
    return (seed >>> 8) % n;
  };
  const chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
  const seedStr = () => Array.from({ length: 24 }, () => chars[rnd(chars.length)]).join("");
  const state = () => Phaser.Math.RND.state().split(",").slice(1).join(" ");
  const biomes = Object.values(BiomeId).filter(v => typeof v === "number") as number[];
  const speciesIds = Object.keys(speciesDataRegistry.data).map(Number);
  const lures = ["LURE", "SUPER_LURE", "MAX_LURE"];
  // a thrown call (the daily mode's config is absent in a test run) prints as NaN
  const ask = (f: () => any) => {
    try {
      const v = f();
      return typeof v === "boolean" ? (v ? 1 : 0) : v;
    } catch {
      return Number.NaN;
    }
  };

  const lines: string[] = [];
  for (let i = 0; i < N; i++) {
    const mode = [GameModes.ENDLESS, GameModes.SPLICED_ENDLESS][i%2];
    const gm = getGameMode(mode);
    scene.gameMode = gm;
    battle.gameMode = gm;
    // Every source-supported wave in both Endless modes, one seeded observation each.
    const wave = 1+Math.floor(i/2);
    scene.seed = seedStr();
    scene.offsetGym = rnd(2) === 0;
    const biome = biomes[rnd(biomes.length)];
    scene.arena.biomeId = biome;
    const trainerChance = scene.arena.trainerChance;

    const det = [
      ask(() => gm.getWaveForDifficulty(wave)), ask(() => gm.getWaveForDifficulty(wave, true)),
      ask(() => gm.isWaveFinal(wave)), ask(() => gm.isBoss(wave)), ask(() => gm.isBattleClassicFinalBoss(wave)),
      ask(() => gm.isEndlessBoss(wave)), ask(() => gm.isEndlessMinorBoss(wave)), ask(() => gm.isEndlessMajorBoss(wave)),
      ask(() => gm.isTrainerBoss(wave, biome, scene.offsetGym)), ask(() => gm.isFixedBattle(wave)),
      ask(() => gm.getClearScoreBonus()), ask(() => gm.getEnemyModifierChance(true)),
      ask(() => gm.getEnemyModifierChance(false)), ask(() => gm.getStartingLevel()), ask(() => gm.getStartingMoney()),
    ];

    // getMaxExpLevel reads currentBattle.waveIndex (0 stands for "no battle yet": `|| 1`)
    const capWave = rnd(8) === 0 ? 0 : wave;
    battle.waveIndex = capWave;
    const cap = [ask(() => scene.getMaxExpLevel()), ask(() => scene.getMaxExpLevel(true))];

    // getLevelForWave on a freshly sown RND
    battle.waveIndex = wave;
    Phaser.Math.RND.sow([seedStr()]);
    const lvBefore = state();
    const level = ask(() => battle.getLevelForWave());
    const lvAfter = state();

    // isWaveTrainer on a freshly sown RND (earlier waves roll on the run seed)
    Phaser.Math.RND.sow([seedStr()]);
    const trBefore = state();
    const trainer = ask(() => gm.isWaveTrainer(wave));
    const trAfter = state();

    // newBattle's seeding: resetSeed(wave), then the Battle is built inside
    // executeWithSeedOffset(wave << 3, waveSeed)
    scene.resetSeed(wave);
    let chainLevel: any = Number.NaN;
    scene.executeWithSeedOffset(() => (chainLevel = ask(() => battle.getLevelForWave())), wave << 3, scene.waveSeed);
    const chainTrainer = ask(() => gm.isWaveTrainer(wave));

    // getEncounterBossSegments
    const species = rnd(3) === 0 ? -1 : speciesIds[rnd(speciesIds.length)];
    const bossLevel = 1 + rnd(200);
    const forceBoss = rnd(8) === 0;
    const segments = ask(() =>
      scene.getEncounterBossSegments(wave, bossLevel, species < 0 ? undefined : speciesDataRegistry.getSpecies(species), forceBoss),
    );

    // getDoubleBattleChance with 0 to 3 Lure-type modifiers
    scene.modifiers.length = 0;
    const nLures = rnd(3) === 0 ? 1 + rnd(3) : 0;
    for (let k = 0; k < nLures; k++) {
      scene.modifiers.push((generateModifierType((modifierTypes as any)[lures[rnd(lures.length)]]) as any).newModifier());
    }
    const chance = ask(() => scene.getDoubleBattleChance(wave));

    lines.push(
      [
        "W", mode, wave, scene.offsetGym ? 1 : 0, biome, trainerChance, scene.seed, ...det,
        capWave, ...cap, lvBefore, level, lvAfter, trBefore, trainer, trAfter, chainLevel, chainTrainer,
        species, bossLevel, forceBoss ? 1 : 0, segments, nLures, chance,
      ].join(" "),
    );
  }
  scene.modifiers.length = 0;
  fs.writeFileSync(OUT, lines.join("\n") + "\n");
}, 900_000);
