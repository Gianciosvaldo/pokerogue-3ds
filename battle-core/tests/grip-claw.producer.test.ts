import { BattleScene } from "#app/battle-scene";
/* Actual Grip Claw override and scene dispatch, with native transfer helpers. */
import { generateModifierType } from "#mystery-encounters/encounter-phase-utils";
import { allMoves, modifierTypes } from "#data/data-lists";
import { getModifierType } from "#utils/modifier-utils";
import { AbilityId } from "#enums/ability-id";
import { SpeciesId } from "#enums/species-id";
import { MoveId } from "#enums/move-id";
import { ContactHeldItemTransferChanceModifier } from "#modifiers/modifier";
import { GameManager } from "#test/framework/game-manager";
import Phaser from "phaser";
import * as fs from "node:fs";
import { expect,it,vi } from "vitest";
it("actual Grip Claw callbacks",async()=>{
 const N=Number(process.env.GRIP_N??16384),SEED=Number(process.env.GRIP_SEED??777);
 if(!process.versions.node.startsWith("22.")||!Number.isSafeInteger(N)||N<16384)throw Error("Node22,N>=16384");
 const realBattleRoll=BattleScene.prototype.randBattleSeedInt;
 const g=new GameManager(new Phaser.Game({type:Phaser.HEADLESS}));g.override.battleStyle("double").ability(AbilityId.BALL_FETCH).enemyAbility(AbilityId.BALL_FETCH).enemySpecies(SpeciesId.MAGIKARP).moveset(MoveId.SPLASH).enemyMoveset(MoveId.SPLASH);await g.classicMode.startBattle(SpeciesId.BULBASAUR,SpeciesId.CHARMANDER,SpeciesId.SQUIRTLE);BattleScene.prototype.randBattleSeedInt=realBattleRoll;
 const s:any=g.scene,pm:any=s.phaseManager,actors:any[]=s.getField();if(actors.length!==4||actors.some(p=>!p))throw Error("natural double field required");
 g.override.ability(AbilityId.NONE).enemyAbility(AbilityId.NONE);vi.spyOn(pm,"shiftPhase").mockImplementation(()=>{});
 for(const p of actors){vi.spyOn(p,"updateInfo").mockImplementation(async()=>{});}
 const nativeIds=actors.map(p=>p.id);
 const keys=["LEFTOVERS","SHELL_BELL","FOCUS_BAND","QUICK_CLAW","KINGS_ROCK","WHITE_HERB","MINI_BLACK_HOLE","BERRY","WIDE_LENS","MULTI_LENS","SCOPE_LENS","TOXIC_ORB","FLAME_ORB","SOUL_DEW","GOLDEN_PUNCH","ATTACK_TYPE_BOOSTER","REVIVER_SEED","GRIP_CLAW"];
 let seed=SEED;const rnd=(n:number)=>{seed=(Math.imul(seed,1103515245)+12345)>>>0;return(seed>>>8)%n;};
 const fires:any={},contexts:any={};
 const rows=[`GRIPCLAW 1 ${N} ${SEED}`],index=(id:number)=>actors.findIndex(p=>p.id===id);
 const inventory=()=>[s.modifiers,s.enemyModifiers].map((list:any[])=>list.map(m=>({key:m.type.id,arg:m.berryType??m.moveType??-1,owner:index(m.pokemonId),stack:m.stackCount,virtual:m.virtualStackCount,transferable:+m.isTransferable,max:m.getMaxStackCount(),tier:m.type.tier??-1})));
 for(let i=0;i<N;i++){
  s.modifiers=[];s.enemyModifiers=[];pm.clearAllPhases();const holder=i%4,target=i%29===0?-1:(holder+1+Math.floor(i/4)%3)%4,mode=Math.floor(i/12)%2,move=allMoves[i%2?MoveId.THIEF:MoveId.COVET];actors.forEach((p,k)=>p.id=nativeIds[k]);if(i%101===0)actors[Math.floor(i/101)%4].id=0;
  for(let k=0;k<4;k++){const p=actors[k];p.resetSummonData();p.resetTurnData();p.tempSummonData.turnCount=0;p.tempSummonData.waveTurnCount=0;if(k>=2)p.trainerSlot=Math.floor(i/24)%2;p.customPokemonData.ability=[AbilityId.NONE,AbilityId.STICKY_HOLD,AbilityId.UNBURDEN][Math.floor(i/48+k)%3];p.customPokemonData.passive=AbilityId.NONE;p.passive=false;p.nature=rnd(25);p.customPokemonData.nature=-1;p.stats=Array(6).fill(0);p.hp=0;p.calculateStats();p.hp=Math.floor(i/12)%5===k?0:p.getMaxHp();}
  if(i%17===0)actors.forEach((p,k)=>{if((k<2)!==(holder<2))p.hp=0;});
  const make=(key:string,owner:number)=>{const args=key==="BERRY"?[rnd(11)]:key==="ATTACK_TYPE_BOOSTER"?[rnd(18)]:undefined;const type:any=args?generateModifierType((modifierTypes as any)[key],args):getModifierType((modifierTypes as any)[key]);if(!type)throw Error("modifier unavailable "+key);return type.newModifier(actors[owner]);};
  const grip:any=make("GRIP_CLAW",holder);grip.stackCount=1+rnd(5);
  (actors[holder].isPlayer()?s.modifiers:s.enemyModifiers).push(grip);
  for(let k=0,amount=rnd(12);k<amount;k++){const owner=rnd(4),key=keys[rnd(keys.length)],m:any=make(key,owner);if([...s.modifiers,...s.enemyModifiers].some((old:any)=>old.pokemonId===actors[owner].id&&old.matchType(m)))continue;m.stackCount=1+rnd(Math.max(1,m.getMaxStackCount()));if(i%7===0)m.isTransferable=false;if(i%5===0)m.type.setTier(rnd(6));(actors[owner].isPlayer()?s.modifiers:s.enemyModifiers).push(m);}
  const boundary=i%4096===1;
  if(boundary){
   s.modifiers=[];s.enemyModifiers=[];grip.stackCount=5;
   (actors[holder].isPlayer()?s.modifiers:s.enemyModifiers).push(grip);
   const stock:any=make("LEFTOVERS",target);stock.stackCount=1;
   (actors[target].isPlayer()?s.modifiers:s.enemyModifiers).push(stock);
   actors[target].customPokemonData.ability=AbilityId.UNBURDEN;
  }
  s.updateModifiers(true,true);s.updateModifiers(false,true);
  if(!boundary&&i%31===0&&grip.stackCount<5)grip.virtualStackCount=5-grip.stackCount;
  Phaser.Math.RND.sow([`transfer-${SEED}-${i}`]);if(boundary)Phaser.Math.RND.state(`!rnd,0,${1.5/2091639},0,0`);s.currentBattle.battleSeed=`transfer-${SEED}-${i}`;s.currentBattle.turn=1+i%19;s.currentBattle.battleSeedState=null;s.currentBattle.randSeedInt(2);const before={global:Phaser.Math.RND.state(),battle:s.currentBattle.battleSeedState};
  if(boundary){const observed=Phaser.Math.RND.frac();Phaser.Math.RND.state(before.global);expect(observed).toBe(.5);}const input=inventory();const beforeActors=actors.map(p=>({id:p.id,species:p.species.speciesId,form:p.formIndex,level:p.level,nature:p.getNature(),ivs:p.ivs.slice(),hp:p.hp,ability:p.getAbility().id,hasTrainer:p.hasTrainer(),tags:p.summonData.tags.map((t:any)=>t.tagType),stats:p.stats.slice()}));
  let result:any=null;
  if(mode===0)result=grip.apply(actors[holder],target<0?undefined:actors[target]);
  else s.applyModifiers(ContactHeldItemTransferChanceModifier,actors[holder].isPlayer(),actors[holder],target<0?undefined:actors[target]);
  const queue=pm.phaseQueue.levels.map((level:any[])=>level.map(q=>({name:q.phaseName,text:q.text??null,battler:q.battlerIndex??-1})));
  if(queue.some((l:any[])=>l.some(q=>q.text?.includes("Grip Claw")))){const key=inventory()[holder<2?0:1].at(-1)!.key;fires[key]=(fires[key]??0)+1;const context=`${mode}/${holder<2}/${target<2}`;contexts[context]=(contexts[context]??0)+1;}
  if(boundary&&mode===0&&result!==true)throw Error("native <= boundary transfer must be observable");
  rows.push(JSON.stringify({i,holder,target,mode,move:move.id,gripOwner:holder,battleSeed:s.currentBattle.battleSeed,waveSeed:s.waveSeed,turn:s.currentBattle.turn,input,beforeActors,result,output:inventory(),actors:actors.map(p=>({hp:p.hp,turnCount:p.tempSummonData.turnCount,waveTurnCount:p.tempSummonData.waveTurnCount,tags:p.summonData.tags.map((t:any)=>t.tagType),stats:p.stats.slice()})),queue,before,after:{global:Phaser.Math.RND.state(),battle:s.currentBattle.battleSeedState}}));
 }
 if(keys.some(k=>!fires[k])||Object.keys(contexts).length!==8)throw Error("native positive coverage missing");console.log("GRIPCLAW_POSITIVE "+JSON.stringify({fires,contexts}));
 const tiers=[0,1,2].map(pool=>getModifierType(modifierTypes.GRIP_CLAW).getOrInferTier(pool));
 rows.push(`GRIPCLAW_TIERS ${tiers.join(" ")}`);
 rows.push(`GRIPCLAW_COMPLETE ${N}`);fs.writeFileSync(process.env.GRIP_OUT??"/tmp/grip-claw.jsonl",rows.join("\n")+"\n");
},600000);
