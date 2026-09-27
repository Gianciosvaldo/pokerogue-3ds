/*
 * Upstream half of run-core's new-battle test (tools/difftest.sh copies it into the upstream checkout).
 * Each case asks, for a random game mode, run seed, wave and battle type, isMysteryEncounterValidForWave
 * and isWaveMysteryEncounter with random mystery-encounter save data; then builds a real Trainer of a
 * random type and variant and asks getPartyLevels for a random wave. The trainer's party template is
 * read once and pinned, so the case records exactly the template the levels come from.
 * build/newbattletest replays the cases.
 */
import { getGameMode } from "#app/game-mode";
import { activeOverrides } from "#app/overrides";
import { BattleType } from "#enums/battle-type";
import { GameModes } from "#enums/game-modes";
import { MoveId } from "#enums/move-id";
import { SpeciesId } from "#enums/species-id";
import { TrainerType } from "#enums/trainer-type";
import { TrainerVariant } from "#enums/trainer-variant";
import { Trainer } from "#field/trainer";
import { GameManager } from "#test/framework/game-manager";
import * as fs from "node:fs";
import Phaser from "phaser";
import { it, vi } from "vitest";

it("newbattle producer", async () => {
  const N = Number(process.env.DIFF_N ?? 20000);
  const OUT = process.env.DIFF_OUT ?? "/tmp/pr-newbattle-cases.txt";
  const game = new GameManager(new Phaser.Game({ type: Phaser.HEADLESS }));
  game.override.battleStyle("single").startingWave(5).enemySpecies(SpeciesId.MAGIKARP).enemyMoveset(MoveId.SPLASH);
  await game.classicMode.startBattle(SpeciesId.BULBASAUR);
  // The test framework forces the mystery-encounter rate to 0 (a dev override); a real run has none
  vi.spyOn(activeOverrides, "MYSTERY_ENCOUNTER_RATE_OVERRIDE", "get").mockReturnValue(null);
  const scene = game.scene as any;
  const battle = scene.currentBattle as any;

  let seed = Number(process.env.DIFF_SEED ?? 12345);
  const rnd = (n: number) => {
    seed = (Math.imul(seed, 1103515245) + 12345) >>> 0;
    return (seed >>> 8) % n;
  };
  const chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
  const seedStr = () => Array.from({ length: 24 }, () => chars[rnd(chars.length)]).join("");
  const modes = [GameModes.CLASSIC, GameModes.ENDLESS, GameModes.SPLICED_ENDLESS, GameModes.DAILY, GameModes.CHALLENGE];
  const trainerTypes = Object.values(TrainerType).filter(v => typeof v === "number") as number[];
  const battleTypes = [BattleType.WILD, BattleType.WILD, BattleType.TRAINER, BattleType.MYSTERY_ENCOUNTER];
  const saveData = scene.mysteryEncounterSaveData;

  const lines: string[] = [];
  for (let i = 0; i < N; i++) {
    const gm = getGameMode(modes[rnd(modes.length)]);
    scene.gameMode = gm;
    battle.gameMode = gm;
    scene.seed = seedStr();
    const wave = rnd(3) === 0 ? 1 + rnd(400) : 1 + rnd(200);
    const battleType = battleTypes[rnd(battleTypes.length)];

    // Mystery-encounter save data: the spawn chance and the waves of earlier encounters
    saveData.encounterSpawnChance = 3 + 3 * rnd(40);
    saveData.encounteredEvents = [];
    let w = 1 + rnd(20);
    for (let k = rnd(3) === 0 ? 0 : rnd(20); k > 0 && w < wave; k--) {
      saveData.encounteredEvents.push({ type: 0, tier: 0, waveIndex: w });
      w += 1 + rnd(12);
    }
    const n = saveData.encounteredEvents.length;
    const last = n ? saveData.encounteredEvents[n - 1].waveIndex : 0;
    let valid: any;
    let me: any;
    try {
      valid = scene.isMysteryEncounterValidForWave(battleType, wave) ? 1 : 0;
      me = scene.isWaveMysteryEncounter(battleType, wave) ? 1 : 0;
    } catch {
      valid = "NaN";
      me = "NaN";
    }

    // A trainer's party levels
    const type = trainerTypes[rnd(trainerTypes.length)];
    const variant = [TrainerVariant.DEFAULT, TrainerVariant.FEMALE, TrainerVariant.DOUBLE][rnd(3)];
    let partyLine = "none";
    try {
      const trainer: any = new Trainer(type, variant);
      const template = trainer.getPartyTemplate();
      trainer.getPartyTemplate = () => template;
      const partyWave = 1 + rnd(250);
      const levels = trainer.getPartyLevels(partyWave);
      const strengths = Array.from({ length: template.size }, (_, k) => template.getStrength(k));
      partyLine = [
        partyWave, trainer.isDouble() ? 1 : 0, template.size, ...strengths, template.strength ?? -1,
        levels.length, ...levels,
      ].join(" ");
      trainer.destroy();
    } catch {
      partyLine = "none";
    }

    lines.push(["N", gm.modeId, scene.seed, wave, battleType, saveData.encounterSpawnChance, n, last, valid, me].join(" "));
    lines.push(`L ${partyLine}`);
  }
  saveData.encounteredEvents = [];
  fs.writeFileSync(OUT, lines.join("\n") + "\n");
}, 900_000);
