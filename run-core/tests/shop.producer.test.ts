/*
 * Upstream half of run-core's shop test (tools/difftest.sh copies it into the upstream checkout).
 * Each case sets a random game mode and wave on the real scene, gives the player random Black
 * Sludges (any shop multiplier), Amulet Coins (any stack count) and sometimes Happy Hour, and
 * writes upstream's wave money, money reward, shop options (as the shop shows them and as a
 * purchase charges them) and reroll cost. build/shoptest replays them through run-core.
 */
import { getGameMode } from "#app/game-mode";
import { getArenaTag } from "#data/arena-tag";
import { modifierTypes } from "#data/data-lists";
import { ArenaTagSide } from "#enums/arena-tag-side";
import { ArenaTagType } from "#enums/arena-tag-type";
import { GameModes } from "#enums/game-modes";
import { MoveId } from "#enums/move-id";
import { SpeciesId } from "#enums/species-id";
import { HealShopCostModifier, MoneyMultiplierModifier } from "#modifiers/modifier";
import { getPlayerShopModifierTypeOptionsForWave } from "#modifiers/modifier-type";
import { generateModifierType } from "#mystery-encounters/encounter-phase-utils";
import { SelectModifierPhase } from "#phases/select-modifier-phase";
import { GameManager } from "#test/framework/game-manager";
import { NumberHolder } from "#utils/common";
import * as fs from "node:fs";
import Phaser from "phaser";
import { it } from "vitest";

it("shop producer", async () => {
  const N = Number(process.env.DIFF_N ?? 20000);
  const OUT = process.env.DIFF_OUT ?? "/tmp/pr-shop-cases.txt";
  const game = new GameManager(new Phaser.Game({ type: Phaser.HEADLESS }));
  game.override.battleStyle("single").startingWave(5).enemySpecies(SpeciesId.MAGIKARP).enemyMoveset(MoveId.SPLASH);
  await game.classicMode.startBattle(SpeciesId.BULBASAUR);
  const scene = game.scene as any;
  const battle = scene.currentBattle as any;

  let seed = Number(process.env.DIFF_SEED ?? 12345);
  const rnd = (n: number) => {
    seed = (Math.imul(seed, 1103515245) + 12345) >>> 0;
    return (seed >>> 8) % n;
  };
  const modes = [GameModes.CLASSIC, GameModes.ENDLESS, GameModes.SPLICED_ENDLESS, GameModes.DAILY, GameModes.CHALLENGE];
  const itemName = (o: any) => o.type.localeKey.split(".").pop();
  const shopLine = (tag: string, opts: any[], costs: number[]) =>
    [tag, opts.length, ...opts.flatMap((o: any, k: number) => [itemName(o), costs[k]])].join(" ");

  const lines: string[] = [];
  for (let i = 0; i < N; i++) {
    const gm = getGameMode(modes[rnd(modes.length)]);
    scene.gameMode = gm;
    battle.gameMode = gm;
    const wave = rnd(4) === 0 ? 1 + rnd(5000) : 1 + rnd(210);
    battle.waveIndex = wave;

    // The player's modifiers: 0 to 2 Black Sludges (only the first applies), 0 to 3 Amulet Coins
    scene.modifiers.length = 0;
    const sludges: number[] = [];
    for (let k = rnd(3) === 0 ? 1 + rnd(2) : 0; k > 0; k--) {
      const pregen = rnd(3) === 0 ? undefined : [[2.5, 1.5, 3, 0.5][rnd(4)] + rnd(8) / 7];
      const m = (generateModifierType((modifierTypes as any).MYSTERY_ENCOUNTER_BLACK_SLUDGE, pregen) as any).newModifier();
      scene.modifiers.push(m);
      sludges.push(m.shopMultiplier);
    }
    const coins: number[] = [];
    for (let k = rnd(2) === 0 ? rnd(4) : 0; k > 0; k--) {
      const m = (generateModifierType((modifierTypes as any).AMULET_COIN) as any).newModifier();
      m.stackCount = 1 + rnd(5);
      if (rnd(4) === 0) {
        m.virtualStackCount = 1 + rnd(2);
      }
      scene.modifiers.splice(rnd(scene.modifiers.length + 1), 0, m);
    }
    // in list order: each coin floors its bonus, so the order changes the total
    for (const m of scene.modifiers) {
      if (m instanceof MoneyMultiplierModifier) {
        coins.push(m.getStackCount());
      }
    }
    scene.arena.tags.length = 0;
    const happyHour = rnd(4) === 0;
    if (happyHour) {
      scene.arena.tags.push(getArenaTag(ArenaTagType.HAPPY_HOUR, 5, MoveId.HAPPY_HOUR, 0, ArenaTagSide.PLAYER));
    }

    const moneyMultiplier = [1, 1, 1.5, 2, 0.2, 2.5, 0.5 + rnd(40) / 10][rnd(7)];
    const money = scene.getWaveMoneyAmount(moneyMultiplier);
    // MoneyRewardPhase.start's arithmetic, on upstream's own modifiers and arena
    const reward = new NumberHolder(scene.getWaveMoneyAmount(moneyMultiplier));
    scene.applyModifiers(MoneyMultiplierModifier, true, reward);
    if (scene.arena.getTag(ArenaTagType.HAPPY_HOUR)) {
      reward.value *= 2;
    }
    const shopStatus = gm.getShopStatus() ? 1 : 0;

    // As the shop screen shows the options (Black Sludge applied to the base cost)
    const shownBase = new NumberHolder(scene.getWaveMoneyAmount(1));
    scene.applyModifier(HealShopCostModifier, true, shownBase);
    const shown = getPlayerShopModifierTypeOptionsForWave(wave, shownBase.value);
    // As a purchase charges them (Black Sludge applied to the option's cost)
    const bought = getPlayerShopModifierTypeOptionsForWave(wave, scene.getWaveMoneyAmount(1));
    const charged = bought.map((o: any) => {
      const h = new NumberHolder(o.cost);
      scene.applyModifier(HealShopCostModifier, true, h);
      return h.value;
    });

    // Reroll cost on a real SelectModifierPhase, with options of random tiers (some undefined)
    const rerollCount = rnd(4) === 0 ? rnd(12) : rnd(3);
    const custom = rnd(6) === 0 ? { rerollMultiplier: [-1, 0, 0.5, 2, 3.5][rnd(5)] } : undefined;
    const phase: any = new SelectModifierPhase(rerollCount, undefined, custom as any);
    const tiers = Array.from({ length: rnd(6) }, () => (rnd(8) === 0 ? -1 : rnd(rnd(10) === 0 ? 6 : 5)));
    phase.typeOptions = tiers.map(t => ({ type: { tier: t < 0 ? undefined : t } }));
    const lock = rnd(2) === 0;
    const reroll = phase.getRerollCost(lock);

    lines.push(
      [
        "S", gm.modeId, wave, sludges.length ? sludges[0] : 0, coins.length, ...coins, happyHour ? 1 : 0,
        moneyMultiplier, money, reward.value, shopStatus, lock ? 1 : 0, rerollCount,
        custom ? custom.rerollMultiplier : "NaN", tiers.length, ...tiers, reroll,
      ].join(" "),
      shopLine("V", shown, shown.map((o: any) => o.cost)),
      shopLine("B", bought, charged),
    );
  }
  scene.modifiers.length = 0;
  scene.arena.tags.length = 0;
  fs.writeFileSync(OUT, lines.join("\n") + "\n");
}, 900_000);
