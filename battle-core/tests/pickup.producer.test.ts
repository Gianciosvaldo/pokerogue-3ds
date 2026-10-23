/* Actual Pickup leaves, dispatcher and BattleEndPhase callback owner. */
import { BattleScene } from "#app/battle-scene";
import { applyAbAttrs } from "#abilities/apply-ab-attrs";
import { allAbilities,modifierTypes } from "#data/data-lists";
import { generateModifierType } from "#mystery-encounters/encounter-phase-utils";
import { getModifierType } from "#utils/modifier-utils";
import { BattleEndPhase } from "#phases/battle-end-phase";
import { AbilityId } from "#enums/ability-id";
import { SpeciesId } from "#enums/species-id";
import { ModifierPoolType } from "#enums/modifier-pool-type";
import { MoveId } from "#enums/move-id";
import { GameManager } from "#test/framework/game-manager";
import Phaser from "phaser";
import fs from "node:fs";
import { expect,it,vi } from "vitest";
it("native ordered Pickup stock, cache, gates and callback owner",async()=>{
 const native=BattleScene.prototype.randBattleSeedInt;
 const g=new GameManager(new Phaser.Game({type:Phaser.HEADLESS}));
 g.override.battleStyle("double").ability(AbilityId.BALL_FETCH).enemyAbility(AbilityId.BALL_FETCH)
  .enemySpecies(SpeciesId.MAGIKARP).moveset(MoveId.SPLASH).enemyMoveset(MoveId.SPLASH);
 await g.classicMode.startBattle(SpeciesId.BULBASAUR,SpeciesId.CHARMANDER);
 BattleScene.prototype.randBattleSeedInt=native;
 const s:any=g.scene,pm:any=s.phaseManager,actors:any[]=s.getField();
 g.override.ability(AbilityId.NONE).enemyAbility(AbilityId.NONE);
 vi.spyOn(pm,"shiftPhase").mockImplementation(()=>{});
 for(const p of actors){vi.spyOn(p,"updateInfo").mockResolvedValue(undefined);if(p.isEnemy())vi.spyOn(p,"destroy").mockImplementation(()=>{});}
 vi.spyOn(s.currentBattle,"addBattleScore").mockImplementation(()=>{});
 // Observe the actual owner callbacks before its subsequent inventory cleanup.
 vi.spyOn(s,"clearEnemyHeldItemModifiers").mockImplementation(()=>{});
 const attr:any=allAbilities[AbilityId.PICKUP].attrs.find((a:any)=>a.constructor.name==="PostBattleLootAbAttr");expect(attr).toBeTruthy();
 const keys=["LEFTOVERS","SHELL_BELL","FOCUS_BAND","QUICK_CLAW","KINGS_ROCK","WHITE_HERB","MINI_BLACK_HOLE","BERRY","WIDE_LENS","MULTI_LENS","SCOPE_LENS","TOXIC_ORB","FLAME_ORB","SOUL_DEW","GOLDEN_PUNCH","ATTACK_TYPE_BOOSTER","REVIVER_SEED","BATON","MYSTICAL_ROCK","LEEK","LUCKY_EGG","GOLDEN_EGG","SOOTHE_BELL"];
 const variants=keys.flatMap(key=>Array.from({length:key==="BERRY"?11:key==="ATTACK_TYPE_BOOSTER"?18:1},(_,arg)=>({key,arg:key==="BERRY"||key==="ATTACK_TYPE_BOOSTER"?arg:-1})));
 expect(variants.length).toBe(50);
 const tiers=keys.filter(key=>!["BERRY","ATTACK_TYPE_BOOSTER"].includes(key)).map(key=>({key,values:[ModifierPoolType.WILD,ModifierPoolType.TRAINER].map(pool=>{const type:any=getModifierType((modifierTypes as any)[key]);type.tier=undefined;return type.getOrInferTier(pool);})}));
 const rows:any[]=[];let i=0;
 const cases=variants.flatMap(v=>Array.from({length:80},(_,j)=>({v,scenario:Math.floor(j/4),mode:j%4,shape:-1})));
 cases.push(...variants.flatMap(v=>Array.from({length:8},(_,shape)=>({v,scenario:1,mode:3,shape}))));
 for(const {v,scenario,mode,shape} of cases){
  s.party.splice(0,2,...(shape===6?[actors[1],actors[0]]:[actors[0],actors[1]]));
  s.modifiers=[];s.enemyModifiers=[];s.currentBattle.postBattleLoot=[];s.currentBattle.moneyScattered=0;pm.clearAllPhases();attr.randItem=undefined;
  for(let k=0;k<4;k++){
   const p=actors[k];if(s.field.getIndex(p)<0)s.field.add(p);p.resetSummonData();p.resetWaveData();p.resetTurnData();p.customPokemonData.ability=k<2?AbilityId.PICKUP:AbilityId.NONE;
   p.customPokemonData.passive=AbilityId.NONE;p.passive=false;p.customPokemonData.nature=-1;p.nature=(scenario+k)%25;
   p.stats=Array(6).fill(0);p.hp=0;p.calculateStats();p.hp=p.getMaxHp();
  }
  if(shape>=0){
   actors[0].customPokemonData.ability=AbilityId.PICKUP;actors[1].customPokemonData.ability=AbilityId.NONE;
   if([0,7].includes(shape)){actors[0].customPokemonData.ability=AbilityId.NONE;actors[1].customPokemonData.ability=AbilityId.PICKUP;s.field.remove(actors[1]);}
   if([1,2,3,4].includes(shape)){actors[0].passive=true;actors[0].customPokemonData.passive=AbilityId.PICKUP;}
   if(shape===1)actors[0].customPokemonData.ability=AbilityId.NONE;
   if(shape===3)actors[0].summonData.abilitySuppressed=true;
   if(shape===4)actors[0].customPokemonData.ability=AbilityId.HONEY_GATHER;
   if(shape===5){actors[0].passive=true;actors[0].customPokemonData.passive=AbilityId.HONEY_GATHER;}
   if(shape===6)actors[1].customPokemonData.ability=AbilityId.PICKUP;
   if(shape===7)actors[0].hp=0;
  }
  const make=(key:string,arg:number,owner:number)=>{
   const type:any=arg<0?getModifierType((modifierTypes as any)[key]):generateModifierType((modifierTypes as any)[key],[arg]);
   expect(type).toBeTruthy();return type.newModifier(actors[owner]);
  };
  const stock:any=make(v.key,v.arg,2),cap=stock.getMaxStackCount();stock.stackCount=[0,1,cap,cap+1][scenario%4];
  if(scenario===18)stock.isTransferable=false;
  s.enemyModifiers.push(stock);
  if(shape>=0 || scenario%2 || (mode===1&&scenario===18))s.enemyModifiers.push(make("BERRY",10,3));
  const match=scenario<4?null:make(v.key,v.arg,0);
  if(match){match.stackCount=scenario%3===0?cap:Math.max(0,cap-1);s.modifiers.push(match);}
  await s.updateModifiers(); // Real initial stats, with canonical existing stock.
  // Raw object boundaries (including zero stacks) follow the initialized scene.
  if(!s.enemyModifiers.includes(stock))s.enemyModifiers.unshift(stock);
  stock.stackCount=[0,1,cap,cap+1][scenario%4];
  if(mode!==3&&scenario%5===0)stock.virtualStackCount=2;
  const refs=new Map<any,number>();let next=0;
  for(const p of actors.slice(2)){
   s.currentBattle.addPostBattleLoot(p);
   for(const m of s.currentBattle.postBattleLoot)if(!refs.has(m))refs.set(m,++next);
  }
  if(mode===1&&scenario===18){attr.randItem=stock;if(!refs.has(stock))refs.set(stock,++next);stock.pokemonId=null;}
  if(mode===1&&scenario===19){attr.randItem=stock;s.currentBattle.postBattleLoot=s.currentBattle.postBattleLoot.filter((m:any)=>m!==stock);}
  if(scenario===19&&mode!==1)actors[0].hp=0;
  const victory=mode===3||scenario!==16,simulated=scenario===17&&mode!==3;
  const item=(m:any)=>({ref:refs.get(m)??0,key:m.type.id,arg:m.berryType??m.moveType??-1,owner:m.pokemonId==null?-1:actors.findIndex(p=>p.id===m.pokemonId),stack:m.stackCount,virtual:m.virtualStackCount,transferable:+m.isTransferable,max:m.getMaxStackCount(),cls:m.constructor.name});
  const inventory=()=>[s.modifiers,s.enemyModifiers].map((list:any[])=>list.map(item));
  const mons=()=>actors.map(p=>({id:p.id,species:p.species.speciesId,form:p.formIndex,level:p.level,nature:p.getNature(),ivs:p.ivs.slice(),hp:p.hp,ability:p.getAbility().id,passive:p.hasPassive()?p.getPassiveAbility().id:0,suppressed:+p.summonData.abilitySuppressed,onField:+p.isOnField(),stats:p.stats.slice(),waveAbilities:[...p.waveData.abilitiesApplied],summonAbilities:[...p.summonData.abilitiesApplied]}));
  const loot=()=>s.currentBattle.postBattleLoot.map((m:any)=>refs.get(m));
  const cache=()=>attr.randItem?(refs.get(attr.randItem)??-1):0;
  Phaser.Math.RND.sow([`pickup-${i}`]);s.currentBattle.battleSeed=`pickup-${i}`;s.currentBattle.battleSeedState=null;s.currentBattle.randSeedInt(2);
  const before={money:s.currentBattle.moneyScattered,inventory:inventory(),actors:mons(),loot:loot(),cache:cache(),global:Phaser.Math.RND.state(),battle:s.currentBattle.battleSeedState};
  const after=()=>({money:s.currentBattle.moneyScattered,inventory:inventory(),actors:mons(),loot:loot(),cache:cache(),global:Phaser.Math.RND.state(),battle:s.currentBattle.battleSeedState});
  const queues=()=>pm.phaseQueue.levels.map((level:any[])=>level.map(q=>({name:q.phaseName,text:q.text??null,battler:q.battlerIndex??-1,passive:q.passive??null,prompt:q.prompt??null})));
  let prefix:any,result:any=null;
  if(mode===0)result=attr.canApply({pokemon:actors[0],victory,simulated});
  else if(mode===1)attr.apply({pokemon:actors[0],victory,simulated});
  else if(mode===2)applyAbAttrs("PostBattleAbAttr",{pokemon:actors[0],victory,simulated});
  else {
   // Snapshot the actual owner prefix before money pickup; run the real money
   // owner afterwards. Its separate native proof covers that continuation.
   const pickUp=s.currentBattle.pickUpScatteredMoney;
   s.currentBattle.pickUpScatteredMoney=function(){prefix={after:after(),queue:queues()};return pickUp.call(this);};
   const phase=new BattleEndPhase(victory);vi.spyOn(phase,"end").mockImplementation(()=>{});phase.start();
   s.currentBattle.pickUpScatteredMoney=pickUp;
  }
  rows.push({i:i++,variant:v,scenario,mode,shape,wave:s.currentBattle.waveIndex,victory,simulated,result,waveSeed:s.waveSeed,turn:s.currentBattle.turn,before,
   after:prefix?.after??after(),queue:prefix?.queue??queues()});
 }
 expect(rows.length).toBe(4400);expect(rows.some(r=>r.mode===0&&r.before.cache===0&&r.after.cache>0&&!r.result)).toBe(true);
 expect(rows.some(r=>r.mode===3&&r.before.loot.length>r.after.loot.length)).toBe(true);
 fs.writeFileSync(process.env.PICKUP_OUT!,JSON.stringify({version:2,tiers,rows})+"\n");
},600000);
