/* Native persistent add, large stock, detached loot and high-index transfers. */
import { BattleScene } from "#app/battle-scene";
import { modifierTypes } from "#data/data-lists";
import { generateModifierType } from "#mystery-encounters/encounter-phase-utils";
import { getModifierType } from "#utils/modifier-utils";
import { SpeciesId } from "#enums/species-id";
import { AbilityId } from "#enums/ability-id";
import { MoveId } from "#enums/move-id";
import { GameManager } from "#test/framework/game-manager";
import Phaser from "phaser";
import fs from "node:fs";
import { expect,it,vi } from "vitest";
it("native full party inventories beyond original C capacities",async()=>{
 const native=BattleScene.prototype.randBattleSeedInt;
 const g=new GameManager(new Phaser.Game({type:Phaser.HEADLESS}));
 g.override.battleStyle("double").ability(AbilityId.NONE).enemyAbility(AbilityId.NONE).enemySpecies(SpeciesId.MAGIKARP).moveset(MoveId.SPLASH).enemyMoveset(MoveId.SPLASH);
 await g.classicMode.startBattle(...Array(6).fill(SpeciesId.BULBASAUR));
 BattleScene.prototype.randBattleSeedInt=native;
 const s:any=g.scene,players:any[]=s.getPlayerParty(),enemies:any[]=s.getEnemyParty(),actors=[...players,...enemies];
 expect(players.length).toBe(6);expect(enemies.length).toBe(2);
 vi.spyOn(s.phaseManager,"shiftPhase").mockImplementation(()=>{});
 for(const p of actors)vi.spyOn(p,"updateInfo").mockResolvedValue(undefined);
 const simple=["LEFTOVERS","SHELL_BELL","FOCUS_BAND","QUICK_CLAW","KINGS_ROCK","WHITE_HERB","MINI_BLACK_HOLE","WIDE_LENS","MULTI_LENS","SCOPE_LENS","TOXIC_ORB","SOUL_DEW","GOLDEN_PUNCH","REVIVER_SEED","BATON","MYSTICAL_ROCK","LEEK","LUCKY_EGG","GOLDEN_EGG","SOOTHE_BELL","GRIP_CLAW","EVIOLITE"];
 const variants=simple.map(key=>({key,arg:-1}));
 variants.push(...Array.from({length:11},(_,arg)=>({key:"BERRY",arg})),...Array.from({length:18},(_,arg)=>({key:"ATTACK_TYPE_BOOSTER",arg})));
 const speciesKeys=["LIGHT_BALL","THICK_CLUB","METAL_POWDER","QUICK_POWDER","DEEP_SEA_SCALE","DEEP_SEA_TOOTH"];
 variants.push(...speciesKeys.map((_,arg)=>({key:arg<4?"RARE_SPECIES_STAT_BOOSTER":"SPECIES_STAT_BOOSTER",arg})));
 expect(variants.length).toBe(57);
 s.modifiers=[];s.enemyModifiers=[];s.currentBattle.postBattleLoot=[];
 const types=new Map<any,any>();
 const make=(v:any,p:any)=>{
  const type:any=v.arg<0?getModifierType((modifierTypes as any)[v.key]):generateModifierType((modifierTypes as any)[v.key],[v.key.includes("SPECIES_STAT")?speciesKeys[v.arg]:v.arg]);
  expect(type).toBeTruthy();expect(type.id).toBe(v.key);types.set(type,v);return type.newModifier(p);
 };
 for(const p of actors)for(const v of variants){const m=make(v,p);const list=p.isPlayer()?s.modifiers:s.enemyModifiers;expect(m.add(list,false)).toBe(true);}
 expect(s.modifiers.length).toBe(342);expect(s.enemyModifiers.length).toBe(114);
 await s.updateModifiers(true,true);await s.updateModifiers(false,true);
 const snapshot=()=>[s.modifiers,s.enemyModifiers].map((list:any[])=>list.map((m:any)=>{
  const v=types.get(m.type);expect(v).toBeTruthy();
  return {key:v.key,arg:v.arg,owner:m.pokemonId==null?-1:actors.findIndex(p=>p.id===m.pokemonId),stack:m.stackCount,cls:m.constructor.name};
 }));
 const before=snapshot();
 // Record the final enemy's Eviolite position as well as its later type booster.
 const source=s.enemyModifiers.find((m:any)=>m.pokemonId===enemies[1].id&&m.type.id==="EVIOLITE");
 const index=s.enemyModifiers.indexOf(source);expect(index).toBe(78);
 // Type boosters near the end of that holder are beyond index 96.
 const high=s.enemyModifiers.find((m:any)=>m.pokemonId===enemies[1].id&&m.type.id==="ATTACK_TYPE_BOOSTER"&&m.moveType===17);
 const highIndex=s.enemyModifiers.indexOf(high);expect(highIndex).toBeGreaterThan(96);
 const transferred=s.tryTransferHeldItemModifier(high,players[5],false,1,true,true);
 expect(transferred).toBe(true);
 const after=snapshot();
 for(const p of enemies)s.currentBattle.addPostBattleLoot(p);
 const loot=s.currentBattle.postBattleLoot.map((m:any)=>s.enemyModifiers.indexOf(m));
 expect(loot.length).toBeGreaterThan(96);
 fs.writeFileSync(process.env.INVENTORY_OUT!,JSON.stringify({version:1,before,after,highIndex,transferred,loot})+"\n");
},120000);
