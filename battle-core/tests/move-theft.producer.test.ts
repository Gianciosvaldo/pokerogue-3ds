import { BattleScene } from "#app/battle-scene";
/* Native Thief/Covet override and attribute dispatch; UI/phase advancement only mocked. */
import { generateModifierType } from "#mystery-encounters/encounter-phase-utils";
import { allMoves, modifierTypes } from "#data/data-lists";
import { getModifierType } from "#utils/modifier-utils";
import { AbilityId } from "#enums/ability-id";
import { SpeciesId } from "#enums/species-id";
import { MoveId } from "#enums/move-id";
import { applyMoveAttrs } from "#moves/apply-attrs";
import { GameManager } from "#test/framework/game-manager";
import Phaser from "phaser";
import * as fs from "node:fs";
import { it,vi } from "vitest";
it("actual Thief and Covet callbacks",async()=>{
 const N=Number(process.env.THEFT_N??16384),SEED=Number(process.env.THEFT_SEED??777);
 if(!process.versions.node.startsWith("22.")||!Number.isSafeInteger(N)||N<16384)throw Error("Node22,N>=16384");
 const realBattleRoll=BattleScene.prototype.randBattleSeedInt;
 const g=new GameManager(new Phaser.Game({type:Phaser.HEADLESS}));g.override.battleStyle("double").ability(AbilityId.BALL_FETCH).enemyAbility(AbilityId.BALL_FETCH).enemySpecies(SpeciesId.MAGIKARP).moveset(MoveId.SPLASH).enemyMoveset(MoveId.SPLASH);await g.classicMode.startBattle(SpeciesId.BULBASAUR,SpeciesId.CHARMANDER,SpeciesId.SQUIRTLE);BattleScene.prototype.randBattleSeedInt=realBattleRoll;
 const s:any=g.scene,pm:any=s.phaseManager,actors:any[]=s.getField();if(actors.length!==4||actors.some(p=>!p))throw Error("natural double field required");
 g.override.ability(AbilityId.NONE).enemyAbility(AbilityId.NONE);vi.spyOn(pm,"shiftPhase").mockImplementation(()=>{});
 for(const p of actors){vi.spyOn(p,"updateInfo").mockImplementation(async()=>{});}
 const nativeIds=actors.map(p=>p.id);
 const keys=["LEFTOVERS","SHELL_BELL","FOCUS_BAND","QUICK_CLAW","KINGS_ROCK","WHITE_HERB","MINI_BLACK_HOLE","BERRY","WIDE_LENS","MULTI_LENS","SCOPE_LENS","TOXIC_ORB","FLAME_ORB","SOUL_DEW","GOLDEN_PUNCH","ATTACK_TYPE_BOOSTER","REVIVER_SEED"];
 let seed=SEED;const rnd=(n:number)=>{seed=(Math.imul(seed,1103515245)+12345)>>>0;return(seed>>>8)%n;};
 const fires:any={},contexts:any={};
 const rows=[`MOVETHEFT 1 ${N} ${SEED}`],index=(id:number)=>actors.findIndex(p=>p.id===id);
 const inventory=()=>[s.modifiers,s.enemyModifiers].map((list:any[])=>list.map(m=>({key:m.type.id,arg:m.berryType??m.moveType??-1,owner:index(m.pokemonId),stack:m.stackCount,virtual:m.virtualStackCount,transferable:+m.isTransferable,max:m.getMaxStackCount(),tier:m.type.tier??-1})));
 for(let i=0;i<N;i++){
  s.modifiers=[];s.enemyModifiers=[];pm.clearAllPhases();const holder=i%4,target=(holder+1+Math.floor(i/4)%3)%4,mode=Math.floor(i/12)%2,move=allMoves[i%2?MoveId.THIEF:MoveId.COVET];actors.forEach((p,k)=>p.id=nativeIds[k]);if(i%101===0)actors[Math.floor(i/101)%4].id=0;
  for(let k=0;k<4;k++){const p=actors[k];p.resetSummonData();p.resetTurnData();p.tempSummonData.turnCount=0;p.tempSummonData.waveTurnCount=0;if(k>=2)p.trainerSlot=Math.floor(i/24)%2;p.customPokemonData.ability=[AbilityId.NONE,AbilityId.STICKY_HOLD,AbilityId.UNBURDEN][Math.floor(i/48+k)%3];p.customPokemonData.passive=AbilityId.NONE;p.passive=false;p.nature=rnd(25);p.customPokemonData.nature=-1;p.stats=Array(6).fill(0);p.hp=0;p.calculateStats();p.hp=Math.floor(i/12)%5===k?0:p.getMaxHp();}
  if(i%17===0)actors.forEach((p,k)=>{if((k<2)!==(holder<2))p.hp=0;});
  const make=(key:string,owner:number)=>{const args=key==="BERRY"?[rnd(11)]:key==="ATTACK_TYPE_BOOSTER"?[rnd(18)]:undefined;const type:any=args?generateModifierType((modifierTypes as any)[key],args):getModifierType((modifierTypes as any)[key]);if(!type)throw Error("modifier unavailable "+key);return type.newModifier(actors[owner]);};
  for(let k=0,amount=rnd(12);k<amount;k++){const owner=rnd(4),key=keys[rnd(keys.length)],m:any=make(key,owner);if([...s.modifiers,...s.enemyModifiers].some((old:any)=>old.pokemonId===actors[owner].id&&old.matchType(m)))continue;m.stackCount=1+rnd(Math.max(1,m.getMaxStackCount()));if(i%7===0)m.isTransferable=false;if(i%5===0)m.type.setTier(rnd(6));(actors[owner].isPlayer()?s.modifiers:s.enemyModifiers).push(m);}
  s.updateModifiers(true,true);s.updateModifiers(false,true);
  Phaser.Math.RND.sow([`transfer-${SEED}-${i}`]);s.currentBattle.battleSeed=`transfer-${SEED}-${i}`;s.currentBattle.turn=1+i%19;s.currentBattle.battleSeedState=null;s.currentBattle.randSeedInt(2);const before={global:Phaser.Math.RND.state(),battle:s.currentBattle.battleSeedState};const input=inventory();const beforeActors=actors.map(p=>({id:p.id,species:p.species.speciesId,form:p.formIndex,level:p.level,nature:p.getNature(),ivs:p.ivs.slice(),hp:p.hp,ability:p.getAbility().id,hasTrainer:p.hasTrainer(),tags:p.summonData.tags.map((t:any)=>t.tagType),stats:p.stats.slice()}));
  const attr:any=move.getAttrs("StealHeldItemChanceAttr")[0];if(!attr)throw Error("native theft attr missing");let result:any=null;
  if(mode===0)result=attr.apply(actors[holder],actors[target],move,[]);else applyMoveAttrs("StealHeldItemChanceAttr",actors[holder],actors[target],move);
  const queue=pm.phaseQueue.levels.map((level:any[])=>level.map(q=>({name:q.phaseName,text:q.text??null,battler:q.battlerIndex??-1})));
  if(queue.some((l:any[])=>l.some(q=>q.text?.includes(" stole\n")))){const key=inventory()[holder<2?0:1].at(-1)!.key;fires[key]=(fires[key]??0)+1;const context=`${mode}/${holder<2}/${target<2}`;contexts[context]=(contexts[context]??0)+1;}
  rows.push(JSON.stringify({i,holder,target,mode,move:move.id,battleSeed:s.currentBattle.battleSeed,waveSeed:s.waveSeed,turn:s.currentBattle.turn,input,beforeActors,result,output:inventory(),actors:actors.map(p=>({hp:p.hp,turnCount:p.tempSummonData.turnCount,waveTurnCount:p.tempSummonData.waveTurnCount,tags:p.summonData.tags.map((t:any)=>t.tagType),stats:p.stats.slice()})),queue,before,after:{global:Phaser.Math.RND.state(),battle:s.currentBattle.battleSeedState}}));
 }
 if(keys.some(k=>!fires[k])||Object.keys(contexts).length!==8)throw Error("native positive coverage missing");console.log("MOVETHEFT_POSITIVE "+JSON.stringify({fires,contexts}));
 rows.push(`MOVETHEFT_COMPLETE ${N}`);fs.writeFileSync(process.env.THEFT_OUT??"/tmp/move-theft.jsonl",rows.join("\n")+"\n");
},600000);
