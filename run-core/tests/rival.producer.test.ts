/*
 * Upstream half of Task 48 rival fixed-party seed verification.
 * Existing producers are untouched. This isolates the static party choices from the unported moveset.
 */
import { activeOverrides } from "#app/overrides";
import { GameModes } from "#enums/game-modes";
import { MoveId } from "#enums/move-id";
import { SpeciesId } from "#enums/species-id";
import { TrainerType } from "#enums/trainer-type";
import { TrainerVariant } from "#enums/trainer-variant";
import { EnemyPokemon } from "#field/pokemon";
import { Trainer } from "#field/trainer";
import { GameManager } from "#test/framework/game-manager";
import * as fs from "node:fs";
import Phaser from "phaser";
import { it, vi } from "vitest";

it("rival fixed party producer", async () => {
  const N = Number(process.env.DIFF_N ?? 500);
  const OUT = process.env.DIFF_OUT ?? "/tmp/pr-rival-cases.txt";
  const game = new GameManager(new Phaser.Game({ type: Phaser.HEADLESS }));
  game.override.battleStyle("single").startingWave(5).enemySpecies(SpeciesId.MAGIKARP).enemyMoveset(MoveId.SPLASH);
  await game.classicMode.startBattle(SpeciesId.BULBASAUR);
  const scene = game.scene as any;
  const battle = scene.currentBattle as any;
  const ov: [string, any][] = [
    ["ENEMY_LEVEL_OVERRIDE", 0], ["ENEMY_SPECIES_OVERRIDE", null], ["ENEMY_FUSION_OVERRIDE", false],
    ["ENEMY_IVS_OVERRIDE", null], ["ENEMY_NATURE_OVERRIDE", null], ["ENEMY_STATUS_OVERRIDE", 0],
    ["ENEMY_GENDER_OVERRIDE", null], ["ENEMY_FORM_OVERRIDES", {}], ["ENEMY_SHINY_OVERRIDE", null],
    ["ENEMY_VARIANT_OVERRIDE", null],
  ];
  for (const [k, v] of ov) vi.spyOn(activeOverrides as any, k, "get").mockReturnValue(v);

  /* Moves are outside Task 48. Removing their rolls makes this test target exactly the static
   * rival seed/pool closures and their deterministic post-process state. */
  vi.spyOn(EnemyPokemon.prototype as any, "generateAndPopulateMoveset").mockImplementation(() => {});

  let seed = Number(process.env.DIFF_SEED ?? 12345) >>> 0;
  const rnd = (n: number) => {
    seed = (Math.imul(seed, 1103515245) + 12345) >>> 0;
    return (seed >>> 8) % n;
  };
  const chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
  const seedStr = () => Array.from({ length: 24 }, () => chars[rnd(chars.length)]).join("");
  const fights = [
    [8, TrainerType.RIVAL], [25, TrainerType.RIVAL_2], [55, TrainerType.RIVAL_3],
    [95, TrainerType.RIVAL_4], [145, TrainerType.RIVAL_5], [195, TrainerType.RIVAL_6],
  ] as const;

  scene.gameMode = scene.gameMode = (await import("#app/game-mode")).getGameMode(GameModes.CLASSIC);
  scene.gameData.trainerId = 1234;
  scene.gameData.secretId = 5678;

  const lines: string[] = [];
  for (let c = 0; c < N; c++) {
    const runSeed = seedStr();
    for (let f = 0; f < fights.length; f++) {
      const [wave, type] = fights[f];
      scene.setSeed(runSeed);
      scene.resetSeed(wave);
      battle.waveIndex = wave;
      battle.battleType = 1;
      battle.enemyParty = [];

      const trainer: any = new Trainer(type, TrainerVariant.DEFAULT);
      battle.trainer = trainer;
      const levels: number[] = trainer.getPartyLevels(wave);
      battle.enemyLevels = levels;
      const members: any[] = [];
      for (let i = 0; i < levels.length; i++) {
        const p: any = trainer.genPartyMember(i);
        battle.enemyParty.push(p);
        members.push({
          species: p.species.speciesId,
          level: p.level,
          ability: p.abilityIndex,
          tera: p.teraType,
          boss: p.bossSegments ?? 0,
          shiny: p.shiny ? 1 : 0,
          variant: p.variant ?? 0,
          form: i === 5 ? p.formIndex : -1,
        });
      }
      lines.push(["R", runSeed, wave, type, levels.length, ...levels].join(" "));
      for (const m of members)
        lines.push(["M",m.species,m.level,m.ability,m.tera,m.boss,m.shiny,m.variant,m.form].join(" "));
      for (const p of battle.enemyParty) {
        // Match Phaser destruction: the headless mock does not detach children.
        // All scalar observations above are complete; this consumes no RNG.
        scene.fieldUI.remove(p.battleInfo);
        p.destroy();
      }
      battle.enemyParty = [];
      trainer.destroy();
      // Retaining mock receivers keeps destroyed Pokemon alive across 18,000
      // fights. No assertion reads this history; behavior and draws are intact.
      vi.clearAllMocks();
    }
  }
  fs.writeFileSync(OUT, lines.join("\n") + "\n");
}, 1_800_000);
