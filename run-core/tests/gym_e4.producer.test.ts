/*
 * Task 49 upstream differential producer.  It exercises real gym-leader selection from the biome
 * boss pool and the fixed Elite Four / Champion configs at 182/184/186/188/190, then records the
 * trainer template, levels, party species and the first EnemyPokemon constructor snapshot (before
 * moveset/postProcess callbacks).
 */
import { activeOverrides } from "#app/overrides";
import { GameModes } from "#enums/game-modes";
import { MoveId } from "#enums/move-id";
import { SpeciesId } from "#enums/species-id";
import { EnemyPokemon } from "#field/pokemon";
import { GameManager } from "#test/framework/game-manager";
import * as fs from "node:fs";
import Phaser from "phaser";
import { it, vi } from "vitest";

it("gym/e4 producer", async () => {
  const N = Number(process.env.DIFF_N ?? 500);
  const OUT = process.env.DIFF_OUT ?? "/tmp/pr-gym-e4-cases.txt";
  const game = new GameManager(new Phaser.Game({ type: Phaser.HEADLESS }));
  game.override.battleStyle("single").startingWave(5).enemySpecies(SpeciesId.MAGIKARP).enemyMoveset(MoveId.SPLASH);
  await game.classicMode.startBattle(SpeciesId.BULBASAUR);
  const scene = game.scene as any;
  const battle = scene.currentBattle as any;

  for (const [k, v] of [
    ["ENEMY_LEVEL_OVERRIDE", 0], ["ENEMY_SPECIES_OVERRIDE", null], ["ENEMY_FUSION_OVERRIDE", false],
    ["ENEMY_IVS_OVERRIDE", null], ["ENEMY_NATURE_OVERRIDE", null], ["ENEMY_STATUS_OVERRIDE", 0],
    ["ENEMY_GENDER_OVERRIDE", null], ["ENEMY_FORM_OVERRIDES", {}], ["ENEMY_SHINY_OVERRIDE", null],
    ["ENEMY_VARIANT_OVERRIDE", null],
  ] as [string, any][]) {
    vi.spyOn(activeOverrides as any, k, "get").mockReturnValue(v);
  }

  let seed = Number(process.env.DIFF_SEED ?? 12345) >>> 0;
  const rnd = (n: number) => {
    seed = (Math.imul(seed, 1103515245) + 12345) >>> 0;
    return (seed >>> 8) % n;
  };
  const chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
  const seedStr = () => Array.from({ length: 24 }, () => chars[rnd(chars.length)]).join("");
  const gymNormal = [20, 50, 80, 110, 140, 170];
  const gymOffset = [30, 60, 90, 120, 150, 180];
  const e4Waves = [182, 184, 186, 188, 190];
  const biomeIds = Array.from({ length: 32 }, (_, i) => i).concat([40, 41, 50]);

  (EnemyPokemon.prototype as any).generateAndPopulateMoveset = function (this: any) {
    // Trainer post-process callbacks inspect this container after regeneration.
    // Keep it valid without adding moves or consuming any moveset RNG.
    this.moveset = [];
    if (this.__task49Snapshot === undefined) {
      this.__task49Snapshot = [
        this.abilityIndex, this.id, this.gender, this.formIndex, this.nature, this.teraType,
        this.fusionSpecies?.speciesId ?? -1, this.fusionAbilityIndex ?? 0,
        this.fusionGender ?? 0, this.fusionFormIndex ?? 0,
      ].join(" ");
    }
  };

  const tplLine = (t: any) => {
    const member = Array.from(
      { length: 8 },
      (_, i) => (t.isSameSpecies(i) ? 1 : 0) | (t.isBalanced(i) ? 2 : 0) | ((t.getEvoThresholdKind(i) ?? 1) << 2),
    );
    const offsets = Array.from({ length: 8 }, (_, i) => {
      let off = 0;
      for (const inner of t.templates ?? []) {
        if (off + inner.size > i) break;
        off += inner.size;
      }
      return off;
    });
    return [t.size, t.evoLevelThresholdKind ?? 1, ...member, ...offsets].join(" ");
  };

  const lines: string[] = [];
  for (let i = 0; i < N; i++) {
    scene.seed = seedStr();
    scene.waveCycleOffset = rnd(8) * 5;
    scene.gameData.trainerId = rnd(65536);
    scene.gameData.secretId = rnd(65536);
    scene.modifiers.length = 0;
    scene.enemyModifiers.length = 0;

    const fixed = rnd(2) !== 0;
    const offsetGym = rnd(2) !== 0;
    scene.offsetGym = offsetGym;
    const wave = fixed ? e4Waves[rnd(e4Waves.length)] : (offsetGym ? gymOffset : gymNormal)[rnd(6)];
    const biome = biomeIds[rnd(biomeIds.length)];
    scene.arena.biomeId = biome;
    battle.waveIndex = wave - 1;
    battle.enemyParty = [];

    scene.resetSeed(wave);
    let trainer: any;
    if (fixed) {
      const cfg: any = scene.gameMode.getFixedBattle(wave);
      if (!cfg) throw new Error("missing fixed config for " + wave);
      scene.executeWithSeedOffset(() => { trainer = cfg.getTrainer(); }, (cfg.seedOffsetWaveIndex || wave) << 8);
    } else {
      trainer = scene.generateNewBattleTrainer(wave);
    }

    const template = trainer.getPartyTemplate();
    trainer.getPartyTemplate = () => template;
    const levels: number[] = trainer.getPartyLevels(wave);
    battle.waveIndex = wave;
    battle.trainer = trainer;
    battle.enemyLevels = levels;
    battle.enemyParty = [];

    const members: string[] = [];
    for (let e = 0; e < levels.length; e++) {
      const p: any = trainer.genPartyMember(e);
      battle.enemyParty[e] = p;
      members.push(`M ${p.species.speciesId} ${p.__task49Snapshot ?? "none"}`);
    }

    lines.push([
      "C", fixed ? 1 : 0, scene.seed, wave, biome, offsetGym ? 1 : 0, scene.waveCycleOffset,
      scene.gameData.trainerId, scene.gameData.secretId,
      trainer.config.trainerType, trainer.variant, trainer.isDouble() ? 1 : 0,
      tplLine(template), levels.length, ...levels,
    ].join(" "));
    lines.push(...members);

    for (const p of battle.enemyParty) p.destroy();
    battle.enemyParty = [];
    trainer.destroy();
    vi.clearAllMocks(); // release destroyed receivers; do not change mock implementations
  }

  fs.writeFileSync(OUT, lines.join("\n") + "\n");
}, 1_800_000);
