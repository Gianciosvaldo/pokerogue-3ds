import { beforeAll, describe, it } from "vitest";
import { globalScene } from "#app/global-scene";
import { allMysteryEncounters, initMysteryEncounters } from "#data/mystery-encounters/mystery-encounter-biomes";
import { MysteryEncounterType } from "#enums/mystery-encounter-type";
import { GameManager } from "#test/framework/game-manager";
import Phaser from "phaser";
import * as fs from "fs";

describe("Dump Mystery Actions", () => {
  let phaserGame: Phaser.Game;

  beforeAll(() => {
    phaserGame = new Phaser.Game({
      type: Phaser.HEADLESS,
    });
  });

  it("dumps mystery encounter choices and outcomes", async () => {
    initMysteryEncounters();
    let out = "";

    const game = new GameManager(phaserGame);
    await game.runToTitle();

    for (let type = 0; type < 31; type++) {
      const enc = allMysteryEncounters[type as MysteryEncounterType];
      if (!enc) continue;

      for (let opt = 0; opt < enc.options.length; opt++) {
        try {
          game.override.seed("seed_" + type + "_" + opt);
          globalScene.money = 1000000;

          const option = enc.options[opt];
          if (option && option.onOptionPhase) {
            globalScene.currentBattle = {
              mysteryEncounter: enc,
              rng: globalScene.rnd,
              waveIndex: 50,
            } as any;
            try {
              await option.onOptionPhase();
            } catch (e) {
              // Ignore UI errors
            }
          }

          out += `TYPE:${type} OPTION:${opt}\n`;
          out += `RESULT_MONEY:${globalScene.money}\n`;
          out += `END_CASE\n`;
        } catch (e) {
          console.log("Error type " + type + " opt " + opt + ": " + e);
        }
      }
    }

    const dumpPath = process.env.DUMP_OUT || "/tmp/mystery_actions.txt";
    fs.writeFileSync(dumpPath, out);
    console.log("Wrote mystery actions dump to " + dumpPath + ", len = " + out.length);
  }, 300000);
});
