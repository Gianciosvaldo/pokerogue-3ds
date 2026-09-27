/*
 * Upstream half of run-core's encounter test (tools/difftest.sh copies it into the upstream checkout).
 * Each case sets a random game mode, run seed, biome, wave, wave-cycle offset and gym offset on the
 * real scene and arena, then asks upstream for the generated wave-cycle offset and gym offset, the
 * time of day, a wild species (arena.randomSpecies, on a freshly sown RND) and a trainer type
 * (arena.randomTrainerType, likewise). The evolution step at the end of randomSpecies
 * (species.getWildSpeciesForLevel) isn't ported yet, so it is intercepted here: the case records the
 * species and the isBoss it receives, and then the whole randomSpecies runs again on the same RND
 * with the real evolution step. Each case also asks determineEnemySpecies on its own for a random
 * species, level and kind. build/encountertest replays the cases.
 */
import { determineEnemySpecies } from "#app/ai/ai-species-gen";
import { getGameMode } from "#app/game-mode";
import { speciesDataRegistry } from "#app/globals/global-species-data-registry";
import { allBiomes } from "#data/data-lists";
import { PokemonSpecies } from "#data/pokemon-species";
import { BiomeId } from "#enums/biome-id";
import { GameModes } from "#enums/game-modes";
import { MoveId } from "#enums/move-id";
import { SpeciesId } from "#enums/species-id";
import { GameManager } from "#test/framework/game-manager";
import * as fs from "node:fs";
import Phaser from "phaser";
import { it } from "vitest";

it("encounter producer", async () => {
  const N = Number(process.env.DIFF_N ?? 20000);
  const OUT = process.env.DIFF_OUT ?? "/tmp/pr-encounter-cases.txt";
  const game = new GameManager(new Phaser.Game({ type: Phaser.HEADLESS }));
  game.override.battleStyle("single").startingWave(5).enemySpecies(SpeciesId.MAGIKARP).enemyMoveset(MoveId.SPLASH);
  await game.classicMode.startBattle(SpeciesId.BULBASAUR);
  const scene = game.scene as any;
  const battle = scene.currentBattle as any;
  const arena = scene.arena as any;

  let seed = Number(process.env.DIFF_SEED ?? 12345);
  const rnd = (n: number) => {
    seed = (Math.imul(seed, 1103515245) + 12345) >>> 0;
    return (seed >>> 8) % n;
  };
  const chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
  const seedStr = () => Array.from({ length: 24 }, () => chars[rnd(chars.length)]).join("");
  const state = () => Phaser.Math.RND.state().split(",").slice(1).join(" ");
  const modes = [GameModes.CLASSIC, GameModes.ENDLESS, GameModes.SPLICED_ENDLESS, GameModes.DAILY, GameModes.CHALLENGE];
  const biomes = Object.values(BiomeId) as number[];

  // The unported evolution step: record what it receives and hand the species back unchanged
  const realWild = PokemonSpecies.prototype.getWildSpeciesForLevel;
  let evo: [number, number] = [-1, -1];
  const interceptWild = function (this: any, _level: number, _allow: boolean, isBoss: boolean) {
    evo = [this.speciesId, isBoss ? 1 : 0];
    return this.speciesId;
  };
  (PokemonSpecies.prototype as any).getWildSpeciesForLevel = interceptWild;
  const speciesIds = Object.keys(speciesDataRegistry.data).map(Number);

  const lines: string[] = [];
  try {
    for (let i = 0; i < N; i++) {
      const gm = getGameMode(modes[rnd(modes.length)]);
      scene.gameMode = gm;
      battle.gameMode = gm;
      scene.seed = seedStr();
      const genCycle = scene.getGeneratedWaveCycleOffset();
      const genGym = scene.getGeneratedOffsetGym() ? 1 : 0;

      const biome = biomes[rnd(biomes.length)];
      arena.biomeId = biome;
      arena.trainerPool = allBiomes.get(biome).trainerPool;
      arena.lastTimeOfDay = undefined;
      const wave = rnd(4) === 0 ? 1 + rnd(3000) : 1 + rnd(210);
      battle.waveIndex = wave;
      scene.waveCycleOffset = rnd(8) * 5;
      scene.offsetGym = rnd(2) === 0;
      const tod = arena.getTimeOfDay();
      arena.updatePoolsForTimeOfDay();

      const level = 1 + rnd(120);
      const luck = rnd(3) === 0 ? rnd(15) : 0;
      const isBoss = [undefined, undefined, true, false][rnd(4)];
      const spSeed = seedStr();
      Phaser.Math.RND.sow([spSeed]);
      const spBefore = state();
      evo = [-1, -1];
      let species: any;
      try {
        species = arena.randomSpecies(wave, level, 0, luck, isBoss).speciesId;
      } catch {
        species = "NaN";
      }
      const spAfter = state();
      // The whole of randomSpecies, evolution step included, on the same RND
      Phaser.Math.RND.sow([spSeed]);
      (PokemonSpecies.prototype as any).getWildSpeciesForLevel = realWild;
      let full: any;
      try {
        full = arena.randomSpecies(wave, level, 0, luck, isBoss).speciesId;
      } catch {
        full = "NaN";
      }
      (PokemonSpecies.prototype as any).getWildSpeciesForLevel = interceptWild;
      const fullAfter = state();

      // determineEnemySpecies on its own: any species, level, kind, evolving or not, forced
      // prevolutions or not, and now and then the wave-20 classic trainer battle that blocks evolving
      const anySpecies = speciesIds[rnd(speciesIds.length)];
      const detLevel = 1 + rnd(100);
      const detAllow = rnd(4) !== 0;
      const detKind = rnd(3);
      const detForce = rnd(4) !== 0;
      const block = rnd(8) === 0;
      battle.waveIndex = block ? 20 : wave;
      battle.trainer = block ? {} : null;
      Phaser.Math.RND.sow([seedStr()]);
      const detBefore = state();
      const det = determineEnemySpecies(
        speciesDataRegistry.getSpecies(anySpecies), detLevel, detAllow, false, undefined, detKind as any, detForce,
      );
      const detAfter = state();
      battle.trainer = null;
      const blocked = block && gm.isClassic ? 1 : 0;

      const trainerBoss = rnd(4) === 0;
      Phaser.Math.RND.sow([seedStr()]);
      const trBefore = state();
      const trainer = arena.randomTrainerType(wave, trainerBoss);
      const trAfter = state();

      lines.push(
        [
          "E", gm.modeId, scene.seed, genCycle, genGym, biome, wave, scene.waveCycleOffset, scene.offsetGym ? 1 : 0, tod,
          level, luck, isBoss === undefined ? -1 : isBoss ? 1 : 0, spBefore, species, evo[0], evo[1], spAfter, full,
          fullAfter, anySpecies, detLevel, detAllow ? 1 : 0, detKind, detForce ? 1 : 0, blocked, detBefore, det, detAfter,
          trainerBoss ? 1 : 0, trBefore, trainer, trAfter,
        ].join(" "),
      );
    }
  } finally {
    (PokemonSpecies.prototype as any).getWildSpeciesForLevel = realWild;
  }
  fs.writeFileSync(OUT, lines.join("\n") + "\n");
}, 900_000);
