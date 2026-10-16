/* Actual SummonPhase.start through native fieldSetup/onEnd; visual callbacks only. */
import { BattleScene } from "#app/battle-scene";
import { audioManager } from "#app/global-audio-manager";
import { AbilityId } from "#enums/ability-id";
import { SpeciesId } from "#enums/species-id";
import { TrainerType } from "#enums/trainer-type";
import { TrainerVariant } from "#enums/trainer-variant";
import { Trainer } from "#field/trainer";
import { GameManager } from "#test/framework/game-manager";
import Phaser from "phaser";
import * as fs from "node:fs";
import { it,vi } from "vitest";
it("actual summon entry",async()=>{
 const N=Number(process.env.SUMMON_N??8192),SEED=Number(process.env.SUMMON_SEED??777);
 if(!process.versions.node.startsWith("22.")||!Number.isSafeInteger(N)||N<512)throw Error("Node22,N>=512");
 const native=BattleScene.prototype.randBattleSeedInt;
 const g=new GameManager(new Phaser.Game({type:Phaser.HEADLESS}));g.override.battleStyle("double").ability(AbilityId.BALL_FETCH).enemyAbility(AbilityId.BALL_FETCH).enemySpecies(SpeciesId.MAGIKARP);await g.classicMode.startBattle(SpeciesId.BULBASAUR,SpeciesId.CHARMANDER,SpeciesId.SQUIRTLE);BattleScene.prototype.randBattleSeedInt=native;
 const s:any=g.scene,pm:any=s.phaseManager,players=s.getPlayerParty().slice(),enemies=[...s.getEnemyParty(),s.addEnemyPokemon(SpeciesId.MAGIKARP,20)],all=[...players,...enemies];
 const trainer=new Trainer(TrainerType.YOUNGSTER,TrainerVariant.DEFAULT,0);s.currentBattle.trainer=trainer;s.currentBattle.battleType=1;
 g.override.ability(AbilityId.NONE).enemyAbility(AbilityId.NONE);vi.spyOn(pm,"shiftPhase").mockImplementation(()=>{});
 const texts:string[]=[];
 vi.spyOn(s.ui,"showText").mockImplementation((text:any,_delay:any,callback:any)=>{if(typeof text==="string")texts.push(text);callback?.();});
 vi.spyOn(s.time,"delayedCall").mockImplementation((_delay:any,callback:any)=>{callback?.();return {remove(){},destroy(){}};});
 vi.spyOn(s.tweens,"add").mockImplementation((config:any)=>{config.onStart?.();config.onComplete?.();return {remove(){},stop(){},isPlaying:()=>false};});
 vi.spyOn(s,"updateFieldScale").mockImplementation(()=>{});vi.spyOn(audioManager,"playSound").mockImplementation(()=>{});
 vi.spyOn(s.pbTray,"hide").mockImplementation(()=>{});vi.spyOn(s.pbTrayEnemy,"hide").mockImplementation(()=>{});
 for(const p of all){vi.spyOn(p,"updateInfo").mockImplementation(async()=>{});for(const method of ["showInfo","playAnim","cry","tint","untint"]){vi.spyOn(p,method as any).mockImplementation(()=>{});}}
 let seed=SEED;const rnd=(n:number)=>{seed=(Math.imul(seed,1103515245)+12345)>>>0;return(seed>>>8)%n;};
 const dumpPhase=(p:any)=>[p.phaseName,p.battlerIndex??-1,p.fieldIndex??-1,p.partyMemberIndex??-1,p.player===undefined?-1:+p.player,p.double===undefined?-1:+p.double,p.useName===undefined?-1:+p.useName,p.phaseType??"-"];
 const actor=(p:any)=>({id:p.id,species:p.species.speciesId,form:p.formIndex,level:p.level,nature:p.getNature(),ivs:p.ivs.slice(),stats:p.stats.slice(),hp:p.hp,ability:p.getAbility().id,onField:+p.isOnField(),switching:+p.switchOutStatus,turnCount:p.tempSummonData.turnCount,waveTurnCount:p.tempSummonData.waveTurnCount,summoned:+p.turnData.summonedThisTurn,shiny:+p.shiny,hitCount:p.turnData.hitCount});
 vi.spyOn(console,"log").mockImplementation(()=>{});vi.spyOn(console,"warn").mockImplementation(()=>{});
 const rows=[`SUMMON_ENTRY 3 ${N} ${SEED}`];
 for(let i=0;i<N;i++){
  const double=!!(i%2),side=Math.floor(i/2)%2,field=double?Math.floor(i/4)%2:0,faint=!!(Math.floor(i/8)%2);
  s.party=players.slice();s.currentBattle.enemyParty=enemies.slice();s.currentBattle.double=double;s.currentBattle.waveIndex=1;s.modifiers=[];s.enemyModifiers=[];pm.clearAllPhases();s.field.removeAll();s.field.add(s.add.container(0,0));
  for(let k=0;k<all.length;k++){const p=all[k];p.resetSummonData();p.resetTurnData();p.customPokemonData.ability=AbilityId.NONE;p.customPokemonData.passive=AbilityId.NONE;p.passive=false;p.nature=rnd(25);p.customPokemonData.nature=-1;p.stats=Array(6).fill(0);p.hp=0;p.calculateStats();p.hp=p.getMaxHp();if(i%5===0)p.nature=(p.nature+1)%25;p.turnData.hitCount=1+i%3;p.shiny=!!(rnd(4)===0);p.switchOutStatus=false;p.tempSummonData.turnCount=i%4;p.tempSummonData.waveTurnCount=i%7;if(k%3<(double?2:1)&&!(Math.floor(k/3)===side&&k%3===field))s.field.add(p);}
  if(faint)all[side*3+field].hp=0;
  texts.length=0;Phaser.Math.RND.sow([`summon-${SEED}-${i}`]);s.currentBattle.battleSeed=`summon-${SEED}-${i}`;s.currentBattle.battleSeedState=null;s.currentBattle.randSeedInt(2);
  const before={global:Phaser.Math.RND.state(),battle:s.currentBattle.battleSeedState};const actors=all.map(actor);
  const ph=pm.create("SummonPhase",field,side===0,false);pm.currentPhase=ph;ph.start();for(let tick=0;tick<8;tick++)await Promise.resolve();
  rows.push(JSON.stringify({i,double,side,field,faint,actors,trainerNames:[trainer.getName(1),trainer.getName(2)],before,after:{global:Phaser.Math.RND.state(),battle:s.currentBattle.battleSeedState},output:all.map(actor),party:[s.party.map((p:any)=>p.id),s.currentBattle.enemyParty.map((p:any)=>p.id)],fieldIds:s.getField().map((p:any)=>p?.id??null),texts:texts.slice(),queue:pm.phaseQueue.levels.map((q:any[])=>q.map(dumpPhase)),dynamic:[...pm.dynamicQueueManager.dynamicPhaseMap.entries()].filter(([_kind,q]:any)=>q.queue.length).map(([kind,q]:any)=>[kind,q.queue.map(dumpPhase)])}));
 }
 rows.push(`SUMMON_ENTRY_COMPLETE ${N}`);fs.writeFileSync(process.env.SUMMON_OUT??"/tmp/summon-entry.jsonl",rows.join("\n")+"\n");
},600000);
