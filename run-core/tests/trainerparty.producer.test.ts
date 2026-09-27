/*
 * Upstream half of run-core's trainer party test (tools/difftest.sh copies it into the upstream
 * checkout). Each case builds a real Trainer of a random type (a quarter of them the double named
 * trainers) and variant on a random run seed, wave, biome and game mode, sets the battle's enemy levels
 * from the trainer's own getPartyLevels, and calls upstream's own trainer.genPartyMember(e) for each
 * member in turn, as the EncounterPhase does. The moveset isn't ported yet: generateAndPopulateMoveset
 * records a snapshot of the member instead (and makes no rolls). The case records the template the
 * trainer uses and, for each member, its species and snapshot. build/trainerpartytest replays the cases.
 */
import { getGameMode } from "#app/game-mode";
import { activeOverrides } from "#app/overrides";
import { BattleType } from "#enums/battle-type";
import { BiomeId } from "#enums/biome-id";
import { GameModes } from "#enums/game-modes";
import { MoveId } from "#enums/move-id";
import { SpeciesId } from "#enums/species-id";
import { TrainerType } from "#enums/trainer-type";
import { TrainerVariant } from "#enums/trainer-variant";
import { EnemyPokemon } from "#field/pokemon";
import { Trainer } from "#field/trainer";
import { GameManager } from "#test/framework/game-manager";
import { trainerConfigs } from "#trainers/trainer-config";
import * as fs from "node:fs";
import Phaser from "phaser";
import { it, vi } from "vitest";

it("trainerparty producer", async () => {
  const N = Number(process.env.DIFF_N ?? 5000);
  const OUT = process.env.DIFF_OUT ?? "/tmp/pr-trainerparty-cases.txt";
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
  for (const [k, v] of ov) {
    vi.spyOn(activeOverrides as any, k, "get").mockReturnValue(v);
  }

  let seed = Number(process.env.DIFF_SEED ?? 12345);
  const rnd = (n: number) => {
    seed = (Math.imul(seed, 1103515245) + 12345) >>> 0;
    return (seed >>> 8) % n;
  };
  const chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
  const seedStr = () => Array.from({ length: 24 }, () => chars[rnd(chars.length)]).join("");
  // The modes without a daily config or spliced fusions, which the port reports as unported
  const modes = [GameModes.CLASSIC, GameModes.ENDLESS, GameModes.CHALLENGE];
  const biomes = Object.values(BiomeId) as number[];
  const types = Object.values(TrainerType).filter(v => typeof v === "number" && (trainerConfigs as any)[v]) as number[];
  const plain = types.filter(t => Object.keys((trainerConfigs as any)[t].partyMemberFuncs).length === 0);
  const named = types.filter(t => (trainerConfigs as any)[t].trainerTypeDouble && !(trainerConfigs as any)[t].doubleOnly);
  const tplLine = (t: any) => {
    const member = Array.from(
      { length: 8 },
      (_, i) => (t.isSameSpecies(i) ? 1 : 0) | (t.isBalanced(i) ? 2 : 0) | ((t.getEvoThresholdKind(i) ?? 1) << 2),
    );
    const offsets = Array.from({ length: 8 }, (_, i) => {
      let offset = 0;
      for (const inner of t.templates ?? []) {
        if (offset + inner.size > i) {
          break;
        }
        offset += inner.size;
      }
      return offset;
    });
    return [t.size, t.evoLevelThresholdKind ?? 1, ...member, ...offsets].join(" ");
  };

  let snap = "";
  (EnemyPokemon.prototype as any).generateAndPopulateMoveset = function (this: any) {
    snap = [this.abilityIndex, this.id, this.gender, this.formIndex, this.nature, this.teraType].join(" ");
  };

  const lines: string[] = [];
  let thrown = 0;
  for (let i = 0; i < N; i++) {
    const gm = getGameMode(modes[rnd(modes.length)]);
    scene.gameMode = gm;
    battle.gameMode = gm;
    scene.seed = seedStr();
    scene.waveSeed = seedStr();
    const wave = rnd(8) === 0 ? 20 : 1 + rnd(300);
    battle.waveIndex = wave;
    battle.battleType = BattleType.TRAINER;
    const biome = biomes[rnd(biomes.length)];
    scene.arena.biomeId = biome;
    scene.waveCycleOffset = rnd(8) * 5;
    scene.modifiers.length = 0;
    scene.enemyModifiers.length = 0;
    scene.gameData.trainerId = rnd(65536);
    scene.gameData.secretId = rnd(65536);
    const r = rnd(4);
    const type = r === 0 ? named[rnd(named.length)] : r === 1 ? types[rnd(types.length)] : plain[rnd(plain.length)];
    const variant = r === 0 ? TrainerVariant.DOUBLE : [TrainerVariant.DEFAULT, TrainerVariant.FEMALE, TrainerVariant.DOUBLE][rnd(3)];
    Phaser.Math.RND.sow([seedStr()]);
    let trainer: any;
    try {
      trainer = new Trainer(type, variant);
    } catch {
      thrown++;
      continue;
    }
    battle.trainer = trainer;
    const template = trainer.getPartyTemplate();
    trainer.getPartyTemplate = () => template;
    const levels: number[] = trainer.getPartyLevels(wave);
    battle.enemyLevels = levels;
    battle.enemyParty = [];
    const members: string[] = [];
    for (let e = 0; e < levels.length; e++) {
      snap = "";
      try {
        const p = trainer.genPartyMember(e);
        battle.enemyParty[e] = p;
        members.push(`${p.species.speciesId} ${snap || "none"}`);
      } catch {
        members.push("threw");
        break;
      }
    }
    lines.push(
      [
        "T", gm.modeId, scene.seed, wave, biome, scene.waveCycleOffset, type, trainer.isDouble() ? 1 : 0,
        wave === 20 && gm.isClassic ? 1 : 0, scene.gameData.trainerId, scene.gameData.secretId, tplLine(template),
        levels.length, ...levels,
      ].join(" "),
    );
    for (const m of members) {
      lines.push(`M ${m}`);
    }
    for (const p of battle.enemyParty) {
      p.destroy();
    }
    battle.enemyParty = [];
    trainer.destroy();
  }
  lines.push(`# Trainer threw: ${thrown}`);
  fs.writeFileSync(OUT, lines.join("\n") + "\n");
}, 1_800_000);
