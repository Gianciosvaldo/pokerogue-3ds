/* Actual EncounterPhase.end queue, including loaded/single/double boundaries. */
import { BattleScene } from "#app/battle-scene";
import { Trainer } from "#field/trainer";
import { TrainerType } from "#enums/trainer-type";
import { TrainerVariant } from "#enums/trainer-variant";
import { audioManager } from "#app/global-audio-manager";
import { getGameMode } from "#app/game-mode";
import { AbilityId } from "#enums/ability-id";
import { SpeciesId } from "#enums/species-id";
import { GameManager } from "#test/framework/game-manager";
import Phaser from "phaser";
import * as fs from "node:fs";
import { it,vi } from "vitest";
it("actual encounter end",async()=>{
 const common=process.env.ENCOUNTER_COMMON==="1";
 const N=Number(process.env.ENCOUNTER_N??8192),seed=Number(process.env.ENCOUNTER_SEED??777);
 if(!process.versions.node.startsWith("22.")||!Number.isSafeInteger(N)||N<512)throw Error("Node22,N>=512");
 const realBattleRoll=BattleScene.prototype.randBattleSeedInt;
 const g=new GameManager(new Phaser.Game({type:Phaser.HEADLESS}));g.override.battleStyle("double").ability(AbilityId.BALL_FETCH).enemyAbility(AbilityId.BALL_FETCH).enemySpecies(SpeciesId.MAGIKARP);await g.classicMode.startBattle(SpeciesId.BULBASAUR,SpeciesId.CHARMANDER,SpeciesId.SQUIRTLE);BattleScene.prototype.randBattleSeedInt=realBattleRoll;
 const s:any=g.scene,pm:any=s.phaseManager,players=s.getPlayerParty().slice(),enemies=s.getEnemyParty().slice();vi.spyOn(pm,"shiftPhase").mockImplementation(()=>{});
 const trainer=common?new Trainer(TrainerType.YOUNGSTER,TrainerVariant.DEFAULT,0):null;
 if(common){vi.spyOn(audioManager,"playBgm").mockImplementation(()=>{});vi.spyOn(s.pbTray,"showPbTray").mockImplementation(()=>{});vi.spyOn(s.pbTrayEnemy,"showPbTray").mockImplementation(()=>{});
 vi.spyOn(s.ui,"showDialogue").mockImplementation((_text:any,_name:any,_delay:any,callback:any)=>callback?.());
 vi.spyOn(s.charSprite,"hide").mockImplementation(async()=>{});vi.spyOn(s.charSprite,"showCharacter").mockImplementation(async()=>{});
 vi.spyOn(s,"showFieldOverlay").mockImplementation(async()=>{});vi.spyOn(s,"hideFieldOverlay").mockImplementation(async()=>{});}
 const rows=[`ENCOUNTER_END 2 ${N} ${seed}`];
 for(let i=0;i<N;i++){
  const double=!!(i%2),loaded=!!(Math.floor(i/2)%2),count=1+Math.floor(i/4)%3,enemyCount=1+Math.floor(i/12)%2,mode=Math.floor(i/24)%5,battleType=common?1:Math.floor(i/120)%3,wave=[1,2,10,200,250,1000][Math.floor(i/360)%6],fieldMask=Math.floor(i/2160)%8;
  s.party=players.slice(0,count);s.currentBattle.enemyParty=enemies.slice(0,enemyCount);s.currentBattle.double=double;s.currentBattle.battleType=battleType;s.currentBattle.waveIndex=wave;s.currentBattle.trainer=trainer;s.gameMode=getGameMode(mode);s.currentBattle.gameMode=s.gameMode;s.modifiers=[];s.enemyModifiers=[];s.field.removeAll();s.field.add(s.add.container(0,0));
  players.forEach((p:any,k:number)=>{p.hp=k===0||i%7!==0?p.getMaxHp():0;p.switchOutStatus=!!(i%11===0&&k===1);if(fieldMask&(1<<k))s.field.add(p);});
  enemies.forEach((p:any,k:number)=>{p.hp=k===0||i%5?p.getMaxHp():0;p.shiny=!!(Math.floor(i/(2+k))%2);s.field.add(p);});pm.clearAllPhases();
  Phaser.Math.RND.sow([`encounter-${seed}-${i}`]);s.currentBattle.battleSeedState=null;s.currentBattle.randSeedInt(2);const before=[Phaser.Math.RND.state(),s.currentBattle.battleSeedState];
  const input={common,double,loaded,count,enemyCount,mode,battleType,wave,players:s.party.map((p:any)=>[p.hp,+p.isOnField(),+p.switchOutStatus]),enemies:s.currentBattle.enemyParty.map((p:any)=>[+p.shiny,p.hp])};
  const ph=pm.create("EncounterPhase",loaded);pm.currentPhase=ph;if(common){ph.doEncounterCommon(false);for(let tick=0;tick<8;tick++)await Promise.resolve();}else ph.end();
  const dump=(p:any)=>[p.phaseName,p.battlerIndex??-1,p.fieldIndex??-1,p.partyMemberIndex??-1,p.player===undefined?-1:+p.player,p.double===undefined?-1:+p.double,p.useName===undefined?-1:+p.useName,p.phaseType??"-"];
  rows.push(JSON.stringify({i,input,queue:pm.phaseQueue.levels.map((q:any[])=>q.map(dump)),dynamic:[...pm.dynamicQueueManager.dynamicPhaseMap.entries()].filter(([_kind,q]:any)=>q.queue.length).map(([kind,q]:any)=>[kind,q.queue.map(dump)]),before,after:[Phaser.Math.RND.state(),s.currentBattle.battleSeedState]}));
 }
 rows.push(`ENCOUNTER_END_COMPLETE ${N}`);fs.writeFileSync(process.env.ENCOUNTER_OUT??"/tmp/encounter-end.jsonl",rows.join("\n")+"\n");
},600000);
