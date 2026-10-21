/* Actual scene helpers; no modeled transfer, clone, cap or callback oracle. */
import { BattleScene } from "#app/battle-scene";
import { generateModifierType } from "#mystery-encounters/encounter-phase-utils";
import { modifierTypes } from "#data/data-lists";
import { getModifierType } from "#utils/modifier-utils";
import { AbilityId } from "#enums/ability-id";
import { SpeciesId } from "#enums/species-id";
import { MoveId } from "#enums/move-id";
import { GameManager } from "#test/framework/game-manager";
import Phaser from "phaser";
import * as fs from "node:fs";
import { expect,it,vi } from "vitest";

it("native scene transfer and preflight",async()=>{
 const nativeBattleRng=BattleScene.prototype.randBattleSeedInt;
 const g=new GameManager(new Phaser.Game({type:Phaser.HEADLESS}));
 g.override.battleStyle("double").ability(AbilityId.BALL_FETCH).enemyAbility(AbilityId.BALL_FETCH)
  .enemySpecies(SpeciesId.MAGIKARP).moveset(MoveId.SPLASH).enemyMoveset(MoveId.SPLASH);
 await g.classicMode.startBattle(SpeciesId.BULBASAUR,SpeciesId.CHARMANDER);
 BattleScene.prototype.randBattleSeedInt=nativeBattleRng;
 const s:any=g.scene,pm:any=s.phaseManager,actors:any[]=s.getField();
 expect(actors.length).toBe(4);expect(actors.every(Boolean)).toBe(true);
 g.override.ability(AbilityId.NONE).enemyAbility(AbilityId.NONE);
 vi.spyOn(pm,"shiftPhase").mockImplementation(()=>{});
 for(const p of actors)vi.spyOn(p,"updateInfo").mockImplementation(async()=>{});
 const keys=["LEFTOVERS","SHELL_BELL","FOCUS_BAND","QUICK_CLAW","KINGS_ROCK","WHITE_HERB","MINI_BLACK_HOLE","BERRY","WIDE_LENS","MULTI_LENS","SCOPE_LENS","TOXIC_ORB","FLAME_ORB","SOUL_DEW","GOLDEN_PUNCH","ATTACK_TYPE_BOOSTER","REVIVER_SEED"];
 const variants=keys.flatMap(key=>Array.from({length:key==="BERRY"?11:key==="ATTACK_TYPE_BOOSTER"?18:1},(_,j)=>({key,arg:key==="BERRY"||key==="ATTACK_TYPE_BOOSTER"?j:-1})));
 const rows:any[]=[];let i=0;
 const inventory=()=>[s.modifiers,s.enemyModifiers].map((list:any[])=>list.map(m=>({key:m.type.id,arg:m.berryType??m.moveType??-1,owner:m.pokemonId==null?-1:actors.findIndex(p=>p.id===m.pokemonId),stack:m.stackCount,virtual:m.virtualStackCount,transferable:+m.isTransferable,max:m.getMaxStackCount()})));
 const mons=()=>actors.map(p=>({id:p.id,species:p.species.speciesId,form:p.formIndex,level:p.level,nature:p.getNature(),ivs:p.ivs.slice(),hp:p.hp,ability:p.getAbility().id,tags:p.summonData.tags.map((t:any)=>t.tagType),stats:p.stats.slice()}));
 const cases=variants.flatMap(v=>Array.from({length:80},(_,j)=>({v,scenario:Math.floor(j/2),mode:j%2})));
 // Explicit same-side source removal before an existing recipient stack, on
 // each side. These supplement all original 3520 observations.
 cases.push(...variants.flatMap(v=>[40,42].map(scenario=>({v,scenario,mode:1}))));
 for(const {v,scenario,mode} of cases){
  const supplement=scenario>=40;
  s.modifiers=[];s.enemyModifiers=[];pm.clearAllPhases();
  const holder=scenario%4,sourceOwner=supplement?holder+1:(holder+(scenario%3)+1)%4;
  for(let k=0;k<4;k++){
   const p=actors[k];p.resetSummonData();p.resetTurnData();
   p.customPokemonData.ability=k===sourceOwner?(scenario%5===0?AbilityId.STICKY_HOLD:AbilityId.UNBURDEN):AbilityId.NONE;
   p.customPokemonData.passive=AbilityId.NONE;p.passive=false;p.nature=(scenario+k)%25;
   p.customPokemonData.nature=-1;p.stats=Array(6).fill(0);p.hp=0;p.calculateStats();p.hp=p.getMaxHp();
  }
  const make=(owner:number)=>{
   const type:any=v.arg<0?getModifierType((modifierTypes as any)[v.key]):generateModifierType((modifierTypes as any)[v.key],[v.arg]);
   expect(type).toBeTruthy();return type.newModifier(actors[owner]);
  };
  const stock:any=make(sourceOwner);const cap=stock.getMaxStackCount();
  stock.stackCount=supplement?1:[0,1,cap,cap+1][Math.floor(scenario/4)%4];
  if(scenario%7===0)stock.isTransferable=false;
  const detached=!supplement&&scenario%4===0;if(detached)stock.pokemonId=null;
  const fromSide=sourceOwner<2?0:1;const lists=[s.modifiers,s.enemyModifiers];
  lists[fromSide].push(stock);
  if(supplement||scenario%3!==0){const target:any=make(holder);target.stackCount=supplement?cap-1:scenario%3===1?cap:Math.max(0,cap-1);lists[holder<2?0:1].push(target);}
  // Unrelated stock makes list ordering and source/receiver update observable.
  const extra:any=getModifierType(supplement&&v.key==="LEFTOVERS"?modifierTypes.SHELL_BELL:modifierTypes.LEFTOVERS).newModifier(actors[(holder+1)%4]);
  if(!lists[(holder+1)%4<2?0:1].some((m:any)=>m.pokemonId===extra.pokemonId&&m.matchType(extra)))lists[(holder+1)%4<2?0:1].push(extra);
  const ignoreUpdate=!supplement&&scenario%2===0,itemLost=supplement||Math.floor(scenario/2)%2===0;
  // Virtual counts remain meaningful for skipped updates; canonical raw Mini
  // Black Hole metadata is retained when a stat recalculation is requested.
  if(ignoreUpdate){stock.virtualStackCount=scenario%3;extra.virtualStackCount=2;}
  const orphan=mode===0&&scenario%9===0;if(orphan)lists[fromSide].splice(lists[fromSide].indexOf(stock),1);
  Phaser.Math.RND.sow([`scene-transfer-${i}`]);s.currentBattle.battleSeed=`scene-transfer-${i}`;
  s.currentBattle.battleSeedState=null;s.currentBattle.randSeedInt(2);
  const before={global:Phaser.Math.RND.state(),battle:s.currentBattle.battleSeedState};
  const input=inventory(),beforeActors=mons();
  const stockBefore={key:stock.type.id,arg:stock.berryType??stock.moveType??-1,owner:stock.pokemonId==null?-1:actors.findIndex(p=>p.id===stock.pokemonId),stack:stock.stackCount,virtual:stock.virtualStackCount,transferable:+stock.isTransferable,max:stock.getMaxStackCount()};
  const quantity=supplement?1:[0,1,2,99][Math.floor(scenario/10)];
  const stockIndex=lists[fromSide].indexOf(stock);
  const result=mode===0?s.canTransferHeldItemModifier(stock,actors[holder],quantity)
   :s.tryTransferHeldItemModifier(stock,actors[holder],false,quantity,true,ignoreUpdate,itemLost);
  rows.push({i:i++,holder,fromSide,stockIndex,waveSeed:s.waveSeed,turn:s.currentBattle.turn,sourceOwner,mode,quantity,ignoreUpdate,itemLost,orphan,detached,
   stock:{key:v.key,arg:v.arg,owner:detached?-1:sourceOwner,stack:stock.stackCount,virtual:stock.virtualStackCount,transferable:+stock.isTransferable,max:cap},
   stockBefore,
   input,beforeActors,result,output:inventory(),actors:mons(),
   queue:pm.phaseQueue.levels.map((level:any[])=>level.map(q=>({name:q.phaseName,text:q.text??null,battler:q.battlerIndex??-1}))),before,
   after:{global:Phaser.Math.RND.state(),battle:s.currentBattle.battleSeedState}});
 }
 expect(variants.length).toBe(44);expect(rows.length).toBe(3608);
 expect(rows.some(r=>r.detached&&r.result)).toBe(true);
 expect(rows.some(r=>r.orphan&&!r.result)).toBe(true);
 fs.writeFileSync(process.env.SCENE_TRANSFER_OUT!,JSON.stringify({version:2,rows})+'\n');
},600000);
