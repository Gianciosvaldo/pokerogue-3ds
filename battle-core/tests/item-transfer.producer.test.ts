import { BattleScene } from "#app/battle-scene";
/* Next batch: actual Mini Black Hole apply -> scene transfer -> inventory/callbacks. */
import { generateModifierType } from "#mystery-encounters/encounter-phase-utils";
import { modifierTypes } from "#data/data-lists";
import { getModifierType } from "#utils/modifier-utils";
import { AbilityId } from "#enums/ability-id";
import { SpeciesId } from "#enums/species-id";
import { MoveId } from "#enums/move-id";
import { GameManager } from "#test/framework/game-manager";
import Phaser from "phaser";
import * as fs from "node:fs";
import { it,vi } from "vitest";
it("actual held-item transfer API",async()=>{
 const turnEnd=process.env.TRANSFER_TURN_END==="1";
 const N=Number(process.env.TRANSFER_N??2048),SEED=Number(process.env.TRANSFER_SEED??777);
 if(!process.versions.node.startsWith("22.")||!Number.isSafeInteger(N)||N<512)throw Error("Node22,N>=512");
 const realBattleRoll=BattleScene.prototype.randBattleSeedInt;
 const g=new GameManager(new Phaser.Game({type:Phaser.HEADLESS}));g.override.battleStyle("double").ability(AbilityId.BALL_FETCH).enemyAbility(AbilityId.BALL_FETCH).enemySpecies(SpeciesId.MAGIKARP).moveset(MoveId.SPLASH).enemyMoveset(MoveId.SPLASH);await g.classicMode.startBattle(SpeciesId.BULBASAUR,SpeciesId.CHARMANDER,SpeciesId.SQUIRTLE);BattleScene.prototype.randBattleSeedInt=realBattleRoll;
 const s:any=g.scene,pm:any=s.phaseManager,actors:any[]=s.getField();if(actors.length!==4||actors.some(p=>!p))throw Error("natural double field required");
 g.override.ability(AbilityId.NONE).enemyAbility(AbilityId.NONE);vi.spyOn(pm,"shiftPhase").mockImplementation(()=>{});
 for(const p of actors){vi.spyOn(p,"updateInfo").mockImplementation(async()=>{});}
 const nativeIds=actors.map(p=>p.id);
 const keys=["LEFTOVERS","SHELL_BELL","FOCUS_BAND","QUICK_CLAW","KINGS_ROCK","WHITE_HERB","MINI_BLACK_HOLE","BERRY","WIDE_LENS","MULTI_LENS","SCOPE_LENS","TOXIC_ORB","FLAME_ORB","SOUL_DEW","GOLDEN_PUNCH","ATTACK_TYPE_BOOSTER","REVIVER_SEED"];
 if(turnEnd){keys.splice(0,keys.length,"MINI_BLACK_HOLE","BERRY","WIDE_LENS","MULTI_LENS","SCOPE_LENS","SOUL_DEW","ATTACK_TYPE_BOOSTER","REVIVER_SEED");}
 let seed=SEED;const rnd=(n:number)=>{seed=(Math.imul(seed,1103515245)+12345)>>>0;return(seed>>>8)%n;};
 const rows=[`TRANSFER 1 ${N} ${SEED}`],index=(id:number)=>actors.findIndex(p=>p.id===id);
 const inventory=()=>[s.modifiers,s.enemyModifiers].map((list:any[])=>list.map(m=>({key:m.type.id,arg:m.berryType??m.moveType??-1,owner:index(m.pokemonId),stack:m.stackCount,virtual:m.virtualStackCount,transferable:+m.isTransferable,max:m.getMaxStackCount()})));
 for(let i=0;i<N;i++){
  s.modifiers=[];s.enemyModifiers=[];pm.clearAllPhases();const holder=i%4;actors.forEach((p,k)=>p.id=nativeIds[k]);if(i%101===0)actors[Math.floor(i/101)%4].id=0;
  for(let k=0;k<4;k++){const p=actors[k];p.resetSummonData();p.resetTurnData();p.tempSummonData.turnCount=0;p.tempSummonData.waveTurnCount=0;p.customPokemonData.ability=k===holder?AbilityId.NONE:[AbilityId.NONE,AbilityId.STICKY_HOLD,AbilityId.UNBURDEN][Math.floor(i/4)%3];p.customPokemonData.passive=AbilityId.NONE;p.passive=false;p.nature=rnd(25);p.customPokemonData.nature=-1;p.stats=Array(6).fill(0);p.hp=0;p.calculateStats();p.hp=Math.floor(i/12)%5===k?0:p.getMaxHp();}
  if(i%17===0)actors.forEach((p,k)=>{if((k<2)!==(holder<2))p.hp=0;});
  const make=(key:string,owner:number)=>{const args=key==="BERRY"?[rnd(11)]:key==="ATTACK_TYPE_BOOSTER"?[rnd(18)]:undefined;const type:any=args?generateModifierType((modifierTypes as any)[key],args):getModifierType((modifierTypes as any)[key]);if(!type)throw Error("modifier unavailable "+key);return type.newModifier(actors[owner]);};
  const black:any=make("MINI_BLACK_HOLE",holder);if(i%13===0)black.setTransferrableFalse();(actors[holder].isPlayer()?s.modifiers:s.enemyModifiers).push(black);
  for(let k=0,amount=rnd(12);k<amount;k++){const owner=rnd(4),key=keys[rnd(keys.length)],m:any=make(key,owner);if([...s.modifiers,...s.enemyModifiers].some((old:any)=>old.pokemonId===actors[owner].id&&old.matchType(m)))continue;m.stackCount=1+rnd(Math.max(1,m.getMaxStackCount()));if(key==="MINI_BLACK_HOLE"&&i%2)m.setTransferrableFalse();(actors[owner].isPlayer()?s.modifiers:s.enemyModifiers).push(m);}
  s.updateModifiers(true,true);s.updateModifiers(false,true);
  Phaser.Math.RND.sow([`transfer-${SEED}-${i}`]);s.currentBattle.battleSeed=`transfer-${SEED}-${i}`;s.currentBattle.turn=1+i%19;s.currentBattle.battleSeedState=null;s.currentBattle.randSeedInt(2);const before={global:Phaser.Math.RND.state(),battle:s.currentBattle.battleSeedState};const input=inventory();const beforeActors=actors.map(p=>({id:p.id,species:p.species.speciesId,form:p.formIndex,level:p.level,nature:p.getNature(),ivs:p.ivs.slice(),hp:p.hp,ability:p.getAbility().id,tags:p.summonData.tags.map((t:any)=>t.tagType),stats:p.stats.slice()}));
  const should=black.shouldApply(actors[holder]);let result=false;if(turnEnd){const ph=pm.create("TurnEndPhase");pm.currentPhase=ph;ph.start();}else result=should?black.apply(actors[holder]):false;
  const queue=pm.phaseQueue.levels.map((level:any[])=>level.map(q=>({name:q.phaseName,text:q.text??null,battler:q.battlerIndex??-1})));
  rows.push(JSON.stringify({i,holder,turnEnd,battleSeed:s.currentBattle.battleSeed,waveSeed:s.waveSeed,turn:s.currentBattle.turn-(turnEnd?1:0),input,beforeActors,should,result,output:inventory(),actors:actors.map(p=>({hp:p.hp,turnCount:p.tempSummonData.turnCount,waveTurnCount:p.tempSummonData.waveTurnCount,tags:p.summonData.tags.map((t:any)=>t.tagType),stats:p.stats.slice()})),queue,before,after:{global:Phaser.Math.RND.state(),battle:s.currentBattle.battleSeedState}}));
 }
 rows.push(`TRANSFER_COMPLETE ${N}`);fs.writeFileSync(process.env.TRANSFER_OUT??"/tmp/transfer.jsonl",rows.join("\n")+"\n");
},600000);
