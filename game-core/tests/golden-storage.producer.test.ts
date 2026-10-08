/* Additive persistence boundary proof: real ModifierData reconstruction omits
 * virtual stacks. Valid holders only; not whole upstream save/session parity. */
import { modifierTypes } from "#data/data-lists";
import { AbilityId } from "#enums/ability-id";
import { MoveId } from "#enums/move-id";
import { SpeciesId } from "#enums/species-id";
import { DamageMoneyRewardModifier } from "#modifiers/modifier";
import { ModifierData } from "#system/modifier-data";
import { GameManager } from "#test/framework/game-manager";
import Phaser from "phaser";
import * as fs from "node:fs";
import { expect, it } from "vitest";
it("Golden Punch real-stack storage boundary",async()=>{
 expect(process.versions.node.split(".")[0]).toBe("22");
 const game=new GameManager(new Phaser.Game({type:Phaser.HEADLESS}));
 game.override.battleStyle("single").startingWave(5).ability(AbilityId.NONE)
  .enemySpecies(SpeciesId.MAGIKARP).enemyMoveset(MoveId.SPLASH);
 await game.classicMode.startBattle(SpeciesId.BULBASAUR);
 const scene=game.scene as any,lines=["GOLDEN_STORAGE 1 10"];
 for(let side=0;side<2;side++)for(let stack=1;stack<=5;stack++){
  const p=side?scene.getEnemyParty()[0]:scene.getPlayerParty()[0];
  const t=modifierTypes.GOLDEN_PUNCH().withIdFromFunc(modifierTypes.GOLDEN_PUNCH),m=t.newModifier(p) as DamageMoneyRewardModifier;
  m.stackCount=stack;m.virtualStackCount=stack===5?0:1;
  const data=new ModifierData(m,!side),serialized=JSON.stringify(data);
  expect(serialized).not.toContain("virtualStackCount");
  const restored=new ModifierData(JSON.parse(serialized),!side).toModifier(DamageMoneyRewardModifier) as DamageMoneyRewardModifier;
  expect(restored).not.toBeNull();expect(restored.pokemonId).toBe(p.id);expect(restored.stackCount).toBe(stack);expect(restored.virtualStackCount).toBe(0);
  scene.modifiers=[];scene.enemyModifiers=[];scene.money=0;
  m.apply(p,{value:40} as any);const before=scene.money;
  scene.money=0;restored.apply(p,{value:40} as any);const after=scene.money;
  expect(before).toBe(20*(stack+m.virtualStackCount));expect(after).toBe(20*stack);
  lines.push(`P ${side} ${stack} ${m.virtualStackCount} ${restored.stackCount} ${restored.virtualStackCount} ${before} ${after}`);
 }
 lines.push("Z 10");fs.writeFileSync(process.env.GC_GOLDEN_STORAGE_OUT??"/tmp/golden-storage.txt",lines.join("\n")+"\n");
 console.log("Golden Punch storage: 10 valid-holder source reconstructions, virtual stacks omitted/reset, amounts identical");
},120000);
