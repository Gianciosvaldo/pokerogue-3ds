/* Real AfterYouAttr and native dynamic queue API, including duplicate owners. */
import { BattleScene } from "#app/battle-scene";
import { allMoves } from "#data/data-lists";
import { AbilityId } from "#enums/ability-id";
import { SpeciesId } from "#enums/species-id";
import { MoveId } from "#enums/move-id";
import { MoveUseMode } from "#enums/move-use-mode";
import { PokemonMove } from "#moves/pokemon-move";
import { GameManager } from "#test/framework/game-manager";
import Phaser from "phaser";
import * as fs from "node:fs";
import { it, vi } from "vitest";

it("actual After You queue and message API", async () => {
  const n = Number(process.env.AFTERYOU_N ?? 8192);
  if (!process.versions.node.startsWith("22.") || !Number.isSafeInteger(n) || n < 8192) throw Error("Node22 and N>=8192 required");
  const nativeBattleRoll = BattleScene.prototype.randBattleSeedInt;
  const game = new GameManager(new Phaser.Game({ type: Phaser.HEADLESS }));
  game.override.battleStyle("double").ability(AbilityId.BALL_FETCH).enemyAbility(AbilityId.BALL_FETCH).enemySpecies(SpeciesId.MAGIKARP);
  await game.classicMode.startBattle(SpeciesId.BULBASAUR, SpeciesId.CHARMANDER);
  BattleScene.prototype.randBattleSeedInt = nativeBattleRoll;
  const s: any = game.scene, pm = s.phaseManager;
  const actors: any[] = [...s.getPlayerField(), ...s.getEnemyField()];
  if (actors.length !== 4 || actors.some(p => !p)) throw Error("natural double field required");
  vi.spyOn(pm, "shiftPhase").mockImplementation(() => {});
  const attr = allMoves[MoveId.AFTER_YOU].getAttrs("AfterYouAttr")[0];
  const rows = [`AFTERYOU 1 ${n}`];
  const rng = (key: string, state: string) => `${key} ${state.replace("!rnd,", "").replaceAll(",", " ")}`;
  for (let i = 0; i < n; i++) {
    pm.clearAllPhases(); pm.phaseQueue.deferredActive = false;
    s.getPlayerParty().splice(0, 2, actors[0], actors[1]);
    s.getEnemyParty().splice(0, 2, actors[2], actors[3]);
    const user = Math.floor(i / 4) % 4, target = i % 4, count = Math.floor(i / 16) % 9, ops = 1 + Math.floor(i / 576) % 3;
    const swap = Math.floor(i / 1728) % 4;
    for (let k = 0; k < 4; k++) {
      const p = actors[k]; p.resetSummonData(); p.resetTurnData();
      p.hp = (Math.floor(i / 144) + k) % 2 ? 0 : p.getMaxHp();
      p.turnData.acted = (Math.floor(i / 288) + k) % 2 === 1;
    }
    Phaser.Math.RND.sow([`after-you-${i}`]); s.currentBattle.battleSeedState = null; s.currentBattle.randSeedInt(2);
    rows.push(`C ${i} ${user} ${target} ${count} ${ops} ${swap}`);
    for (const p of actors) rows.push(`MON ${p.species.speciesId} ${p.formIndex} ${p.hp} ${+p.turnData.acted}`);
    for (let j = 0; j < count; j++) {
      const who = (i + j * 3) % 4, timing = (i + j) % 3;
      pm.pushNew("MovePhase", actors[who], [target], new PokemonMove(MoveId.TACKLE), MoveUseMode.NORMAL, timing);
      rows.push(`MOVE ${who} ${timing}`);
    }
    if (swap & 1) s.getPlayerParty().splice(0, 2, actors[1], actors[0]);
    if (swap & 2) s.getEnemyParty().splice(0, 2, actors[3], actors[2]);
    rows.push(rng("R", Phaser.Math.RND.state()), rng("B", s.currentBattle.battleSeedState));
    for (let op = 0; op < ops; op++) {
      const to = (target + op % 2) % 4;
      const result = attr.apply(actors[user], actors[to], allMoves[MoveId.AFTER_YOU], []);
      rows.push(`OP ${to} ${+result}`, `LEVELS ${pm.phaseQueue.levels.length}`);
      for (const [l, queue] of pm.phaseQueue.levels.entries()) {
        rows.push(`L ${l} ${queue.length}`);
        for (const ph of queue) {
          if (ph.phaseName === "DynamicPhaseMarker" && ph.phaseType === "MovePhase") rows.push("MARKER MovePhase");
          else if (ph.phaseName === "MessagePhase") rows.push(`MSG ${Buffer.from(ph.text).toString("hex")}`);
          else throw Error("unexpected phase");
        }
      }
      const dynamic: any[] = [...pm.dynamicQueueManager.dynamicPhaseMap.values()].flatMap((q: any) => q.queue);
      rows.push(`DYNAMIC ${dynamic.length}`);
      for (const ph of dynamic) {
        if (ph.phaseName !== "MovePhase") throw Error("unexpected dynamic phase");
        rows.push(`D ${actors.indexOf(ph.pokemon)} ${ph.timingModifier}`);
      }
    }
    rows.push(rng("RO", Phaser.Math.RND.state()), rng("BO", s.currentBattle.battleSeedState));
    for (const p of actors) rows.push(`AFTER ${p.hp} ${+p.turnData.acted}`);
  }
  rows.push(`AFTERYOU_COMPLETE ${n}`);
  fs.writeFileSync(process.env.AFTERYOU_OUT ?? "/tmp/after-you.txt", rows.join("\n") + "\n");
}, 600000);
