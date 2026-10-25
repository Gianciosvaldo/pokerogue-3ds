/* Source constructor locks through serialization, UI filtering and Knock Off. */
import { BattleScene } from "#app/battle-scene";
import { modifierTypes,allMoves } from "#data/data-lists";
import { AbilityId } from "#enums/ability-id";
import { MoveId } from "#enums/move-id";
import { SpeciesId } from "#enums/species-id";
import { PartyUiHandler } from "#ui/party-ui-handler";
import { FormChangeItem } from "#enums/form-change-item";
import { ModifierData } from "#system/modifier-data";
import { GameManager } from "#test/framework/game-manager";
import Phaser from "phaser";
import fs from "node:fs";
import { expect,it } from "vitest";
it("native canonical held constructor locks",async()=>{
 const native=BattleScene.prototype.randBattleSeedInt,g=new GameManager(new Phaser.Game({type:Phaser.HEADLESS}));
 g.override.battleStyle("double").ability(AbilityId.NONE).enemyAbility(AbilityId.NONE).moveset(MoveId.SPLASH).enemyMoveset(MoveId.SPLASH).enemySpecies(SpeciesId.MEW);
 await g.classicMode.startBattle(SpeciesId.MEW,SpeciesId.MEW);BattleScene.prototype.randBattleSeedInt=native;
 const s:any=g.scene,actors:any[]=[...s.getPlayerField(),...s.getEnemyField()],rows:any[]=[];
 for(const p of actors)p.ivs=Array(6).fill(31);
 const cases:[string,number][]=[];
 for(const id of Object.values(FormChangeItem).filter((n:any)=>typeof n==="number"&&n>=0) as number[])for(const key of ["FORM_CHANGE_ITEM","RARE_FORM_CHANGE_ITEM"])cases.push([key,id]);
 for(let stat=0;stat<6;stat++)cases.push(["BASE_STAT_BOOSTER",stat]);
 cases.push(["EVOLUTION_TRACKER_GIMMIGHOUL",0],["MYSTERY_ENCOUNTER_SHUCKLE_JUICE",10],["MYSTERY_ENCOUNTER_SHUCKLE_JUICE",-15],["MYSTERY_ENCOUNTER_OLD_GATEAU",0],["MYSTERY_ENCOUNTER_MACHO_BRACE",0]);
 const attr=allMoves[MoveId.KNOCK_OFF].getAttrs("RemoveHeldItemAttr")[0],partyUI:any=Object.create(PartyUiHandler.prototype);
 expect(typeof partyUI.getItemModifiers).toBe("function");
 // Positive controls distinguish the real menu query from an empty UI stub.
 for(let owner=0;owner<2;owner++){
  const f=modifierTypes.LEFTOVERS,m=f().withIdFromFunc(f).newModifier(actors[owner]);
  s.modifiers=[m];expect(partyUI.getItemModifiers(actors[owner])).toEqual([m]);
 }
 s.modifiers=[];
 for(const [key,arg] of cases)for(let owner=0;owner<4;owner++)for(let active=0;active<2;active++){
  s.modifiers=[];s.enemyModifiers=[];const p=actors[owner],user=actors[owner<2?2:0];
  const f=(modifierTypes as any)[key];let t:any=f().withIdFromFunc(f);
  if(typeof t.generateType==="function"){t=t.generateType([p],[arg]);expect(t).not.toBeNull();t.id=key;}
  const m:any=t.newModifier(p);if("active" in m)m.active=!!active;
  (owner<2?s.modifiers:s.enemyModifiers).push(m);
  const data=new ModifierData(m,owner<2),restored:any=new ModifierData(JSON.parse(JSON.stringify(data)),owner<2).toModifier(m.constructor);
  expect(restored).not.toBeNull();expect(restored.isTransferable).toBe(false);
  const rb=s.currentBattle.battleSeedState,rg=Phaser.Math.RND.state();
  const result=attr.apply(user,p,allMoves[MoveId.KNOCK_OFF],[]);
  expect(Phaser.Math.RND.state()).toBe(rg);expect(s.currentBattle.battleSeedState).toBe(rb);
  const visible=partyUI.getItemModifiers(p).length;
  expect(m.isTransferable).toBe(false);expect(result).toBe(false);expect(visible).toBe(0);expect(p.getHeldItems()).toContain(m);
  rows.push({key,arg,owner,active,locked:!m.isTransferable,restoredLocked:!restored.isTransferable,result,visible});
 }
 fs.writeFileSync(process.env.LOCK_OUT!,JSON.stringify({version:1,forms:FormChangeItem.NORMAL_MEMORY,rows})+"\n");
},240000);
