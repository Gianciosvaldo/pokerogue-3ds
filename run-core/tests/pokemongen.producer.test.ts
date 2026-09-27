/*
 * Upstream half of run-core's enemy Pokemon test (tools/difftest.sh copies it into the upstream
 * checkout). Each case sets a random game mode, run and wave seeds, wave, biome, battle type (with a
 * trainer of a random type), the player's Ability and Shiny Charms, the enemy's fusion chance, the
 * trainer and secret ids and a timed event's shiny boosts, then asks upstream's own
 * scene.addEnemyPokemon(species, level, trainerSlot, boss, shinyLock) for a new enemy. The moveset
 * isn't ported yet: generateAndPopulateMoveset is replaced by a snapshot of the Pokemon and
 * Phaser.Math.RND's state at that point (and makes no rolls), so the rolls before and after it are
 * checked on their own. build/pokemongentest replays the cases.
 */
import { timedEventManager } from "#app/global-event-manager";
import { speciesDataRegistry } from "#app/globals/global-species-data-registry";
import { getGameMode } from "#app/game-mode";
import { activeOverrides } from "#app/overrides";
import { modifierTypes } from "#data/data-lists";
import { BattleType } from "#enums/battle-type";
import { BiomeId } from "#enums/biome-id";
import { GameModes } from "#enums/game-modes";
import { MoveId } from "#enums/move-id";
import { SpeciesId } from "#enums/species-id";
import { TrainerType } from "#enums/trainer-type";
import { EnemyPokemon } from "#field/pokemon";
import { generateModifierType } from "#mystery-encounters/encounter-phase-utils";
import { clearVariantData, variantData } from "#sprites/variant";
import { GameManager } from "#test/framework/game-manager";
import { trainerConfigs } from "#trainers/trainer-config";
import * as fs from "node:fs";
import Phaser from "phaser";
import { it, vi } from "vitest";

it("pokemongen producer", async () => {
  const N = Number(process.env.DIFF_N ?? 20000);
  const OUT = process.env.DIFF_OUT ?? "/tmp/pr-pokemongen-cases.txt";
  const game = new GameManager(new Phaser.Game({ type: Phaser.HEADLESS }));
  game.override.battleStyle("single").startingWave(5).enemySpecies(SpeciesId.MAGIKARP).enemyMoveset(MoveId.SPLASH);
  await game.classicMode.startBattle(SpeciesId.BULBASAUR);
  const scene = game.scene as any;
  const battle = scene.currentBattle as any;
  // The enemy dev overrides addEnemyPokemon and the constructor read; a real run has none of them
  const ov: [string, any][] = [
    ["ENEMY_LEVEL_OVERRIDE", 0], ["ENEMY_SPECIES_OVERRIDE", null], ["ENEMY_FUSION_OVERRIDE", false],
    ["ENEMY_IVS_OVERRIDE", null], ["ENEMY_NATURE_OVERRIDE", null], ["ENEMY_STATUS_OVERRIDE", 0],
    ["ENEMY_GENDER_OVERRIDE", null], ["ENEMY_FORM_OVERRIDES", {}], ["ENEMY_SHINY_OVERRIDE", null],
    ["ENEMY_VARIANT_OVERRIDE", null],
  ];
  for (const [k, v] of ov) {
    vi.spyOn(activeOverrides as any, k, "get").mockReturnValue(v);
  }
  // The variant masterlist the game loads at startup (the assets aren't in the checkout)
  clearVariantData();
  Object.assign(variantData, JSON.parse(fs.readFileSync(process.env.VARIANT_MASTERLIST as string, "utf8")));
  let ev = { active: false, mult: 1, tchance: 0 };
  vi.spyOn(timedEventManager, "isEventActive").mockImplementation(() => ev.active);
  vi.spyOn(timedEventManager, "getShinyEncounterMultiplier").mockImplementation(() => ev.mult);
  vi.spyOn(timedEventManager, "getClassicTrainerShinyChance").mockImplementation(() => ev.tchance);

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
  const trainerTypes = Object.values(TrainerType).filter(v => typeof v === "number") as number[];
  const specialtyTrainers = trainerTypes.filter(t => (trainerConfigs as any)[t]?.hasSpecialtyType());
  const speciesIds = Object.keys(speciesDataRegistry.data).map(Number);
  // Species with a special case in getSpeciesFormIndex, and a few with none
  const special = [
    SpeciesId.UNOWN, SpeciesId.SHELLOS, SpeciesId.GASTRODON, SpeciesId.ROTOM, SpeciesId.BASCULIN, SpeciesId.DEERLING,
    SpeciesId.SAWSBUCK, SpeciesId.SCATTERBUG, SpeciesId.SPEWPA, SpeciesId.VIVILLON, SpeciesId.FLABEBE, SpeciesId.FLOETTE,
    SpeciesId.FLORGES, SpeciesId.FURFROU, SpeciesId.PUMPKABOO, SpeciesId.GOURGEIST, SpeciesId.ORICORIO, SpeciesId.ZARUDE,
    SpeciesId.SQUAWKABILLY, SpeciesId.PALDEA_TAUROS, SpeciesId.SINISTEA, SpeciesId.POLTEAGEIST, SpeciesId.MAUSHOLD,
    SpeciesId.DUDUNSPARCE, SpeciesId.POLTCHAGEIST, SpeciesId.SINISTCHA, SpeciesId.PICHU, SpeciesId.PIKACHU,
    SpeciesId.EEVEE, SpeciesId.MAGEARNA, SpeciesId.URSHIFU, SpeciesId.TATSUGIRI, SpeciesId.ZYGARDE, SpeciesId.MINIOR,
    SpeciesId.ALCREMIE, SpeciesId.MEOWSTIC, SpeciesId.INDEEDEE, SpeciesId.BASCULEGION, SpeciesId.OINKOLOGNE,
    SpeciesId.TOXTRICITY, SpeciesId.GIMMIGHOUL, SpeciesId.BURMY, SpeciesId.WORMADAM, SpeciesId.LYCANROC,
    SpeciesId.SILVALLY, SpeciesId.ARCEUS, SpeciesId.CHARIZARD, SpeciesId.SHEDINJA, SpeciesId.MAGNEMITE,
  ];
  const charm = (key: string, stacks: number) => {
    const m: any = generateModifierType((modifierTypes as any)[key]).newModifier();
    m.stackCount = stacks;
    return m;
  };

  // generateAndPopulateMoveset (not ported): snapshot instead
  let mid = "";
  (EnemyPokemon.prototype as any).generateAndPopulateMoveset = function (this: any) {
    mid = [
      this.abilityIndex, this.id, ...this.ivs, this.gender, this.formIndex, this.nature, this.teraType,
      this.bossSegments ?? 0, state(),
    ].join(" ");
  };

  const lines: string[] = [];
  let thrown = 0;
  for (let i = 0; i < N; i++) {
    const gm = getGameMode(modes[rnd(modes.length)]);
    scene.gameMode = gm;
    battle.gameMode = gm;
    scene.seed = seedStr();
    scene.waveSeed = rnd(10) === 0 ? "" : seedStr();
    // DIFF_FOCUS=1: the form special cases (species from the list, specialty-type trainers, waves around 30)
    const focus = process.env.DIFF_FOCUS === "1";
    const wave = focus && rnd(3) === 0 ? 29 + rnd(3) : rnd(4) === 0 ? 1 + rnd(1000) : 1 + rnd(210);
    battle.waveIndex = wave;
    const biome = biomes[rnd(biomes.length)];
    scene.arena.biomeId = biome;
    scene.arena.lastTimeOfDay = undefined;
    scene.waveCycleOffset = rnd(8) * 5;
    const battleType = rnd(2) === 0 ? BattleType.WILD : BattleType.TRAINER;
    battle.battleType = battleType;
    let trainerType = -1;
    if (battleType === BattleType.TRAINER) {
      trainerType = focus || rnd(2) === 0 ? specialtyTrainers[rnd(specialtyTrainers.length)] : trainerTypes[rnd(trainerTypes.length)];
    }
    battle.trainer = trainerType < 0 ? null : { config: (trainerConfigs as any)[trainerType] };
    const slot = battleType === BattleType.TRAINER && rnd(4) !== 0 ? 1 + rnd(2) : 0;
    const boss = rnd(3) === 0;
    const shinyLock = rnd(8) === 0;
    const species = focus || rnd(2) === 0 ? special[rnd(special.length)] : speciesIds[rnd(speciesIds.length)];
    const level = rnd(6) === 0 ? 1 : 1 + rnd(200);

    const haStacks = rnd(3) === 0 ? 1 + rnd(4) : 0;
    const shStacks = rnd(3) === 0 ? 1 + rnd(4) : 0;
    scene.modifiers.length = 0;
    if (haStacks) {
      scene.modifiers.push(charm("ABILITY_CHARM", haStacks));
    }
    if (shStacks) {
      scene.modifiers.push(charm("SHINY_CHARM", shStacks));
    }
    const fusionStacks = rnd(6) === 0 ? 1 + rnd(10) : 0;
    scene.enemyModifiers.length = 0;
    let fusionChance = 0;
    if (fusionStacks) {
      const m = charm("ENEMY_FUSED_CHANCE", fusionStacks);
      fusionChance = m.chance;
      scene.enemyModifiers.push(m);
    }
    scene.gameData.trainerId = rnd(65536);
    scene.gameData.secretId = rnd(65536);
    ev = { active: rnd(3) === 0, mult: [1, 2, 1000][rnd(3)], tchance: [0, 0, 1000, 30, 1][rnd(5)] };

    Phaser.Math.RND.sow([seedStr()]);
    const before = state();
    mid = "";
    let p: any;
    try {
      p = scene.addEnemyPokemon(speciesDataRegistry.getSpecies(species), level, slot, boss, shinyLock);
    } catch {
      thrown++;
      lines.push(`X ${species} ${battleType} ${trainerType}`);
      continue;
    }
    lines.push(
      [
        "P", gm.modeId, scene.seed, scene.waveSeed || "-", wave, biome, scene.waveCycleOffset, battleType, trainerType,
        slot, boss ? 1 : 0, shinyLock ? 1 : 0, species, level, haStacks, shStacks, fusionStacks, fusionChance,
        scene.gameData.trainerId, scene.gameData.secretId, ev.active ? 1 : 0, ev.mult, ev.tchance, before,
      ].join(" "),
    );
    lines.push(`M ${mid || "none"}`);
    lines.push(
      [
        "F", p.fusionSpecies ? 1 : 0, p.shiny ? 1 : 0, p.variant, p.luck, slot ? p.friendship : -1, ...p.ivs, state(),
      ].join(" "),
    );
    p.destroy();
  }
  lines.push(`# addEnemyPokemon threw: ${thrown}`);
  fs.writeFileSync(OUT, lines.join("\n") + "\n");
}, 900_000);
