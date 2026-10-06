/*
 * Upstream half of game-core's Pokeball differential (tools/applydiff.sh copies it into upstream).
 * Runs upstream's own AddPokeballModifier through BattleScene.addModifier, the way SelectModifierPhase
 * applies a reward, on various starting Pokeball counts (including 0, 94, 95, 98, 99), for each ball type,
 * and records the initial counts, whether applied, and final counts.
 */
import { modifierTypes } from "#data/data-lists";
import { MoveId } from "#enums/move-id";
import { SpeciesId } from "#enums/species-id";
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

  let seed = Number(process.env.DIFF_SEED ?? 12345);
  const rnd = (n: number) => {
    seed = (Math.imul(seed, 1103515245) + 12345) >>> 0;
    return (seed >>> 8) % n;
  };

  const keys = ["POKEBALL", "GREAT_BALL", "ULTRA_BALL", "ROGUE_BALL", "MASTER_BALL"];
  const edgeCounts = [0, 94, 95, 98, 99];

  const lines: string[] = [];
  let thrown = 0;

  for (let i = 0; i < N; i++) {
    try {
      // Set starting counts covering edge counts 0, 94, 95, 98, 99 and randoms
      const initCounts: number[] = [];
      for (let k = 0; k < 5; k++) {
        const c = rnd(3) === 0 ? edgeCounts[rnd(edgeCounts.length)] : rnd(105);
        scene.pokeballCounts[k] = c;
        initCounts.push(c);
      }

      const itemKey = keys[i % keys.length];
      const modifierType = (modifierTypes as any)[itemKey]();
      const filterMsg: string | null = modifierType.selectFilter ? modifierType.selectFilter() : null;

      lines.push(["K", itemKey, ...initCounts].join(" "));

      scene.modifiers.length = 0;
      let applied = false;
      if (filterMsg === null) {
        const mod = modifierType.newModifier();
        applied = !!scene.addModifier(mod);
      }

      const finalCounts = [0, 1, 2, 3, 4].map(k => scene.pokeballCounts[k]);
      lines.push(["R", applied ? 1 : 0, filterMsg ? filterMsg.replace(/\s+/g, "_") : "OK", ...finalCounts].join(" "));
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
