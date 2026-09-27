/*
 * Upstream half of run-core's wild enemy test (tools/difftest.sh copies it into the upstream checkout).
 * Each case sets a random game mode, run and wave seeds, biome (End one time in 8), wave, cycle and gym
 * offsets, party luck, Golden Bug Net, charms and trainer ids, then makes the first enemy of a wild
 * battle the way EncounterPhase.doEncounter does. The phase itself is tied to the UI and asset loading,
 * so its few lines for a wild enemy are transcribed here, calling upstream's own functions:
 *   species = scene.randomSpecies(waveIndex, level, true)   (= arena.randomSpecies(waveIndex, level, 0, luck))
 *   Golden Bug Net: !gameMode.isBoss(waveIndex) && biome !== END && randSeedInt(10) === 0 → getGoldenBugNetSpecies(level)
 *   scene.addEnemyPokemon(species, level, NONE, !!getEncounterBossSegments(waveIndex, level, species), shinyLocked)
 * The moveset isn't ported yet: generateAndPopulateMoveset records a snapshot and Phaser.Math.RND's state.
 * build/wildenemytest replays the cases.
 */
import { getGameMode } from "#app/game-mode";
import { activeOverrides } from "#app/overrides";
import { allBiomes, modifierTypes } from "#data/data-lists";
import { BattleType } from "#enums/battle-type";
import { BiomeId } from "#enums/biome-id";
import { Challenges } from "#enums/challenges";
import { GameModes } from "#enums/game-modes";
import { MoveId } from "#enums/move-id";
import { SpeciesId } from "#enums/species-id";
import { EnemyPokemon } from "#field/pokemon";
import { getGoldenBugNetSpecies } from "#mystery-encounters/encounter-pokemon-utils";
import { generateModifierType } from "#mystery-encounters/encounter-phase-utils";
import { clearVariantData, variantData } from "#sprites/variant";
import { GameManager } from "#test/framework/game-manager";
import { randSeedInt } from "#utils/common";
import * as fs from "node:fs";
import Phaser from "phaser";
import { it, vi } from "vitest";

it("wildenemy producer", async () => {
  const N = Number(process.env.DIFF_N ?? 5000);
  const OUT = process.env.DIFF_OUT ?? "/tmp/pr-wildenemy-cases.txt";
  const game = new GameManager(new Phaser.Game({ type: Phaser.HEADLESS }));
  game.override.battleStyle("single").startingWave(5).enemySpecies(SpeciesId.MAGIKARP).enemyMoveset(MoveId.SPLASH);
  await game.classicMode.startBattle(SpeciesId.BULBASAUR);
  const scene = game.scene as any;
  const battle = scene.currentBattle as any;
  const arena = scene.arena as any;
  const ov: [string, any][] = [
    ["ENEMY_LEVEL_OVERRIDE", 0], ["ENEMY_SPECIES_OVERRIDE", null], ["ENEMY_FUSION_OVERRIDE", false],
    ["ENEMY_IVS_OVERRIDE", null], ["ENEMY_NATURE_OVERRIDE", null], ["ENEMY_STATUS_OVERRIDE", 0],
    ["ENEMY_GENDER_OVERRIDE", null], ["ENEMY_FORM_OVERRIDES", {}], ["ENEMY_SHINY_OVERRIDE", null],
    ["ENEMY_VARIANT_OVERRIDE", null],
  ];
  for (const [k, v] of ov) {
    vi.spyOn(activeOverrides as any, k, "get").mockReturnValue(v);
  }
  clearVariantData();
  Object.assign(variantData, JSON.parse(fs.readFileSync(process.env.VARIANT_MASTERLIST as string, "utf8")));

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
  const charm = (key: string, stacks: number) => {
    const m: any = generateModifierType((modifierTypes as any)[key]).newModifier();
    m.stackCount = stacks;
    return m;
  };
  let mid = "";
  (EnemyPokemon.prototype as any).generateAndPopulateMoveset = function (this: any) {
    mid = [
      this.abilityIndex, this.id, this.gender, this.formIndex, this.nature, this.teraType, this.bossSegments ?? 0,
      state(),
    ].join(" ");
  };

  const lines: string[] = [];
  let thrown = 0;
  for (let i = 0; i < N; i++) {
    const gm = getGameMode(modes[rnd(modes.length)]);
    scene.gameMode = gm;
    battle.gameMode = gm;
    battle.battleType = BattleType.WILD;
    battle.trainer = null;
    scene.seed = seedStr();
    scene.waveSeed = seedStr();
    const biome = rnd(8) === 0 ? BiomeId.END : biomes[rnd(biomes.length)];
    arena.biomeId = biome;
    arena.trainerPool = allBiomes.get(biome).trainerPool;
    arena.lastTimeOfDay = undefined;
    const wave = rnd(4) === 0 ? 1 + rnd(1000) : 1 + rnd(210);
    battle.waveIndex = wave;
    scene.waveCycleOffset = rnd(8) * 5;
    scene.offsetGym = rnd(2) === 0;
    arena.getTimeOfDay();
    arena.updatePoolsForTimeOfDay();
    const level = 1 + rnd(150);
    const luck = rnd(3) === 0 ? rnd(15) : 0;
    const bugNet = rnd(3) === 0;
    scene.modifiers.length = 0;
    const ha = rnd(3) === 0 ? 1 + rnd(4) : 0;
    const sh = rnd(3) === 0 ? 1 + rnd(4) : 0;
    if (ha) {
      scene.modifiers.push(charm("ABILITY_CHARM", ha));
    }
    if (sh) {
      scene.modifiers.push(charm("SHINY_CHARM", sh));
    }
    scene.enemyModifiers.length = 0;
    scene.gameData.trainerId = rnd(65536);
    scene.gameData.secretId = rnd(65536);

    Phaser.Math.RND.sow([seedStr()]);
    const before = state();
    mid = "";
    let species: any;
    let boss = false;
    let shinyLock = false;
    try {
      species = arena.randomSpecies(wave, level, 0, luck);
      if (bugNet && !gm.isBoss(wave) && biome !== BiomeId.END && randSeedInt(10) === 0) {
        species = getGoldenBugNetSpecies(level);
      }
      boss = !!scene.getEncounterBossSegments(wave, level, species);
      shinyLock = biome === BiomeId.END && (gm.isEndless || gm.hasChallenge(Challenges.FRESH_START));
      const p = scene.addEnemyPokemon(species, level, 0, boss, shinyLock);
      p.destroy();
    } catch {
      thrown++;
      lines.push("X");
      continue;
    }
    lines.push(
      [
        "W", gm.modeId, scene.seed, scene.waveSeed, wave, biome, scene.waveCycleOffset, scene.offsetGym ? 1 : 0,
        level, luck, bugNet ? 1 : 0, ha, sh, scene.gameData.trainerId, scene.gameData.secretId, before,
        species.speciesId, boss ? 1 : 0, shinyLock ? 1 : 0, mid || "none",
      ].join(" "),
    );
  }
  lines.push(`# threw: ${thrown}`);
  fs.writeFileSync(OUT, lines.join("\n") + "\n");
}, 1_800_000);
