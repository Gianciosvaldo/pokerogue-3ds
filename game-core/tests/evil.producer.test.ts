/*
 * Upstream differential producer for the evil teams' fixed battles in the run (game-core, tools/evildiff.sh). It
 * is run-core's gym/E4 producer (tests/gym_e4.producer.test.ts, left as it is) with the fixed waves swapped for
 * DIFF_WAVES (a comma list; default the Town Youngster at 5 and the grunt fights at 35, 62, 64 and 112): the real
 * fixed config's trainer under its seed offset, then the party template, the levels, the party species and the
 * first EnemyPokemon constructor snapshot (before the moveset and the post-process callbacks, "M"), and what the post-process callbacks
 * left ("P": ability slot, gender, form, boss segments, friendship, the four moves). The moveset is upstream's real generator (the member's moves
 * and RNG draws are compared).
 *
 * getRandomTrainerFunc rolls an evil team's gender and a grunt's double variant with randInt, which is
 * Math.random upstream: not seeded. The producer feeds it two known values and writes them in an "R" line before
 * the case ("R <gender roll 0..1> <double roll 0..2>"), so that the C side can be given the same rolls.
 */
import { activeOverrides } from "#app/overrides";
import { allBiomes } from "#data/data-lists";
import { GameModes } from "#enums/game-modes";
import { MoveId } from "#enums/move-id";
import { SpeciesId } from "#enums/species-id";
import { EnemyPokemon } from "#field/pokemon";
import { GameManager } from "#test/framework/game-manager";
import * as fs from "node:fs";
import Phaser from "phaser";
import { it, vi } from "vitest";

it("evil team producer", async () => {
  const N = Number(process.env.DIFF_N ?? 500);
  const OUT = process.env.DIFF_OUT ?? "/tmp/pr-evil-cases.txt";
  const WAVES = (process.env.DIFF_WAVES ?? "5,35,62,64,112").split(",").map(Number);
  // DIFF_MODE=gym: run-core's selection instead (the Elite Four / Champion fixed fights and the gym leaders from the biome's boss pool, at the
  // gym waves of either offset)
  const GYM = process.env.DIFF_MODE === "gym";
  const gymNormal = [20, 50, 80, 110, 140, 170];
  const gymOffset = [30, 60, 90, 120, 150, 180];
  const e4Waves = [182, 184, 186, 188, 190];
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
  const biomeIds = Array.from({ length: 32 }, (_, i) => i).concat([40, 41, 50]);

  const realMoveset = (EnemyPokemon.prototype as any).generateAndPopulateMoveset;
  (EnemyPokemon.prototype as any).generateAndPopulateMoveset = function (this: any, ...args: any[]) {
    if (this.__task49Snapshot === undefined) {
      this.__task49Snapshot = [
        this.abilityIndex, this.id, this.gender, this.formIndex, this.nature, this.teraType,
        this.fusionSpecies?.speciesId ?? -1, this.fusionAbilityIndex ?? 0,
        this.fusionGender ?? 0, this.fusionFormIndex ?? 0,
      ].join(" ");
    }
    realMoveset.apply(this, args);
    // the constructor's own moveset (the first generation), which a postProcess's regeneration replaces
    this.__ctorMoves ??= [0, 1, 2, 3].map(k => this.moveset[k]?.moveId ?? 0).join(" ");
  };

  // init() follows the postProcess in addEnemyPokemon: the Tera type then is the one the constructor and the postProcess left (Trainer.genAI picks later)
  const realInit = (EnemyPokemon.prototype as any).init;
  (EnemyPokemon.prototype as any).init = function (this: any, ...args: any[]) {
    this.__postTera = this.teraType;
    return realInit.apply(this, args);
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
    const textureCount = scene.textures.list.length;
    scene.seed = seedStr();
    scene.waveCycleOffset = rnd(8) * 5;
    scene.gameData.trainerId = rnd(65536);
    scene.gameData.secretId = rnd(65536);
    scene.modifiers.length = 0;
    scene.enemyModifiers.length = 0;

    const fixed = GYM ? rnd(2) !== 0 : true;
    const offsetGym = rnd(2) !== 0;
    scene.offsetGym = offsetGym;
    const wave = GYM ? (fixed ? e4Waves[rnd(e4Waves.length)] : (offsetGym ? gymOffset : gymNormal)[rnd(6)]) : WAVES[rnd(WAVES.length)];
    const biome = biomeIds[rnd(biomeIds.length)];
    scene.arena.biomeId = biome;
    // The Arena constructor copies its biome's trainer pool; setting biomeId alone kept the starting
    // biome's pool (no boss tier), so upstream drew a Youngster on every gym wave. Consumes no RNG.
    scene.arena.trainerPool = allBiomes.get(biome).trainerPool;
    battle.waveIndex = wave - 1;
    battle.enemyParty = [];

    scene.resetSeed(wave);
    let trainer: any;
    // randInt is Math.random: hand getRandomTrainerFunc two known values (gender, then the grunts' double roll)
    const u1 = rnd(1000) / 1000, u2 = rnd(1000) / 1000;
    const real = Math.random;
    let calls = 0;
    Math.random = () => (calls++ === 0 ? u1 : calls === 2 ? u2 : real());
    try {
      if (fixed) {
        const cfg: any = scene.gameMode.getFixedBattle(wave);
        if (!cfg) throw new Error("missing fixed config for " + wave);
        scene.executeWithSeedOffset(() => { trainer = cfg.getTrainer(); }, (cfg.seedOffsetWaveIndex || wave) << 8);
      } else {
        trainer = scene.generateNewBattleTrainer(wave);
      }
    } finally {
      Math.random = real;
    }
    lines.push(`R ${Math.floor(u1 * 2)} ${Math.floor(u2 * 3)}`);

    const template = trainer.getPartyTemplate();
    trainer.getPartyTemplate = () => template;
    const levels: number[] = trainer.getPartyLevels(wave);
    battle.waveIndex = wave;
    battle.trainer = trainer;
    battle.enemyLevels = levels;
    battle.enemyParty = [];
    battle.double = trainer.isDouble();   // filterMovePool: the doubles-only moves are for doubles

    const members: string[] = [];
    for (let e = 0; e < levels.length; e++) {
      const p: any = trainer.genPartyMember(e);
      battle.enemyParty[e] = p;
      members.push(`M ${p.species.speciesId} ${p.__task49Snapshot ? `${p.__task49Snapshot} ${p.__ctorMoves}` : "none"}`);
      members.push(`P ${p.abilityIndex} ${p.gender} ${p.formIndex} ${p.bossSegments ?? 0} ${p.friendship} ${[0, 1, 2, 3].map(k => p.moveset[k]?.moveId ?? 0).join(" ")} ${p.__postTera ?? p.teraType}`);
    }

    lines.push([
      "C", fixed ? 1 : 0, scene.seed, wave, biome, offsetGym ? 1 : 0, scene.waveCycleOffset,
      scene.gameData.trainerId, scene.gameData.secretId,
      trainer.config.trainerType, trainer.variant, trainer.isDouble() ? 1 : 0,
      tplLine(template), levels.length, ...levels,
    ].join(" "));
    lines.push(`T ${trainer.nameKey ?? "-"} ${trainer.partnerNameKey ?? "-"}`);
    lines.push(...members);

    // Release this configuration's display objects as Phaser would (see the rival producer): the
    // mock containers neither destroy their children nor leave their parents, and the field, the
    // Tera sparkle handler and the mock factory keep references, so memory grew with N.
    // All scalar observations above are complete; none of this draws RNG.
    const owned: any[] = [];
    const subtree = (o: any) => { for (const child of o?.list ?? []) { owned.push(child); subtree(child); } };
    for (const p of battle.enemyParty) { subtree(p); subtree(p.battleInfo); }
    subtree(trainer);
    for (const p of battle.enemyParty) {
      scene.fieldUI.remove(p.battleInfo);
      scene.field.remove(p);
      p.destroy();
    }
    battle.enemyParty = [];
    trainer.destroy();
    for (const o of owned) o.destroy?.();
    scene.spriteTeraSparkleHandler.remove(...owned);
    scene.textures.list.length = textureCount;
    vi.clearAllMocks(); // release destroyed receivers; do not change mock implementations
  }

  fs.writeFileSync(OUT, lines.join("\n") + "\n");
}, 1_800_000);
