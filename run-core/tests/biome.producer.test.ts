/*
 * Upstream half of run-core's biome test (tools/difftest.sh copies it into the upstream checkout).
 * Each case rebuilds biomeDepths with initBiomeDepths() on a freshly sown RND, then runs a real
 * SelectBiomePhase for a random game mode, run seed, current biome and wave, with or without a
 * Map (its menu's choices are read back through the options' own handlers). It also asks
 * generateRandomBiome for a random wave on a freshly sown RND. build/biometest replays them.
 */
import { getGameMode } from "#app/game-mode";
import { biomeDepths, modifierTypes } from "#data/data-lists";
import { BiomeId } from "#enums/biome-id";
import { GameModes } from "#enums/game-modes";
import { MoveId } from "#enums/move-id";
import { SpeciesId } from "#enums/species-id";
import { initBiomeDepths } from "#init/init-biome-depths";
import { generateModifierType } from "#mystery-encounters/encounter-phase-utils";
import { SelectBiomePhase } from "#phases/select-biome-phase";
import { GameManager } from "#test/framework/game-manager";
import * as fs from "node:fs";
import Phaser from "phaser";
import { it } from "vitest";

it("biome producer", async () => {
  const N = Number(process.env.DIFF_N ?? 20000);
  const OUT = process.env.DIFF_OUT ?? "/tmp/pr-biome-cases.txt";
  const game = new GameManager(new Phaser.Game({ type: Phaser.HEADLESS }));
  game.override.battleStyle("single").startingWave(5).enemySpecies(SpeciesId.MAGIKARP).enemyMoveset(MoveId.SPLASH);
  await game.classicMode.startBattle(SpeciesId.BULBASAUR);
  const scene = game.scene as any;
  const battle = scene.currentBattle as any;

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
  const realSetMode = scene.ui.setMode;

  const lines: string[] = [];
  for (let i = 0; i < N; i++) {
    // initBiomeDepths on a freshly sown RND
    Phaser.Math.RND.sow([seedStr()]);
    const initState = state();
    for (const k of Object.keys(biomeDepths)) {
      delete (biomeDepths as any)[k];
    }
    initBiomeDepths();
    const depths = biomes.map(b => (Object.hasOwn(biomeDepths, b) ? `${biomeDepths[b][0]},${biomeDepths[b][1]}` : "-"));

    // generateRandomBiome for a random wave
    const randWave = 1 + rnd(3000);
    Phaser.Math.RND.sow([seedStr()]);
    const randState = state();
    let random: any;
    try {
      random = scene.generateRandomBiome(randWave);
    } catch {
      random = "NaN";
    }
    const randAfter = state();

    // SelectBiomePhase
    const gm = getGameMode(modes[rnd(modes.length)]);
    scene.gameMode = gm;
    battle.gameMode = gm;
    scene.seed = seedStr();
    const wave = rnd(4) === 0 ? 1 + rnd(3000) : 1 + rnd(210);
    battle.waveIndex = wave;
    const current = biomes[rnd(biomes.length)];
    scene.arena.biomeId = current;
    scene.modifiers.length = 0;
    const hasMap = rnd(3) === 0;
    if (hasMap) {
      scene.modifiers.push((generateModifierType((modifierTypes as any).MAP) as any).newModifier());
    }
    const phase: any = new SelectBiomePhase();
    let next: any = "none";
    const picked: number[] = [];
    phase.setNextBiomeAndEnd = (b: number) => {
      next = b;
    };
    scene.ui.setMode = (_mode: any, config?: any) => {
      if (config?.options) {
        for (const o of config.options) {
          phase.setNextBiomeAndEnd = (b: number) => picked.push(b);
          o.handler();
        }
        next = -1;
      }
    };
    phase.start();
    scene.ui.setMode = realSetMode;
    lines.push(
      [
        "B", initState, ...depths, randWave, randState, random, randAfter,
        gm.modeId, scene.seed, wave, current, hasMap ? 1 : 0, next === undefined ? "undef" : next, picked.length, ...picked,
        state(),
      ].join(" "),
    );
  }
  scene.modifiers.length = 0;
  fs.writeFileSync(OUT, lines.join("\n") + "\n");
}, 900_000);
