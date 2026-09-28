/*
 * Upstream half of run-core's endless enemy buff test (tools/difftest.sh copies it into the upstream
 * checkout). Each case plays the buff waves of an endless run (every 50th wave, from 50 up to a random
 * last wave, up to 5,000) on one freshly sown Phaser.Math.RND, so the buffs carry over from wave to wave,
 * and records the buffs held (type and stack) after each wave and the RNG state at the end. The phase's
 * own lines (AddEnemyBuffModifierPhase.start, which would drive the phase queue) are transcribed:
 *   tier = waveIndex % 1000 ? (waveIndex % 250 ? COMMON : GREAT) : ULTRA;
 *   regenerateModifierPoolThresholds(getEnemyParty(), ENEMY_BUFF);
 *   for (ceil(waveIndex / 250)) addEnemyModifier(getEnemyBuffModifierForWave(tier, enemy persistent modifiers), true, true)
 * build/enemybuffstest replays the cases.
 */
import { getGameMode } from "#app/game-mode";
import { GameModes } from "#enums/game-modes";
import { ModifierPoolType } from "#enums/modifier-pool-type";
import { ModifierTier } from "#enums/modifier-tier";
import { MoveId } from "#enums/move-id";
import { SpeciesId } from "#enums/species-id";
import { EnemyPersistentModifier } from "#modifiers/modifier";
import { getEnemyBuffModifierForWave, regenerateModifierPoolThresholds } from "#modifiers/modifier-type";
import { GameManager } from "#test/framework/game-manager";
import * as fs from "node:fs";
import Phaser from "phaser";
import { it } from "vitest";

it("enemybuffs producer", async () => {
  const N = Number(process.env.DIFF_N ?? 2000);
  const OUT = process.env.DIFF_OUT ?? "/tmp/pr-enemybuffs-cases.txt";
  const game = new GameManager(new Phaser.Game({ type: Phaser.HEADLESS }));
  game.override.battleStyle("single").startingWave(5).enemySpecies(SpeciesId.MAGIKARP).enemyMoveset(MoveId.SPLASH);
  await game.classicMode.startBattle(SpeciesId.BULBASAUR);
  const scene = game.scene as any;
  const battle = scene.currentBattle as any;
  scene.gameMode = getGameMode(GameModes.ENDLESS);
  battle.gameMode = scene.gameMode;

  let seed = Number(process.env.DIFF_SEED ?? 12345);
  const rnd = (n: number) => {
    seed = (Math.imul(seed, 1103515245) + 12345) >>> 0;
    return (seed >>> 8) % n;
  };
  const chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
  const seedStr = () => Array.from({ length: 24 }, () => chars[rnd(chars.length)]).join("");
  const state = () => Phaser.Math.RND.state().split(",").slice(1).join(" ");

  const lines: string[] = [];
  let thrown = 0;
  for (let i = 0; i < N; i++) {
    scene.enemyModifiers.length = 0;
    const last = 50 * (1 + rnd(rnd(4) === 0 ? 100 : 30));
    Phaser.Math.RND.sow([seedStr()]);
    lines.push(["B", last, state()].join(" "));
    try {
      for (let wave = 50; wave <= last; wave += 50) {
        battle.waveIndex = wave;
        const tier = wave % 1000 ? (wave % 250 ? ModifierTier.COMMON : ModifierTier.GREAT) : ModifierTier.ULTRA;
        regenerateModifierPoolThresholds(scene.getEnemyParty(), ModifierPoolType.ENEMY_BUFF);
        const count = Math.ceil(wave / 250);
        for (let k = 0; k < count; k++) {
          scene.addEnemyModifier(
            getEnemyBuffModifierForWave(
              tier,
              scene.findModifiers((m: any) => m instanceof EnemyPersistentModifier, false),
            ),
            true,
            true,
          );
        }
        const held = scene.enemyModifiers.filter((m: any) => m instanceof EnemyPersistentModifier);
        lines.push(["W", wave, held.length, ...held.flatMap((m: any) => [m.type.id, m.stackCount])].join(" "));
      }
      lines.push(`E ${state()}`);
    } catch (e) {
      thrown++;
      lines.push(`X ${String(e).slice(0, 60).replace(/\s+/g, "_")}`);
    }
  }
  scene.enemyModifiers.length = 0;
  lines.push(`# threw: ${thrown}`);
  fs.writeFileSync(OUT, lines.join("\n") + "\n");
}, 1_800_000);
