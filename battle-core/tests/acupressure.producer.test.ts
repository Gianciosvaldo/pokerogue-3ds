/* Actual pinned Acupressure override; queue and RNG observations, no expected rolls. */
import { BattleScene } from "#app/battle-scene";
import { speciesDataRegistry } from "#app/globals/global-species-data-registry";
import { SpeciesFormChangePostMoveTrigger } from "#data/form-change-triggers";
import { allMoves } from "#data/data-lists";
import { AbilityId } from "#enums/ability-id";
import { SpeciesId } from "#enums/species-id";
import { MoveId } from "#enums/move-id";
import { BATTLE_STATS } from "#enums/stat";
import { GameManager } from "#test/framework/game-manager";
import Phaser from "phaser";
import * as fs from "node:fs";
import { it, vi } from "vitest";

it("native Acupressure override queues", async () => {
  const n = Number(process.env.ACUPRESSURE_N ?? 16384);
  if (!process.versions.node.startsWith("22.") || !Number.isSafeInteger(n) || n < 16384) throw Error("Node22 and N>=16384 required");
  const nativeBattleRoll = BattleScene.prototype.randBattleSeedInt;
  const game = new GameManager(new Phaser.Game({ type: Phaser.HEADLESS }));
  game.override.battleStyle("double").ability(AbilityId.BALL_FETCH).enemyAbility(AbilityId.BALL_FETCH)
    .enemySpecies(SpeciesId.MAGIKARP);
  await game.classicMode.startBattle(SpeciesId.BULBASAUR, SpeciesId.CHARMANDER);
  BattleScene.prototype.randBattleSeedInt = nativeBattleRoll;
  const scene: any = game.scene, pm = scene.phaseManager;
  const actors: any[] = [...scene.getPlayerField(), ...scene.getEnemyField()];
  if (actors.length !== 4 || actors.some(p => !p)) throw Error("natural double field required");
  game.override.ability(AbilityId.NONE).enemyAbility(AbilityId.NONE);
  vi.spyOn(pm, "shiftPhase").mockImplementation(() => {});
  const attr = allMoves[MoveId.ACUPRESSURE].getAttrs("AcupressureStatStageChangeAttr")[0];
  const rows = [`ACUPRESSURE 1 ${n}`];
  const rng = (prefix: string, state: string) => `${prefix} ${state.replace("!rnd,", "").replaceAll(",", " ")}`;
  for (let i = 0; i < n; i++) {
    const mask = i % 128, user = Math.floor(i / 128) % 4, target = Math.floor(i / 512) % 4;
    const move = [MoveId.ACUPRESSURE, MoveId.EMBER, MoveId.THUNDER, MoveId.TACKLE][Math.floor(i / 4096) % 4];
    for (let k = 0; k < 4; k++) {
      const p = actors[k]; p.resetSummonData(); p.resetTurnData();
      p.hp = ((Math.floor(i / 2048) >> (k % 2)) & 1) ? 0 : p.getMaxHp();
      p.customPokemonData.ability = AbilityId.NONE;
      p.customPokemonData.passive = AbilityId.NONE; p.passive = false;
      for (let j = 0; j < 7; j++) p.summonData.statStages[j] = mask & (1 << j) ? 6 : [-6, 0, 4, 5][(i + j) % 4];
    }
    pm.clearAllPhases(); scene.modifiers = []; scene.enemyModifiers = [];
    Phaser.Math.RND.sow([`acupressure-${i}`]);
    scene.currentBattle.battleSeedState = null; scene.currentBattle.randSeedInt(2);
    rows.push(`C ${i} ${user} ${target} ${move}`);
    for (const p of actors) rows.push(`MON ${p.hp} ${BATTLE_STATS.map(s => p.getStatStage(s)).join(" ")}`);
    rows.push(rng("R", Phaser.Math.RND.state()), rng("B", scene.currentBattle.battleSeedState));
    const result = attr.apply(actors[user], actors[target], allMoves[move], []);
    rows.push(`RESULT ${+result}`, `LEVELS ${pm.phaseQueue.levels.length}`);
    for (const [l, queue] of pm.phaseQueue.levels.entries()) {
      rows.push(`L ${l} ${queue.length}`);
      for (const ph of queue) {
        if (ph.phaseName !== "DynamicPhaseMarker" || ph.phaseType !== "StatStageChangePhase") throw Error("unexpected marker");
        rows.push("MARKER StatStageChangePhase");
      }
    }
    const dynamic: any[] = [...pm.dynamicQueueManager.dynamicPhaseMap.values()].flatMap((q: any) => q.queue);
    rows.push(`DYNAMIC ${dynamic.length}`);
    for (const ph of dynamic) {
      if (ph.phaseName !== "StatStageChangePhase" || ph.options.changes.length !== 1) throw Error("unexpected dynamic phase");
      const change = ph.options.changes[0];
      rows.push(`PH ${ph.options.battlerIndex} ${actors.indexOf(ph.options.sourcePokemon)} ${change.stat} ${change.stages} ${+ph.selfTarget}`);
    }
    for (const p of actors) rows.push(`AFTER ${p.hp} ${BATTLE_STATS.map(s => p.getStatStage(s)).join(" ")}`);
    rows.push(rng("RO", Phaser.Math.RND.state()), rng("BO", scene.currentBattle.battleSeedState));
  }
  // A retained owning turn switches to Meloetta with no Relic Song history.
  // Capture native negative predicates plus positive controls, without executing
  // the positive transition (which remains an explicit C boundary).
  rows.push("MELOETTA 40");
  for (let i = 0; i < 40; i++) {
    const side = Math.floor(i / 20), form = Math.floor(i / 10) % 2;
    const move = [-1, MoveId.TACKLE, MoveId.ACUPRESSURE, MoveId.SPLASH, MoveId.RELIC_SONG][Math.floor(i / 2) % 5];
    const p = actors[side * 2]; p.resetSummonData();
    p.species = speciesDataRegistry.getSpecies(SpeciesId.MELOETTA); p.formIndex = form;
    p.hp = i % 2 ? 1 : 0;
    if (move >= 0) p.summonData.moveHistory.push({ move, targets: [side * 2], useMode: 0, result: 0 });
    pm.clearAllPhases();
    const before = [Phaser.Math.RND.state(), scene.currentBattle.battleSeedState];
    const result = scene.triggerPokemonFormChange(p, SpeciesFormChangePostMoveTrigger);
    const queue = pm.phaseQueue.levels.flat();
    if (queue.some((ph: any) => ph.phaseName !== "QuietFormChangePhase")
      || before[0] !== Phaser.Math.RND.state() || before[1] !== scene.currentBattle.battleSeedState) throw Error("unexpected Meloetta predicate side effect");
    rows.push(`MNOOP ${i} ${side} ${form} ${p.hp} ${move} ${+result} ${queue.length}`);
  }
  rows.push(`ACUPRESSURE_COMPLETE ${n}`);
  fs.writeFileSync(process.env.ACUPRESSURE_OUT ?? "/tmp/acupressure.txt", rows.join("\n") + "\n");
}, 600000);
