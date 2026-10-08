/* Additive persistence boundary proof: real ModifierData reconstruction omits
 * virtual stacks. Valid holders only; not whole upstream save/session parity. */
import { modifierTypes } from "#data/data-lists";
import { AbilityId } from "#enums/ability-id";
import { MoveId } from "#enums/move-id";
import { SpeciesId } from "#enums/species-id";
import { PokemonMoveAccuracyBoosterModifier } from "#modifiers/modifier";
import { ModifierData } from "#system/modifier-data";
import { GameManager } from "#test/framework/game-manager";
import Phaser from "phaser";
import * as fs from "node:fs";
import { expect, it } from "vitest";
it("Wide Lens real-stack storage boundary",async()=>{
 expect(process.versions.node.split(".")[0]).toBe("22");
 const game=new GameManager(new Phaser.Game({type:Phaser.HEADLESS}));
 game.override.battleStyle("single").startingWave(5).ability(AbilityId.NONE)
  .enemySpecies(SpeciesId.MAGIKARP).enemyMoveset(MoveId.SPLASH);
 await game.classicMode.startBattle(SpeciesId.BULBASAUR);
 const scene=game.scene as any,lines=["WIDE_STORAGE 1 6"];
 for(let side=0;side<2;side++)for(let stack=1;stack<=3;stack++){
  const p=side?scene.getEnemyParty()[0]:scene.getPlayerParty()[0];
  const t=modifierTypes.WIDE_LENS().withIdFromFunc(modifierTypes.WIDE_LENS),m=t.newModifier(p) as PokemonMoveAccuracyBoosterModifier;
  m.stackCount=stack;m.virtualStackCount=stack===3?0:1;
  const data=new ModifierData(m,!side),serialized=JSON.stringify(data);
  expect(serialized).not.toContain("virtualStackCount");
  const restored=new ModifierData(JSON.parse(serialized),!side).toModifier(PokemonMoveAccuracyBoosterModifier) as PokemonMoveAccuracyBoosterModifier;
  expect(restored).not.toBeNull();expect(restored.pokemonId).toBe(p.id);expect(restored.stackCount).toBe(stack);expect(restored.virtualStackCount).toBe(0);
  const before={value:0},after={value:0};m.apply(p,before as any);restored.apply(p,after as any);
  expect(before.value).toBe(5*(stack+m.virtualStackCount));expect(after.value).toBe(5*stack);
  lines.push(`P ${side} ${stack} ${m.virtualStackCount} ${restored.stackCount} ${restored.virtualStackCount} ${before.value} ${after.value}`);
 }
 lines.push("Z 6");fs.writeFileSync(process.env.GC_LENS_STORAGE_OUT??"/tmp/lens-storage.txt",lines.join("\n")+"\n");
 console.log("Wide Lens storage: 6 valid-holder source reconstructions, virtual stacks omitted/reset, amounts identical");
},120000);
