/* Pinned source observations for variable multi-target moves in doubles. */
import { BattleScene } from "#app/battle-scene";
import { Terrain } from "#data/terrain";
import { Command } from "#enums/command";
import { SpeciesId } from "#enums/species-id";
import { TerrainType } from "#enums/terrain-type";
import { MoveId } from "#enums/move-id";
import { speciesDataRegistry } from "#app/globals/global-species-data-registry";
import { getMoveTargets } from "#data/moves/move-utils";
import { GameManager } from "#test/framework/game-manager";
import Phaser from "phaser";
import * as fs from "node:fs";
import { it } from "vitest";

it("pinned variable-target doubles cases", async () => {
  const nativeRandBattleSeedInt = BattleScene.prototype.randBattleSeedInt;
  const game = new GameManager(new Phaser.Game({ type: Phaser.HEADLESS }));
  game.override.battleStyle("double").enemySpecies(SpeciesId.RATTATA).disableTrainerWaves().mysteryEncounterChance(0);
  await game.classicMode.startBattle(SpeciesId.RATTATA, SpeciesId.RATTATA);
  /* GameManager patches this method while it starts a test battle. Restore the
   * native source implementation before observing target selection. */
  BattleScene.prototype.randBattleSeedInt = nativeRandBattleSeedInt;

  const scene: any = game.scene;
  const battle: any = scene.currentBattle;
  const slots: any[] = scene.getField();
  if (slots.length !== 4 || slots.some(p => !p)) throw new Error("expected natural doubles field");
  const rows = ["VARIABLE_TARGET 1 5"];
  const observe = (move: MoveId, terrain: TerrainType, speciesId: SpeciesId, tera: boolean, command: Command) => {
    const user = slots[0];
    user.species = speciesDataRegistry.getSpecies(speciesId);
    user.formIndex = 0;
    user.isTerastallized = tera;
    user.calculateStats();
    user.hp = user.getMaxHp();
    slots.slice(1).forEach(p => { p.hp = p.getMaxHp(); });
    scene.arena.terrain = terrain === TerrainType.NONE ? null : new Terrain(terrain, 5);
    battle.preTurnCommands[0] = { command };
    const result = getMoveTargets(user, move);
    rows.push(`CASE ${move} ${terrain} ${speciesId} ${+tera} ${command} ${result.targets.length} ${+result.multiple} ${[...result.targets, -1, -1, -1, -1].slice(0, 4).join(" ")}`);
  };

  observe(MoveId.EXPANDING_FORCE, TerrainType.NONE, SpeciesId.PIKACHU, false, Command.FIGHT);
  observe(MoveId.EXPANDING_FORCE, TerrainType.PSYCHIC, SpeciesId.PIKACHU, false, Command.FIGHT);
  observe(MoveId.TERA_STARSTORM, TerrainType.NONE, SpeciesId.TERAPAGOS, false, Command.FIGHT);
  observe(MoveId.TERA_STARSTORM, TerrainType.NONE, SpeciesId.TERAPAGOS, true, Command.FIGHT);
  observe(MoveId.TERA_STARSTORM, TerrainType.NONE, SpeciesId.TERAPAGOS, false, Command.TERA);
  fs.writeFileSync(process.env.VARIABLE_TARGET_OUT!, `${rows.join("\n")}\n`);
});
