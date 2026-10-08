/* Additive persistence boundary proof: real ModifierData reconstruction omits
 * virtual stacks. Valid holders only; not whole upstream save/session parity. */
import { modifierTypes } from "#data/data-lists";
import { AbilityId } from "#enums/ability-id";
import { MoveId } from "#enums/move-id";
import { SpeciesId } from "#enums/species-id";
import { PokemonMultiHitModifier } from "#modifiers/modifier";
import { ModifierData } from "#system/modifier-data";
import { GameManager } from "#test/framework/game-manager";
import Phaser from "phaser";
import * as fs from "node:fs";
import { expect, it } from "vitest";
it("Multi Lens real-stack storage boundary",async()=>{
 expect(process.versions.node.split(".")[0]).toBe("22");
 const game=new GameManager(new Phaser.Game({type:Phaser.HEADLESS}));
 game.override.battleStyle("single").startingWave(5).ability(AbilityId.NONE)
  .enemySpecies(SpeciesId.MAGIKARP).enemyMoveset(MoveId.SPLASH);
 await game.classicMode.startBattle(SpeciesId.BULBASAUR);
 const scene=game.scene as any,lines=["MULTI_STORAGE 1 4"];
 for(let side=0;side<2;side++)for(let stack=1;stack<=2;stack++){
  const p=side?scene.getEnemyParty()[0]:scene.getPlayerParty()[0];
  const t=modifierTypes.MULTI_LENS().withIdFromFunc(modifierTypes.MULTI_LENS),m=t.newModifier(p) as PokemonMultiHitModifier;
  m.stackCount=stack;m.virtualStackCount=stack===2?0:1;
  const data=new ModifierData(m,!side),serialized=JSON.stringify(data);
  expect(serialized).not.toContain("virtualStackCount");
  const restored=new ModifierData(JSON.parse(serialized),!side).toModifier(PokemonMultiHitModifier) as PokemonMultiHitModifier;
  expect(restored).not.toBeNull();expect(restored.pokemonId).toBe(p.id);expect(restored.stackCount).toBe(stack);expect(restored.virtualStackCount).toBe(0);
  const before={value:1},after={value:1};
  expect(m.apply(p,MoveId.SWIFT,before as any)).toBe(true);expect(restored.apply(p,MoveId.SWIFT,after as any)).toBe(true);
  expect(before.value).toBe(1+stack+m.virtualStackCount);expect(after.value).toBe(1+stack);
  p.turnData.hitCount=before.value;p.turnData.hitsLeft=before.value;const beforeDamage={value:1};
  expect(m.apply(p,MoveId.SWIFT,null,beforeDamage as any)).toBe(true);
  p.turnData.hitCount=after.value;p.turnData.hitsLeft=after.value;const afterDamage={value:1};
  expect(restored.apply(p,MoveId.SWIFT,null,afterDamage as any)).toBe(true);
  lines.push(`P ${side} ${stack} ${m.virtualStackCount} ${restored.stackCount} ${restored.virtualStackCount} ${before.value} ${after.value} ${beforeDamage.value} ${afterDamage.value}`);
 }
 lines.push("Z 4");fs.writeFileSync(process.env.GC_MULTI_STORAGE_OUT??"/tmp/multi-storage.txt",lines.join("\n")+"\n");
 console.log("Multi Lens storage: 4 valid-holder source reconstructions, virtual stacks omitted/reset, amounts identical");
},120000);
