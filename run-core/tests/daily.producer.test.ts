/*
 * Upstream half of run-core's Daily Run differential test.
 * Runs 50 waves of known daily seeds in upstream TypeScript and records:
 * - Initial starters and starting biome
 * - Wave boundary state (1 to 50): wave, biome, battle type, double, trainer type, variant, levels, shop status, RNG state
 * - Wild enemy / trainer party attributes (species, boss segments, shiny, variant, form, nature, gender, ability, id, IVs)
 *
 * build/dailytest replays and verifies these 50-wave runs against C.
 */
import { getGameMode } from "#app/game-mode";
import { activeOverrides } from "#app/overrides";
import { allBiomes } from "#data/data-lists";
import { getDailyRunStarters, getDailyStartingBiome } from "#data/daily-seed/daily-run";
import { BiomeId } from "#enums/biome-id";
import { GameModes } from "#enums/game-modes";
import { MoveId } from "#enums/move-id";
import { SpeciesId } from "#enums/species-id";
import { PlayerPokemon } from "#field/pokemon";
import { GameManager } from "#test/framework/game-manager";
import * as fs from "node:fs";
import Phaser from "phaser";
import { it, vi } from "vitest";

it("daily run producer", async () => {
  const OUT = process.env.DIFF_OUT ?? "/tmp/pr-daily-cases.txt";
  const game = new GameManager(new Phaser.Game({ type: Phaser.HEADLESS }));
  game.override.battleStyle("single").startingWave(1).enemySpecies(SpeciesId.MAGIKARP).enemyMoveset(MoveId.SPLASH);
  await game.classicMode.startBattle(SpeciesId.BULBASAUR);

  vi.spyOn(activeOverrides, "MYSTERY_ENCOUNTER_RATE_OVERRIDE", "get").mockReturnValue(null);
  vi.spyOn(activeOverrides, "FIELD_SIZE_OVERRIDE", "get").mockReturnValue(null);
  vi.spyOn(activeOverrides, "BATTLE_TYPE_OVERRIDE", "get").mockReturnValue(null);
  vi.spyOn(activeOverrides, "RANDOM_TRAINER_OVERRIDE", "get").mockReturnValue(null);
  vi.spyOn(activeOverrides, "DISABLE_STANDARD_TRAINERS_OVERRIDE", "get").mockReturnValue(false);

  // Moveset generation isn't ported in run-core yet (generateAndPopulateMoveset)
  vi.spyOn(PlayerPokemon.prototype, "generateAndPopulateMoveset").mockImplementation(() => {});

  const scene = game.scene as any;
  const state = () => Phaser.Math.RND.state().split(",").slice(1).join(" ");

  // Known daily seed strings (e.g. ISO date base64 strings)
  const dailySeeds = [
    "MjAyNC0wNS0xMA==", // 2024-05-10
    "MjAyNC0wOS0zMA==", // 2024-09-30
    "MjAyNS0wMS0wMQ==", // 2025-01-01
    "MjAyNS0wNS0xNQ==", // 2025-05-15
    "MjAyNS0xMC0zMQ==", // 2025-10-31
  ];

  const lines: string[] = [];

  for (const seed of dailySeeds) {
    scene.gameMode = getGameMode(GameModes.DAILY);
    scene.setSeed(seed);
    scene.resetSeed();

    const starters = getDailyRunStarters();
    const rawStartingBiome = getDailyStartingBiome();
    const startingBiome: number = typeof rawStartingBiome === "string" ? (BiomeId as any)[rawStartingBiome] : rawStartingBiome;

    lines.push(["INIT", seed, startingBiome].join(" "));

    for (const starter of starters) {
      lines.push(
        [
          "STARTER", starter.speciesId, starter.abilityIndex ?? 0, 0, starter.female ? 1 : 0,
          starter.formIndex, starter.shiny ? 1 : 0, starter.variant, starter.nature, ...starter.ivs,
        ].join(" "),
      );
    }

    scene.arena.biomeId = startingBiome;
    scene.arena.trainerPool = allBiomes.get(startingBiome).trainerPool;
    scene.currentBattle = null;

    for (let wave = 1; wave <= 50; wave++) {
      const battle = scene.newBattle();
      const trainer = battle.trainer;
      const shopStatus = scene.gameMode.getShopStatus() ? 1 : 0;

      lines.push(
        [
          "WAVE", battle.waveIndex, scene.arena.biomeId, battle.battleType, battle.double ? 1 : 0,
          trainer ? trainer.config.trainerType : -1, trainer ? trainer.variant : -1,
          trainer ? trainer.partyTemplateIndex : -1, battle.enemyLevels.length, ...battle.enemyLevels,
          shopStatus, state(),
        ].join(" "),
      );

      const enemyParty = scene.getEnemyParty();
      for (const mon of enemyParty) {
        lines.push(
          [
            "ENEMY", mon.species.speciesId, mon.bossSegments ?? 0, mon.shiny ? 1 : 0, mon.variant,
            mon.formIndex, mon.nature, mon.gender ?? -1, mon.abilityIndex, mon.id, ...mon.ivs,
          ].join(" "),
        );
      }

      if (trainer) {
        scene.field.remove(trainer, true);
      }
      scene.phaseManager.clearPhaseQueue?.();
    }
  }

  fs.writeFileSync(OUT, lines.join("\n") + "\n");
}, 900_000);
