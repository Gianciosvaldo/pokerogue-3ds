/*
 * Upstream half of game-core's voucher application replay (tools/voucherapplydiff.sh copies it into the pinned upstream
 * checkout). Runs upstream's own classes, none of the rule rewritten here: modifierTypes.VOUCHER / VOUCHER_PLUS /
 * VOUCHER_PREMIUM (AddVoucherModifierType), their newModifier() (AddVoucherModifier) and
 *  - for the reward taken from the reward screen: BattleScene.addModifier, as SelectModifierPhase.applyModifier calls it
 *    (a ConsumableModifier: apply() adds the count; no text is shown),
 *  - for the reward given directly: ModifierRewardPhase.doReward (addModifier, then the "You received" text).
 * Both on random starting counts of the four voucher types. Output, per case:
 *   K <key> <the 4 starting voucherCounts>
 *   R <applied 0/1> <selectFilter's text or OK> <the 4 voucherCounts after addModifier> <times ui.showText was called>
 *     <number of modifiers the scene holds afterwards>
 *   G <the 4 voucherCounts after doReward> <times ui.showText was called> <the first text shown, newlines as '|'>
 *   E
 * and a last line "# cases N", so a cut file can't pass. ui.showText is replaced by a recorder that calls the callback
 * it is given (doReward waits for it). An exception anywhere fails the producer.
 */
import { modifierTypes } from "#data/data-lists";
import { MoveId } from "#enums/move-id";
import { SpeciesId } from "#enums/species-id";
import { ModifierRewardPhase } from "#phases/modifier-reward-phase";
import { GameManager } from "#test/framework/game-manager";
import * as fs from "node:fs";
import Phaser from "phaser";
import { it, vi } from "vitest";

it("voucher apply producer", async () => {
  const N = Number(process.env.GC_VOUCHER_N ?? 2000);
  const OUT = process.env.GC_VOUCHER_OUT ?? "/tmp/gc-voucher-apply.txt";
  const game = new GameManager(new Phaser.Game({ type: Phaser.HEADLESS }));
  game.override.battleStyle("single").startingWave(5).enemySpecies(SpeciesId.MAGIKARP).enemyMoveset(MoveId.SPLASH);
  await game.classicMode.startBattle(SpeciesId.BULBASAUR);
  const scene = game.scene as any;

  const shown: string[] = [];
  vi.spyOn(scene.ui, "showText").mockImplementation((...args: any[]) => {
    shown.push(String(args[0]));
    if (typeof args[2] === "function") {
      args[2]();
    }
  });

  let seed = Number(process.env.GC_VOUCHER_SEED ?? 777);
  const rnd = (n: number) => {
    seed = (Math.imul(seed, 1103515245) + 12345) >>> 0;
    return (seed >>> 8) % n;
  };

  const keys = ["VOUCHER", "VOUCHER_PLUS", "VOUCHER_PREMIUM"];
  const counts = () => [0, 1, 2, 3].map(k => scene.gameData.voucherCounts[k]);

  const lines: string[] = [];
  for (let i = 0; i < N; i++) {
    const init: number[] = [];
    for (let k = 0; k < 4; k++) {
      init.push(rnd(3) === 0 ? [0, 1, 99, 999][rnd(4)] : rnd(60));
    }
    const itemKey = keys[i % keys.length];
    const typeFunc = (modifierTypes as any)[itemKey];
    const setCounts = () => init.forEach((c, k) => (scene.gameData.voucherCounts[k] = c));
    lines.push(["K", itemKey, ...init].join(" "));

    // taken from the reward screen
    setCounts();
    scene.modifiers.length = 0;
    shown.length = 0;
    const modifierType = typeFunc();
    const filterMsg: string | null = modifierType.selectFilter ? modifierType.selectFilter() : null;
    let applied = false;
    if (filterMsg === null) {
      applied = !!scene.addModifier(modifierType.newModifier(), false, false, undefined, undefined, -1);
    }
    lines.push(
      ["R", applied ? 1 : 0, filterMsg ? filterMsg.replace(/\s+/g, "_") : "OK", ...counts(), shown.length, scene.modifiers.length].join(" "),
    );

    // given by ModifierRewardPhase
    setCounts();
    scene.modifiers.length = 0;
    shown.length = 0;
    await new ModifierRewardPhase(typeFunc).doReward();
    lines.push(["G", ...counts(), shown.length, (shown[0] ?? "").replace(/\n/g, "|")].join(" "));
    lines.push("E");
  }

  scene.modifiers.length = 0;
  lines.push(`# cases ${N}`);
  fs.writeFileSync(OUT, lines.join("\n") + "\n");
}, 1_800_000);
