/* Native SwitchSummonPhase branch, stopped at send-out presentation. */
import { BattleScene } from "#app/battle-scene";
import { speciesDataRegistry } from "#app/globals/global-species-data-registry";
import { modifierTypes } from "#data/data-lists";
import { getModifierType } from "#utils/modifier-utils";
import { AbilityId } from "#enums/ability-id";
import { SpeciesId } from "#enums/species-id";
import { MoveId } from "#enums/move-id";
import { SwitchType } from "#enums/switch-type";
import { TrainerSlot } from "#enums/trainer-slot";
import { GameManager } from "#test/framework/game-manager";
import Phaser from "phaser";
import * as fs from "node:fs";
import { expect,it,vi } from "vitest";

it("native Baton switching inventory branch",async()=>{
 const nativeRng=BattleScene.prototype.randBattleSeedInt;
 const g=new GameManager(new Phaser.Game({type:Phaser.HEADLESS}));
 g.override.battleStyle("double").ability(AbilityId.BALL_FETCH).enemyAbility(AbilityId.BALL_FETCH)
  .enemySpecies(SpeciesId.MAGIKARP).moveset(MoveId.SPLASH).enemyMoveset(MoveId.SPLASH);
 await g.classicMode.startBattle(SpeciesId.BULBASAUR,SpeciesId.CHARMANDER,SpeciesId.SQUIRTLE);
 BattleScene.prototype.randBattleSeedInt=nativeRng;
 const s:any=g.scene,pm:any=s.phaseManager;
 const player:any[]=s.getPlayerParty().slice(0,3),enemy:any[]=s.getEnemyParty().slice(0,2);
 enemy.push(s.addEnemyPokemon(speciesDataRegistry.getSpecies(SpeciesId.PIDGEY),5,TrainerSlot.NONE));
 const actors=[...player,...enemy];expect(new Set(actors.map(p=>p.id)).size).toBe(6);
 g.override.ability(AbilityId.NONE).enemyAbility(AbilityId.NONE);
 vi.spyOn(pm,"shiftPhase").mockImplementation(()=>{});
 for(const p of actors)vi.spyOn(p,"updateInfo").mockImplementation(async()=>{});
 const presentationStop={};let hitPresentation=false;
 vi.spyOn(s.ui,"showText").mockImplementation(()=>{hitPresentation=true;throw presentationStop;});
 vi.spyOn(s.time,"delayedCall").mockImplementation((_ms:any,callback:any)=>{callback();return {} as any;});
 vi.spyOn(s.pbTrayEnemy,"hide").mockImplementation(()=>{});
 const inventory=()=>[s.modifiers,s.enemyModifiers].map((list:any[])=>list.map(m=>({key:m.type.id,arg:-1,
  owner:actors.findIndex(p=>p.id===m.pokemonId),stack:m.stackCount,virtual:m.virtualStackCount,
  transferable:+m.isTransferable,max:m.getMaxStackCount(),tier:m.type.tier??-1})));
 const mons=()=>actors.map(p=>({id:p.id,species:p.species.speciesId,form:p.formIndex,level:p.level,nature:p.getNature(),ivs:p.ivs.slice(),hp:p.hp,ability:p.getAbility().id,stats:p.stats.slice(),tags:p.summonData.tags.map((t:any)=>t.tagType)}));
 const rows:any[]=[];let i=0;
 for(let side=0;side<2;side++)for(let field=0;field<2;field++)
 for(const kind of [SwitchType.SWITCH,SwitchType.BATON_PASS,SwitchType.FORCE_SWITCH,SwitchType.SHED_TAIL])
 for(let source=0;source<2;source++)for(let target=0;target<2;target++)
 for(let sourceLock=0;sourceLock<2;sourceLock++)for(let targetLock=0;targetLock<2;targetLock++){
  pm.clearAllPhases();s.modifiers=[];s.enemyModifiers=[];
  s.getPlayerParty().splice(0,s.getPlayerParty().length,...player);
  s.getEnemyParty().splice(0,s.getEnemyParty().length,...enemy);
  for(let k=0;k<6;k++){
   const p=actors[k];p.resetSummonData();p.resetTurnData();p.resetBattleAndWaveData();
   p.customPokemonData.ability=AbilityId.UNBURDEN;p.customPokemonData.passive=AbilityId.NONE;p.passive=false;
   p.fusionSpecies=null;p.nature=(i+k)%25;p.customPokemonData.nature=-1;p.shiny=false;
   p.switchOutStatus=false;p.stats=Array(6).fill(0);p.hp=0;p.calculateStats();p.hp=p.getMaxHp();
   if(k%3===2){if(p.isOnField())s.field.remove(p);}else if(!p.isOnField())s.field.add(p);
  }
  const last=side*3+field,incoming=side*3+2;
  const add=(owner:number,locked:number)=>{
   const m:any=getModifierType(modifierTypes.BATON).newModifier(actors[owner]);m.isTransferable=!locked;
   (actors[owner].isPlayer()?s.modifiers:s.enemyModifiers).push(m);
  };
  if(source)add(last,sourceLock);if(target)add(incoming,targetLock);
  add((1-side)*3+(1-field),0); // unrelated holder must survive
  Phaser.Math.RND.sow([`baton-switch-${i}`]);s.currentBattle.battleSeed=`baton-switch-${i}`;
  s.currentBattle.battleSeedState=null;s.currentBattle.randSeedInt(2);
  const before={global:Phaser.Math.RND.state(),battle:s.currentBattle.battleSeedState};
  const input=inventory(),beforeActors=mons();
  const sourceStock=(side===0?s.modifiers:s.enemyModifiers).find((m:any)=>m.pokemonId===actors[last].id);
  const can=sourceStock?s.canTransferHeldItemModifier(sourceStock,actors[incoming],1):null;
  const ph:any=pm.create("SwitchSummonPhase",kind,field,2,false,side===0);
  vi.spyOn(ph,"hideEnemyTrainer").mockImplementation(()=>{});
  hitPresentation=false;
  try{ph.switchAndSummon();throw Error("send-out presentation was not reached");}
  catch(error){if(error!==presentationStop)throw error;}
  expect(hitPresentation).toBe(true);
  rows.push({i:i++,side,field,kind,can,source,target,sourceLock,targetLock,input,beforeActors,before,
   output:inventory(),actors:mons(),after:{global:Phaser.Math.RND.state(),battle:s.currentBattle.battleSeedState}});
 }
 expect(rows.length).toBe(256);
 expect(rows.some(r=>r.side===0&&r.kind===SwitchType.BATON_PASS&&r.source&&!r.target&&r.output[0].some((m:any)=>m.owner===2))).toBe(true);
 fs.writeFileSync(process.env.BATON_SWITCH_OUT!,JSON.stringify({version:2,rows,tiers:[0,1,2].map(pool=>getModifierType(modifierTypes.BATON).getOrInferTier(pool))})+'\n');
},600000);
