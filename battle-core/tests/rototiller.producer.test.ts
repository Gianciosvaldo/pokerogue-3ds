/* Pinned-source observations for Rototiller's party-wide move condition. */
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

it("pinned Rototiller party condition cases", async () => {
  /* Keep the production RNG implementation: GameManager patches it while it
   * constructs and starts the battle, so capture before construction. */
  const nativeRandBattleSeedInt = BattleScene.prototype.randBattleSeedInt;
  const game = new GameManager(new Phaser.Game({ type: Phaser.HEADLESS }));
  game.override.battleStyle("double").disableTrainerWaves().mysteryEncounterChance(0);
  await game.classicMode.startBattle(SpeciesId.RATTATA, SpeciesId.RATTATA);
  BattleScene.prototype.randBattleSeedInt = nativeRandBattleSeedInt;

  const scene: any = game.scene;
  const players: any[] = [...scene.getPlayerParty()];
  const enemies: any[] = [...scene.getEnemyParty()];
  while (players.length < 6) players.push(scene.addPlayerPokemon(speciesDataRegistry.getSpecies(SpeciesId.RATTATA), 30));
  while (enemies.length < 6) enemies.push(scene.addEnemyPokemon(speciesDataRegistry.getSpecies(SpeciesId.RATTATA), 30, TrainerSlot.NONE));
  const setSpecies = (p: any, speciesId: SpeciesId, fainted = false) => {
    p.species = speciesDataRegistry.getSpecies(speciesId);
    p.formIndex = 0;
    p.calculateStats();
    p.hp = fainted ? 0 : p.getMaxHp();
  };
  const reset = () => {
    for (const p of [...players, ...enemies]) setSpecies(p, SpeciesId.RATTATA);
    scene.getPlayerParty().splice(0, scene.getPlayerParty().length, ...players);
    scene.getEnemyParty().splice(0, scene.getEnemyParty().length, ...enemies);
  };
  const rows = ["ROTOTILLER 1 7"];
  const observe = (name: string, setup: () => void) => {
    reset();
    setup();
    const user = players[0];
    const target = enemies[0];
    const allowed = allMoves[MoveId.ROTOTILLER].applyConditions(user, target, 4);
    const p = scene.getPlayerParty() as any[];
    const e = scene.getEnemyParty() as any[];
    const species = [...p, ...e].map(x => x.species.speciesId);
    const hp = [...p, ...e].map(x => x.hp > 0 ? 1 : 0);
    rows.push(["CASE", name, p.length, e.length, ...species, ...hp, +allowed].join(" "));
  };

  observe("no-grass", () => {});
  observe("field-grass", () => setSpecies(players[0], SpeciesId.BULBASAUR));
  observe("player-bench-grass", () => setSpecies(players[2], SpeciesId.BULBASAUR));
  observe("fainted-player-bench-grass", () => setSpecies(players[2], SpeciesId.BULBASAUR, true));
  observe("enemy-bench-grass", () => setSpecies(enemies[4], SpeciesId.BULBASAUR));
  observe("airborne-grass-only", () => {
    setSpecies(players[2], SpeciesId.JUMPLUFF);
    setSpecies(enemies[4], SpeciesId.JUMPLUFF);
  });
  observe("empty-parties", () => {
    scene.getPlayerParty().splice(0, scene.getPlayerParty().length);
    scene.getEnemyParty().splice(0, scene.getEnemyParty().length);
  });
  rows.push("ROTOTILLER_COMPLETE 7");
  fs.writeFileSync(process.env.ROTOTILLER_OUT!, `${rows.join("\n")}\n`);
});
