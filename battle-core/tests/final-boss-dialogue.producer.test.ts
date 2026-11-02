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
import i18next from "i18next";
import fs from "node:fs";
import { gzipSync } from "node:zlib";
import { it,expect,vi } from "vitest";

it("native final-boss dialogue before ordinary faint",async()=>{
 const native=BattleScene.prototype.randBattleSeedInt;expect(vi.isMockFunction(native)).toBe(false);
 const game=new GameManager(new Phaser.Game({type:Phaser.HEADLESS}));
 game.override.ability(AbilityId.BALL_FETCH).enemyAbility(AbilityId.BALL_FETCH).passiveAbility(AbilityId.NONE).enemyPassiveAbility(AbilityId.NONE).enemySpecies(SpeciesId.ETERNATUS).moveset(MoveId.SPLASH).enemyMoveset(MoveId.SPLASH).disableTrainerWaves().mysteryEncounterChance(0);
 await game.classicMode.startBattle(SpeciesId.ETERNATUS);BattleScene.prototype.randBattleSeedInt=native;
 const s:any=game.scene,b:any=s.currentBattle,pm:any=s.phaseManager,actors=[s.getPlayerParty()[0],s.getEnemyParty()[0]];
 actors.forEach((p:any,k:number)=>{p.id=101+k;});vi.spyOn(pm,'shiftPhase').mockImplementation(()=>{});
 let called=false,prefix:any,dialogue:any,complete:(()=>void)|undefined;
 const snapshot=(p:any)=>({hp:p.hp,form:p.formIndex,status:p.status?.effect??0,field:p.isOnField(),tags:p.summonData.tags.map((t:any)=>t.tagType),queue:p.getMoveQueue().map((m:any)=>m.move),participants:[...b.playerParticipantIds].sort((a:any,z:any)=>a-z),counts:[s.arena.playerFaints,s.arena.enemyFaints,b.playerFaintsHistory.length,b.enemyFaintsHistory.length],global:Phaser.Math.RND.state(),battle:b.battleSeedState});
 vi.spyOn(FaintPhase.prototype as any,'doFaint').mockImplementation(function(this:any){called=true;prefix=snapshot(this.getPokemon());});
 vi.spyOn(s.ui,'showDialogue').mockImplementation((key:any,name:any,delay:any,callback:any)=>{
  expect(dialogue).toBeUndefined();expect(delay).toBeNull();expect(called).toBe(false);
  dialogue={key,name,text:i18next.t(key),before:snapshot(actors[1])};complete=callback;
 });
 const hex=(x:string)=>Buffer.from(x).toString('hex');
 const rows:string[]=['FINAL_BOSS_DIALOGUE 1 640'];let index=0;
 const render=(o:any)=>{expect(o.tags.every((t:any)=>t==='CHARGING')).toBe(true);return [`S ${[o.hp,o.form,+o.field,o.tags.length,o.queue.length,...o.queue,o.participants.length,...o.participants,...o.counts].join(' ')}`,`R ${o.global.split(',').slice(1).join(' ')}`,`B ${o.battle.split(',').slice(1).join(' ')}`];};
 for(const mode of [0,1,2,3,4])for(const wave of [199,200,201,5850])for(const side of [0,1])for(const form of [0,1])for(const hp of [0,1])for(const count of [0,4])for(const tag of [false,true]){
  s.gameMode=getGameMode(mode as GameModes);b.gameMode=s.gameMode;b.waveIndex=wave;b.double=false;b.playerParticipantIds.clear();b.playerFaintsHistory.length=b.enemyFaintsHistory.length=0;s.arena.playerFaints=s.arena.enemyFaints=0;pm.clearAllPhases();
  for(const p of actors){p.hp=100;p.formIndex=0;p.status=null;p.resetSummonData();p.resetTurnData();s.field.add(p);}
  const p:any=actors[side];p.hp=hp;p.formIndex=form;
  for(let i=0;i<count;i++)p.getMoveQueue().push({move:MoveId.DIG,targets:[side?0:2],useMode:0});
  if(tag)p.addTag(BattlerTagType.CHARGING,1,MoveId.DIG,p.id);
  Phaser.Math.RND.sow([`faint-dialogue-${index}`]);b.battleSeedState=Phaser.Math.RND.state();called=false;prefix=null;dialogue=undefined;complete=undefined;
  const before=snapshot(p),phase:any=new FaintPhase(side?2:0,true);pm.currentPhase=phase;phase.start();
  const waiting=!!complete;if(waiting){expect(called).toBe(false);expect(dialogue.before).toEqual(snapshot(p));complete!();expect(called).toBe(true);}
  const queue=pm.phaseQueue.levels.map((level:any[])=>level.map((q:any)=>({name:q.phaseName,battler:q.battlerIndex,amount:q.amount,result:q.damageResult})));
  rows.push(`C ${index++} ${mode} ${wave} ${side} ${form} ${hp} ${count} ${+tag}`, ...render(before),`D ${+waiting}`);
  if(waiting)rows.push(`K ${hex(dialogue.key)}`,`N ${hex(dialogue.name)}`,`T ${hex(dialogue.text)}`,...render(dialogue.before));
  rows.push(`CALLED ${+called}`,...render(called?prefix:snapshot(p)),`Q ${[queue.length,...queue.flatMap((l:any[])=>[l.length,...l.flatMap((q:any)=>{expect(q.name).toBe('DamageAnimPhase');return [q.battler,q.amount,q.result];})])].join(' ')}`);
 }
 expect(index).toBe(640);rows.push('COMPLETE 640');fs.writeFileSync(process.env.FINAL_DIALOGUE_OUT!,gzipSync(rows.join('\n')+'\n'));
},120000);
