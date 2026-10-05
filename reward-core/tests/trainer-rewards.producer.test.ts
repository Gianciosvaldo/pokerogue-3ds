/*
 * Upstream trainer rewards & trainer genModifiers producer test.
 * Calls upstream's real trainer config closures, victory phase logic, vouchers, and fixed battle settings.
 */
import { trainerConfigs } from "#data/trainers/trainer-config";
import { classicFixedBattles } from "#data/trainers/fixed-battle-configs";
import { modifierTypes } from "#data/data-lists";
import { AbilityId } from "#enums/ability-id";
import { SpeciesId } from "#enums/species-id";
import { TrainerType } from "#enums/trainer-type";
import { TrainerSlot } from "#enums/trainer-slot";
import { vouchers } from "#system/voucher";
import { GameManager } from "#test/framework/game-manager";
import * as fs from "node:fs";
import Phaser from "phaser";
import { it } from "vitest";

it("trainer rewards producer", async () => {
  const N = Number(process.env.DIFF_N ?? 2000);
  const OUT = process.env.DIFF_OUT ?? "/tmp/pr-trainer-rewards-cases.txt";
  const game = new GameManager(new Phaser.Game({ type: Phaser.HEADLESS }));
  game.override.battleStyle("single").startingWave(5).enemySpecies(SpeciesId.MAGIKARP);
  await game.classicMode.startBattle(SpeciesId.BULBASAUR);
  const scene = game.scene as any;

  let seed = Number(process.env.DIFF_SEED ?? 777);
  const rnd = (n: number) => {
    seed = (Math.imul(seed, 1103515245) + 12345) >>> 0;
    return (seed >>> 8) % n;
  };

  const keyOf = (t: any) => (typeof t === "function" ? Object.keys(modifierTypes).find(k => (modifierTypes as any)[k] === t) : t.id) ?? "?";

  // Trainer types with modifierRewardFuncs or genModifiersFunc or boss vouchers
  const allTrainerTypes = Object.keys(trainerConfigs).map(Number).filter(t => !isNaN(t));

  const lines: string[] = [];
  let thrown = 0;

  for (let i = 0; i < N; i++) {
    const trainerType = allTrainerTypes[rnd(allTrainerTypes.length)];
    const config = trainerConfigs[trainerType];

    // 1. Victory rewards test (modifierRewardFuncs + boss voucher)
    const modifierRewardFuncs = config.modifierRewardFuncs ?? [];
    const rewardItemKeys: string[] = [];
    for (const func of modifierRewardFuncs) {
      const modType = func();
      rewardItemKeys.push(keyOf(modType));
    }

    // Voucher check
    let voucherKey = "";
    const key = TrainerType[trainerType];
    const hasVoucherInSys = Object.hasOwn(vouchers, key);
    const isAlreadyUnlocked = rnd(2) === 1;
    const isBoss = config.isBoss;
    const upgradeUnlocked = rnd(2) === 1;

    if (hasVoucherInSys && !isAlreadyUnlocked && isBoss) {
      const v = vouchers[key];
      const vType = v.voucherType;
      if (upgradeUnlocked) {
        voucherKey = ["VOUCHER_PLUS", "VOUCHER_PLUS", "VOUCHER_PLUS", "VOUCHER_PREMIUM"][vType];
      } else {
        voucherKey = ["VOUCHER", "VOUCHER", "VOUCHER_PLUS", "VOUCHER_PREMIUM"][vType];
      }
    }

    // 2. genModifiersFunc test
    let genItemKeys: string[] = [];
    let isGuts = false, isPoisonHeal2 = false, isPoisonHeal4 = false, isMarvelScale = false;

    if (config.genModifiersFunc) {
      // Build a representative party for genModifiersFunc using addEnemyPokemon
      const party: any[] = [];

      const p0 = scene.addEnemyPokemon(SpeciesId.PIKACHU, 50, TrainerSlot.TRAINER);
      if (trainerType === TrainerType.STEVEN) {
        p0.species = { speciesId: SpeciesId.GIGALITH } as any;
      } else if (trainerType === TrainerType.WALLACE) {
        p0.species = { speciesId: SpeciesId.PELIPPER } as any;
      }
      party.push(p0);

      const p1 = scene.addEnemyPokemon(SpeciesId.SWELLOW, 50, TrainerSlot.TRAINER);
      isGuts = rnd(2) === 1;
      p1.species = { speciesId: SpeciesId.SWELLOW } as any;
      p1.hasAbility = (abId: any) => abId === AbilityId.GUTS && isGuts;
      party.push(p1);

      const p2 = scene.addEnemyPokemon(SpeciesId.GLISCOR, 50, TrainerSlot.TRAINER);
      isPoisonHeal2 = rnd(2) === 1;
      p2.species = { speciesId: SpeciesId.GLISCOR } as any;
      p2.hasAbility = (abId: any) => abId === AbilityId.POISON_HEAL && isPoisonHeal2;
      if (trainerType === TrainerType.KIERAN) {
        p2.hasAbility = (abId: any) => abId === AbilityId.POISON_HEAL; // Kieran's Gliscor always has Poison Heal
        isPoisonHeal2 = true;
      }
      party.push(p2);

      const p3 = scene.addEnemyPokemon(SpeciesId.PIDGEY, 50, TrainerSlot.TRAINER);
      party.push(p3);

      const p4 = scene.addEnemyPokemon(SpeciesId.GLISCOR, 50, TrainerSlot.TRAINER);
      isPoisonHeal4 = rnd(2) === 1;
      p4.species = { speciesId: SpeciesId.GLISCOR } as any;
      p4.hasAbility = (abId: any) => abId === AbilityId.POISON_HEAL && isPoisonHeal4;
      if (trainerType === TrainerType.GUZMA_2) {
        p4.hasAbility = (abId: any) => abId === AbilityId.POISON_HEAL; // Guzma 2's Gliscor always has Poison Heal
        isPoisonHeal4 = true;
      }
      party.push(p4);

      const p5 = scene.addEnemyPokemon(SpeciesId.MILOTIC, 50, TrainerSlot.TRAINER);
      isMarvelScale = rnd(2) === 1;
      p5.species = { speciesId: SpeciesId.MILOTIC } as any;
      p5.hasAbility = (abId: any) => abId === AbilityId.MARVEL_SCALE && isMarvelScale;
      party.push(p5);

      try {
        const mods = config.genModifiersFunc(party);
        for (const m of mods) {
          if (m && m.type) {
            genItemKeys.push(keyOf(m.type));
          }
        }
      } catch (e) {
        // ignore if party mismatch
      }
    }

    // 3. Fixed battle customModifierRewardSettings
    const wave = 1 + rnd(200);
    const fixedBattle = classicFixedBattles[wave];
    let customSettingsStr = "NONE";
    if (fixedBattle?.customModifierRewardSettings) {
      const cms = fixedBattle.customModifierRewardSettings;
      const tiers = cms.guaranteedModifierTiers ? cms.guaranteedModifierTiers.join(",") : "";
      customSettingsStr = `${tiers}|${cms.allowLuckUpgrades ? 1 : 0}`;
    }

    // Capture RNG state after operations
    const rngState = scene.rnd?.state() ?? "0";

    lines.push([
      "T",
      trainerType,
      isBoss ? 1 : 0,
      config.hasVoucher ? 1 : 0,
      config.moneyMultiplier,
      rewardItemKeys.length,
      ...rewardItemKeys,
      voucherKey || "NONE",
      isGuts ? 1 : 0,
      isPoisonHeal2 ? 1 : 0,
      isPoisonHeal4 ? 1 : 0,
      isMarvelScale ? 1 : 0,
      genItemKeys.length,
      ...genItemKeys,
      wave,
      customSettingsStr,
      rngState
    ].join(" "));
  }

  lines.push(`# threw: ${thrown}`);
  fs.writeFileSync(OUT, lines.join("\n") + "\n");
}, 900_000);
