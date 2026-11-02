import { BattleScene } from "#app/battle-scene";
import { getGameMode } from "#app/game-mode";
import { AbilityId } from "#enums/ability-id";
import { GameModes } from "#enums/game-modes";
import { MoveId } from "#enums/move-id";
import { SpeciesId } from "#enums/species-id";
import { BattlerTagType } from "#enums/battler-tag-type";
import { FaintPhase } from "#phases/faint-phase";
import { GameManager } from "#test/framework/game-manager";
import Phaser from "phaser";
import fs from "node:fs";
import { gzipSync } from "node:zlib";
import { it,expect,vi } from "vitest";
it('native first-form faint prefix and mode admission',async()=>{
 const native=BattleScene.prototype.randBattleSeedInt;expect(vi.isMockFunction(native)).toBe(false);
 const game=new GameManager(new Phaser.Game({type:Phaser.HEADLESS}));
 game.override.ability(AbilityId.BALL_FETCH).enemyAbility(AbilityId.BALL_FETCH).passiveAbility(AbilityId.NONE).enemyPassiveAbility(AbilityId.NONE).enemySpecies(SpeciesId.ETERNATUS).moveset(MoveId.SPLASH).enemyMoveset(MoveId.SPLASH).disableTrainerWaves().mysteryEncounterChance(0);
 await game.classicMode.startBattle(SpeciesId.MEW);BattleScene.prototype.randBattleSeedInt=native;
 const s:any=game.scene,b:any=s.currentBattle,pm:any=s.phaseManager,actors=[s.getPlayerParty()[0],s.getEnemyParty()[0]];
 actors.forEach((p:any,k:number)=>{p.id=101+k;p.formIndex=0;});
 vi.spyOn(pm,'shiftPhase').mockImplementation(()=>{});
 let called=false,prefix:any;
 const snapshot=(p:any)=>({hp:p.hp,tags:p.summonData.tags.map((t:any)=>t.tagType),queue:p.getMoveQueue().map((m:any)=>m.move),participants:[...b.playerParticipantIds].sort((a:any,z:any)=>a-z),global:Phaser.Math.RND.state(),battle:b.battleSeedState});
 vi.spyOn(FaintPhase.prototype as any,'doFaint').mockImplementation(function(this:any){called=true;prefix=snapshot(this.getPokemon());});
 const rows:string[]=['FINAL_BOSS_FAINT 1 480'];let index=0;
 const render=(o:any,queue:any)=>{
  expect(o.tags.every((t:any)=>t==='CHARGING')).toBe(true);
  return [`S ${[o.hp,o.tags.length,o.queue.length,...o.queue,o.participants.length,...o.participants].join(' ')}`,`R ${o.global.split(',').slice(1).join(' ')}`,`B ${o.battle.split(',').slice(1).join(' ')}`,`Q ${[queue.length,...queue.flatMap((l:any[])=>[l.length,...l.flatMap((q:any)=>{expect(q.name).toBe('DamageAnimPhase');return [q.battler,q.amount,q.result];})])].join(' ')}`];
 };
 for(const mode of [0,1,2,3,4])for(const wave of [199,200,201,5850])for(const side of [0,1])for(const hp of [0,1])for(const count of [0,1,4])for(const tag of [false,true]){
  s.gameMode=getGameMode(mode as GameModes);b.gameMode=s.gameMode;b.waveIndex=wave;b.double=false;b.playerParticipantIds.clear();pm.clearAllPhases();
  for(const p of actors){p.hp=100;p.status=null;p.resetSummonData();p.resetTurnData();s.field.add(p);}
  const p:any=actors[side];p.hp=hp;
  for(let i=0;i<count;i++)p.getMoveQueue().push({move:MoveId.DIG,targets:[side?0:2],useMode:0});
  if(tag)p.addTag(BattlerTagType.CHARGING,1,MoveId.DIG,p.id);
  Phaser.Math.RND.sow([`faint-prefix-${index}`]);b.battleSeedState=Phaser.Math.RND.state();called=false;prefix=null;
  const before=snapshot(p),beforeQueue=pm.phaseQueue.levels.map((l:any[])=>[...l]),phase:any=new FaintPhase(side?2:0,true);pm.currentPhase=phase;phase.start();
  const queue=pm.phaseQueue.levels.map((level:any[])=>level.map((q:any)=>({name:q.phaseName,battler:q.battlerIndex,amount:q.amount,result:q.damageResult})));
  rows.push(`C ${index++} ${mode} ${wave} ${side} ${hp} ${count} ${+tag} ${+!!b.isClassicFinalBoss}`, ...render(before,beforeQueue),`CALLED ${+called}`,...render(called?prefix:snapshot(p),queue));
 }
 expect(index).toBe(480);rows.push('COMPLETE 480');fs.writeFileSync(process.env.FINAL_FAINT_OUT!,gzipSync(rows.join('\n')+'\n'));
},120000);
