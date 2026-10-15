import { BattleScene } from "#app/battle-scene";
/* Actual scene controller, moveset construction and phase queue; visual transport only. */
import { getGameMode } from "#app/game-mode";
import { audioManager } from "#app/global-audio-manager";
import { AbilityId } from "#enums/ability-id";
import { SpeciesId } from "#enums/species-id";
import { GameManager } from "#test/framework/game-manager";
import Phaser from "phaser";
import * as fs from "node:fs";
import { it,vi } from "vitest";
it("actual final boss phase two controller",async()=>{
 const N=Number(process.env.FINAL_BOSS_N??16384);
 if(!process.versions.node.startsWith("22.")||!Number.isSafeInteger(N)||N<512)throw Error("Node22,N>=512");
 const realBattleRoll=BattleScene.prototype.randBattleSeedInt;
 const g=new GameManager(new Phaser.Game({type:Phaser.HEADLESS}));g.override.ability(AbilityId.BALL_FETCH).enemyAbility(AbilityId.BALL_FETCH).enemySpecies(SpeciesId.ETERNATUS);await g.classicMode.startBattle(SpeciesId.BULBASAUR,SpeciesId.CHARMANDER,SpeciesId.SQUIRTLE);BattleScene.prototype.randBattleSeedInt=realBattleRoll;
 const s:any=g.scene,pm:any=s.phaseManager,enemy:any=g.field.getEnemyPokemon(),party:any[]=s.getPlayerParty();
 vi.spyOn(pm,"shiftPhase").mockImplementation(()=>{});
 vi.spyOn(audioManager,"fadeOutBgm").mockImplementation(()=>{});
 vi.spyOn(s,"setFieldScale").mockImplementation(()=>{});
 vi.spyOn(s.ui,"showDialogue").mockImplementation((_text:any,_name:any,_delay:any,callback:any)=>callback?.());
 const rows=[`FINAL_BOSS 1 ${N}`];
 for(let i=0;i<N;i++){
  const method=Math.floor(i/192)%3,mode=Math.floor(i/576)%5,wave=[199,200,250][Math.floor(i/2880)%3];s.gameMode=getGameMode(mode);s.currentBattle.gameMode=s.gameMode;s.currentBattle.waveIndex=wave;
  const p=i%2?enemy:party[0],boss=Math.floor(i/2)%2,form=Math.floor(i/4)%2,segment=Math.floor(i/8)%3,alive=Math.floor(i/24)%4,onfield=Math.floor(i/96)%2;
  enemy.formIndex=0;enemy.bossSegments=boss?4:0;enemy.bossSegmentIndex=segment;p.formIndex=form;p.hp=1;
  s.field.removeAll();s.field.add(s.add.container(0,0));s.field.add(enemy);party.forEach((q:any,k:number)=>{q.hp=k<alive?Math.max(1,q.getMaxHp()):0;if(k===0||!!onfield)s.field.add(q);});
  s.enemyModifiers=[];s.modifiers=[];s.currentBattle.double=false;pm.clearAllPhases();
  Phaser.Math.RND.sow([`final-boss-${i}`]);s.currentBattle.battleSeedState=null;
  const before=Phaser.Math.RND.state();const inputParty=party.map((q:any)=>[q.hp,+q.isOnField()]);const inputMoves=p.moveset.map((m:any)=>[m.moveId,m.ppUsed,m.ppUp,m.getMovePp()]);if(method===0)s.initFinalBossPhaseTwo(p);
  else {const ph:any=pm.create(method===1?"DamageAnimPhase":"PostTurnStatusEffectPhase",p.isEnemy()?2:0,1);pm.currentPhase=ph;if(method===1)ph.end();else {p.status=null;ph.start();}}
  const meta=(q:any)=>[q.phaseName,q.battlerIndex??-1,q.pokemon?.id===p.id?1:0,q.fieldIndex??-1,q.player===undefined?-1:+q.player,q.partyMemberIndex??-1,q.phaseType??"-",q.formChange?.formKey??"-",+(q.constructor.name==="PostSummonActivateAbilityPhase")];
  const queue=pm.phaseQueue.levels.flat().map(meta);
  const dynamic=[...pm.dynamicQueueManager.dynamicPhaseMap.entries()].flatMap(([kind,q]:any)=>q.queue.map((ph:any)=>[kind,...meta(ph)]));
  rows.push(JSON.stringify({i,method,mode,wave,enemy:+p.isEnemy(),boss:+p.isBoss(),form,segment,alive,onfield,inputParty,inputMoves,double:+s.currentBattle.double,moves:p.moveset.map((m:any)=>[m.moveId,m.ppUsed,m.ppUp,m.getMovePp()]),queueLevels:pm.phaseQueue.levels.map((level:any[])=>level.length),dynamic,mods:s.enemyModifiers.map((m:any)=>[m.constructor.name,m.pokemonId===p.id?1:0,m.stackCount,+m.isTransferable]),queue,rngBefore:before,rngAfter:Phaser.Math.RND.state()}));
 }
 rows.push(`FINAL_BOSS_COMPLETE ${N}`);fs.writeFileSync(process.env.FINAL_BOSS_OUT??"/tmp/final-boss.jsonl",rows.join("\n")+"\n");
},600000);
