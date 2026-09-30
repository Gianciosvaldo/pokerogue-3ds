/*
 * Upstream half of reward-core's victory reward test (tools/difftest.sh copies it into the upstream checkout).
 * Each case sets a random game mode, wave, offsetGym and battle type, empties the enemy party (the side is
 * beaten) and runs VictoryPhase.start() with the phase queue stubbed: it records the ModifierRewardPhase items in
 * the order they're queued and whether a SelectModifierPhase follows. Timed events are off (no fixed event
 * rewards), as the port assumes. build/victorytest replays the cases.
 */
import { timedEventManager } from "#app/global-event-manager";
import { getGameMode } from "#app/game-mode";
import { modifierTypes } from "#data/data-lists";
import { BattleType } from "#enums/battle-type";
import { BattlerIndex } from "#enums/battler-index";
import { GameModes } from "#enums/game-modes";
import { MoveId } from "#enums/move-id";
import { SpeciesId } from "#enums/species-id";
import { VictoryPhase } from "#phases/victory-phase";
import { GameManager } from "#test/framework/game-manager";
import * as fs from "node:fs";
import Phaser from "phaser";
import { it, vi } from "vitest";

it("victory producer", async () => {
  const N = Number(process.env.DIFF_N ?? 20000);
  const OUT = process.env.DIFF_OUT ?? "/tmp/pr-victory-cases.txt";
  const game = new GameManager(new Phaser.Game({ type: Phaser.HEADLESS }));
  game.override.battleStyle("single").startingWave(5).enemySpecies(SpeciesId.MAGIKARP).enemyMoveset(MoveId.SPLASH);
  await game.classicMode.startBattle(SpeciesId.BULBASAUR);
  const scene = game.scene as any;
  const battle = scene.currentBattle as any;
  vi.spyOn(timedEventManager, "getFixedBattleEventRewards").mockReturnValue([]);
  vi.spyOn(scene, "getEnemyParty").mockReturnValue([]);
  vi.spyOn(scene, "applyPartyExp").mockImplementation(() => {});
  let queued: [string, any][] = [];
  vi.spyOn(scene.phaseManager, "pushNew").mockImplementation((...args: any[]) => {
    queued.push([args[0], args[1]]);
  });
  vi.spyOn(scene.phaseManager, "hasPhaseOfType").mockReturnValue(false);
  const keyOf = (fn: any) => Object.keys(modifierTypes).find(k => (modifierTypes as any)[k] === fn) ?? "?";

  let seed = Number(process.env.DIFF_SEED ?? 12345);
  const rnd = (n: number) => {
    seed = (Math.imul(seed, 1103515245) + 12345) >>> 0;
    return (seed >>> 8) % n;
  };
  const modes = [GameModes.CLASSIC, GameModes.ENDLESS, GameModes.SPLICED_ENDLESS, GameModes.DAILY, GameModes.CHALLENGE];
  const hot = [10, 20, 30, 50, 60, 90, 100, 150, 165, 200, 250, 270, 500, 510, 520, 750, 760, 1000, 1250];
  const lines: string[] = [];
  let thrown = 0;
  for (let i = 0; i < N; i++) {
    const gm = getGameMode(modes[rnd(modes.length)]);
    scene.gameMode = gm;
    battle.gameMode = gm;
    const wave = rnd(3) === 0 ? hot[rnd(hot.length)] : rnd(3) === 0 ? 1 + rnd(1300) : 1 + rnd(210);
    battle.waveIndex = wave;
    battle.battleType = rnd(2) ? BattleType.TRAINER : BattleType.WILD;
    scene.offsetGym = rnd(2) === 0;
    queued = [];
    try {
      const phase: any = new VictoryPhase(BattlerIndex.ENEMY);
      phase.end = () => {};
      phase.getPokemon = () => ({ getExpValue: () => 0 });
      phase.start();
      const rewards = queued.filter(q => q[0] === "ModifierRewardPhase").map(q => keyOf(q[1]));
      const select = queued.some(q => q[0] === "SelectModifierPhase");
      lines.push(["V", gm.modeId, wave, scene.offsetGym ? 1 : 0, select ? 1 : 0, rewards.length, ...rewards].join(" "));
    } catch (e) {
      thrown++;
      lines.push(`X ${String(e).slice(0, 80).replace(/\s+/g, "_")}`);
    }
  }
  lines.push(`# threw: ${thrown}`);
  fs.writeFileSync(OUT, lines.join("\n") + "\n");
}, 900_000);
