/*
 * Upstream half of run-core's new-battle generation test (tools/difftest.sh copies it into the upstream
 * checkout). Each case sets a random game mode, run seed, biome, previous wave, Lures and
 * mystery-encounter save data on the real scene, then runs upstream's own scene.newBattle() and records
 * what it built: battle type, double or not, trainer type, variant, party template, the names rolled
 * (as indexes among the class's English locale names), the enemy levels, the spawn chance afterwards and
 * Phaser.Math.RND's state. build/battlegentest replays the cases.
 */
import { getGameMode } from "#app/game-mode";
import { activeOverrides } from "#app/overrides";
import { allBiomes, modifierTypes } from "#data/data-lists";
import { BiomeId } from "#enums/biome-id";
import { GameModes } from "#enums/game-modes";
import { MoveId } from "#enums/move-id";
import { SpeciesId } from "#enums/species-id";
import { generateModifierType } from "#mystery-encounters/encounter-phase-utils";
import { GameManager } from "#test/framework/game-manager";
import i18next from "i18next";
import * as fs from "node:fs";
import Phaser from "phaser";
import { it, vi } from "vitest";

it("battlegen producer", async () => {
  const N = Number(process.env.DIFF_N ?? 20000);
  const OUT = process.env.DIFF_OUT ?? "/tmp/pr-battlegen-cases.txt";
  const game = new GameManager(new Phaser.Game({ type: Phaser.HEADLESS }));
  game.override.battleStyle("single").startingWave(5).enemySpecies(SpeciesId.MAGIKARP).enemyMoveset(MoveId.SPLASH);
  await game.classicMode.startBattle(SpeciesId.BULBASAUR);
  // The test framework's dev overrides that newBattle reads; a real run has none of them
  vi.spyOn(activeOverrides, "MYSTERY_ENCOUNTER_RATE_OVERRIDE", "get").mockReturnValue(null);
  vi.spyOn(activeOverrides, "FIELD_SIZE_OVERRIDE", "get").mockReturnValue(null);
  vi.spyOn(activeOverrides, "BATTLE_TYPE_OVERRIDE", "get").mockReturnValue(null);
  vi.spyOn(activeOverrides, "RANDOM_TRAINER_OVERRIDE", "get").mockReturnValue(null);
  vi.spyOn(activeOverrides, "DISABLE_STANDARD_TRAINERS_OVERRIDE", "get").mockReturnValue(false);
  const scene = game.scene as any;
  await i18next.changeLanguage("en");

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
  const lures = ["LURE", "SUPER_LURE", "MAX_LURE"];
  const nameIndex = (key: string | undefined) => {
    if (!key) {
      return -1;
    }
    const cut = key.lastIndexOf(".");
    const entries = Object.keys(i18next.t(key.slice(0, cut), { returnObjects: true }) as any);
    return entries.indexOf(key.slice(cut + 1));
  };

  const lines: string[] = [];
  let thrown = 0;
  for (let i = 0; i < N; i++) {
    const gm = getGameMode(modes[rnd(modes.length)]);
    scene.gameMode = gm;
    scene.currentBattle.gameMode = gm;
    scene.seed = seedStr();
    scene.offsetGym = rnd(2) === 0;
    // DIFF_FOCUS=1: half the new waves are gym waves (wave % 30 === 20, or 0 with the gym offset), where
    // a boss trainer with a wave-dependent party template is due
    let prevWave = rnd(4) === 0 ? rnd(3000) + 1 : rnd(210) + 1;
    if (process.env.DIFF_FOCUS === "1" && rnd(2) === 0) {
      prevWave = 30 * rnd(8) + (scene.offsetGym ? 29 : 19);
    }
    scene.currentBattle.waveIndex = prevWave;
    scene.currentBattle.double = false;
    const biome = biomes[rnd(biomes.length)];
    scene.arena.biomeId = biome;
    scene.arena.trainerPool = allBiomes.get(biome).trainerPool;
    scene.modifiers.length = 0;
    const nLures = rnd(4) === 0 ? 1 + rnd(2) : 0;
    for (let k = 0; k < nLures; k++) {
      scene.modifiers.push((generateModifierType((modifierTypes as any)[lures[rnd(3)]]) as any).newModifier());
    }
    const saveData = scene.mysteryEncounterSaveData;
    saveData.encounterSpawnChance = 3 + 3 * rnd(40);
    saveData.encounteredEvents = [];
    const lastMe = rnd(3) === 0 ? 0 : Math.max(1, prevWave - rnd(12));
    if (lastMe) {
      for (let k = 1 + rnd(12); k > 0; k--) {
        saveData.encounteredEvents.push({ type: 0, tier: 0, waveIndex: lastMe });
      }
    }
    const nMe = saveData.encounteredEvents.length;
    const chanceBefore = saveData.encounterSpawnChance;

    let battle: any;
    try {
      battle = scene.newBattle();
    } catch {
      thrown++;
      continue;
    }
    const trainer = battle.trainer;
    lines.push(
      [
        "G", gm.modeId, scene.seed, scene.offsetGym ? 1 : 0, prevWave, biome, scene.arena.trainerChance, nLures,
        chanceBefore, nMe, lastMe, battle.waveIndex, battle.battleType, battle.double ? 1 : 0,
        trainer ? trainer.config.trainerType : -1, trainer ? trainer.variant : -1, trainer ? trainer.partyTemplateIndex : -1,
        trainer ? nameIndex(trainer.nameKey) : -1, trainer ? nameIndex(trainer.partnerNameKey) : -1,
        battle.enemyLevels.length, ...battle.enemyLevels, saveData.encounterSpawnChance, battle.battleSeed, state(),
      ].join(" "),
    );
    if (trainer) {
      scene.field.remove(trainer, true);
    }
    scene.phaseManager.clearPhaseQueue?.();
  }
  lines.push(`# newBattle threw: ${thrown}`);
  fs.writeFileSync(OUT, lines.join("\n") + "\n");
}, 900_000);
