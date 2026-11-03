/* Pinned-source observations for Healing Wish/Lunar Dance party eligibility. */
import { BattleScene } from "#app/battle-scene";
import { speciesDataRegistry } from "#app/globals/global-species-data-registry";
import { allMoves } from "#data/data-lists";
import { MoveId } from "#enums/move-id";
import { SpeciesId } from "#enums/species-id";
import { TrainerSlot } from "#enums/trainer-slot";
import { GameManager } from "#test/framework/game-manager";
import Phaser from "phaser";
import * as fs from "node:fs";
import { it } from "vitest";

it("pinned Healing Wish and Lunar Dance party eligibility cases", async () => {
  const nativeRandBattleSeedInt = BattleScene.prototype.randBattleSeedInt;
  const game = new GameManager(new Phaser.Game({ type: Phaser.HEADLESS }));
  game.override.battleStyle("double").disableTrainerWaves().mysteryEncounterChance(0);
  await game.classicMode.startBattle(SpeciesId.RATTATA, SpeciesId.RATTATA);
  BattleScene.prototype.randBattleSeedInt = nativeRandBattleSeedInt;

  const scene: any = game.scene;
  const players: any[] = [...scene.getPlayerParty()];
  const enemies: any[] = [...scene.getEnemyParty()];
  while (players.length < 3) players.push(scene.addPlayerPokemon(speciesDataRegistry.getSpecies(SpeciesId.RATTATA), 30));
  while (enemies.length < 3) enemies.push(scene.addEnemyPokemon(speciesDataRegistry.getSpecies(SpeciesId.RATTATA), 30, TrainerSlot.NONE));
  const reset = () => {
    for (const p of [...players, ...enemies]) {
      p.species = speciesDataRegistry.getSpecies(SpeciesId.RATTATA);
      p.formIndex = 0;
      p.calculateStats();
      p.hp = p.getMaxHp();
    }
    scene.getPlayerParty().splice(0, scene.getPlayerParty().length, ...players);
    scene.getEnemyParty().splice(0, scene.getEnemyParty().length, ...enemies);
  };
  const rows = ["HEALING_WISH_PARTY 1 6"];
  const observe = (name: string, move: MoveId, userSide: 0 | 1,
                   playerMembers: Array<[boolean, boolean]>, enemyMembers: Array<[boolean, boolean]>) => {
    reset();
    scene.getPlayerParty().splice(0, scene.getPlayerParty().length, ...playerMembers.map((_, i) => players[i]));
    scene.getEnemyParty().splice(0, scene.getEnemyParty().length, ...enemyMembers.map((_, i) => enemies[i]));
    playerMembers.forEach(([active], i) => { players[i].hp = active ? players[i].getMaxHp() : 0; });
    enemyMembers.forEach(([active], i) => { enemies[i].hp = active ? enemies[i].getMaxHp() : 0; });
    const user = userSide === 0 ? players[0] : enemies[0];
    const target = userSide === 0 ? enemies[0] : players[0];
    const moveData = allMoves[move];
    const conditionResults = moveData.conditions.map((c: any) => +c.apply(user, target, moveData));
    const allowed = moveData.applyConditions(user, target, 4);
    const states = [
      ...scene.getPlayerParty().flatMap((p: any) => [+p.isActive(), +p.isOnField()]),
      ...scene.getEnemyParty().flatMap((p: any) => [+p.isActive(), +p.isOnField()]),
    ];
    rows.push(["CASE", name, move, userSide, playerMembers.length, enemyMembers.length,
      ...states, conditionResults.length, ...conditionResults, +allowed].join(" "));
  };

  const pair = (active: boolean, onField: boolean): [boolean, boolean] => [active, onField];
  const twoField = [pair(true, true), pair(true, true)];
  observe("ordinary-player-bench", MoveId.HEALING_WISH, 0, [...twoField, pair(true, false)], twoField);
  observe("no-player-bench", MoveId.LUNAR_DANCE, 0, twoField, twoField);
  observe("fainted-field-and-living-bench", MoveId.HEALING_WISH, 0,
    [pair(true, true), pair(false, true), pair(true, false)], twoField);
  observe("fainted-bench-only", MoveId.LUNAR_DANCE, 0,
    [...twoField, pair(false, false)], twoField);
  observe("enemy-bench-does-not-help-player", MoveId.HEALING_WISH, 0,
    twoField, [...twoField, pair(true, false)]);
  observe("player-bench-does-not-help-enemy", MoveId.LUNAR_DANCE, 1,
    [...twoField, pair(true, false)], twoField);
  rows.push("HEALING_WISH_PARTY_COMPLETE 6");
  fs.writeFileSync(process.env.HEALING_WISH_PARTY_OUT!, `${rows.join("\n")}\n`);
});
