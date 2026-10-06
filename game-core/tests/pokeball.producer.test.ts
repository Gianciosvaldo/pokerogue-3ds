/*
 * Upstream half of game-core's Poke Ball differential (tools/applydiff.sh copies it into upstream).
 * Runs upstream's own classes, none of the rule rewritten here: modifierTypes.POKEBALL / GREAT_BALL / ULTRA_BALL / ROGUE_BALL /
 * MASTER_BALL (AddPokeballModifierType), their newModifier() (AddPokeballModifier) and, for the reward taken from the screen,
 * BattleScene.addModifier as SelectModifierPhase.applyModifier calls it; for the reward given directly,
 * ModifierRewardPhase.doReward. Both on random starting counts of every ball type, with 0, 94, 95, 98 and 99 (the cap's edges)
 * over-represented, and records:
 *   K <key> <the 5 starting counts>
 *   R <applied 0/1> <selectFilter's text or OK> <the 5 counts after addModifier> <times ui.showText was called>
 *   G <the 5 counts after doReward> <times ui.showText was called> <the first text shown, newlines written as '|'>
 *   E
 * ui.showText is replaced by a recorder that calls the callback it is given (doReward waits for it).
 */
import { modifierTypes } from "#data/data-lists";
import { MoveId } from "#enums/move-id";
import { SpeciesId } from "#enums/species-id";
import { ModifierRewardPhase } from "#phases/modifier-reward-phase";
import { GameManager } from "#test/framework/game-manager";
import * as fs from "node:fs";
import Phaser from "phaser";
import { it, vi } from "vitest";

it("pokeball apply producer", async () => {
  const N = Number(process.env.DIFF_N ?? 4000);
  const OUT = process.env.DIFF_OUT ?? "/tmp/gc-pokeball-cases.txt";
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

  let seed = Number(process.env.DIFF_SEED ?? 12345);
  const rnd = (n: number) => {
    seed = (Math.imul(seed, 1103515245) + 12345) >>> 0;
    return (seed >>> 8) % n;
  };

  const keys = ["POKEBALL", "GREAT_BALL", "ULTRA_BALL", "ROGUE_BALL", "MASTER_BALL"];
  const edgeCounts = [0, 94, 95, 98, 99];
  const counts = () => [0, 1, 2, 3, 4].map(k => scene.pokeballCounts[k]);

  const lines: string[] = [];
  let thrown = 0;

  for (let i = 0; i < N; i++) {
    try {
      // a count is 0-99 in a real run (the reward is not offered at 99 in Classic, but it can be given)
      const init: number[] = [];
      for (let k = 0; k < 5; k++) {
        init.push(rnd(3) === 0 ? edgeCounts[rnd(edgeCounts.length)] : rnd(100));
      }
      const itemKey = keys[i % keys.length];
      const typeFunc = (modifierTypes as any)[itemKey];
      const setCounts = () => init.forEach((c, k) => (scene.pokeballCounts[k] = c));
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
      lines.push(["R", applied ? 1 : 0, filterMsg ? filterMsg.replace(/\s+/g, "_") : "OK", ...counts(), shown.length].join(" "));

      // given by ModifierRewardPhase
      setCounts();
      scene.modifiers.length = 0;
      shown.length = 0;
      await new ModifierRewardPhase(typeFunc).doReward();
      lines.push(["G", ...counts(), shown.length, (shown[0] ?? "").replace(/\n/g, "|")].join(" "));
      lines.push("E");
    } catch (e) {
      thrown++;
      lines.push(`X ${String(e).slice(0, 120).replace(/\s+/g, "_")}`);
    }
  }

  scene.modifiers.length = 0;
  if (thrown) {
    fs.writeFileSync(OUT, lines.join("\n") + "\n");
    throw new Error(`${thrown} upstream pokeball cases threw`);
  }
  lines.push(`# threw: ${thrown}`);
  fs.writeFileSync(OUT, lines.join("\n") + "\n");
}, 1_800_000);
