/* Actual FaintPhase owner and Retaliate / Trainer.shouldTera consumers. */
import { BattleScene } from "#app/battle-scene";
import { audioManager } from "#app/global-audio-manager";
import { allMoves } from "#data/data-lists";
import { speciesDataRegistry } from "#app/globals/global-species-data-registry";
import { AbilityId } from "#enums/ability-id";
import { BattleType } from "#enums/battle-type";
import { MoveId } from "#enums/move-id";
import { SpeciesId } from "#enums/species-id";
import { TrainerSlot } from "#enums/trainer-slot";
import { TrainerType } from "#enums/trainer-type";
import { TrainerVariant } from "#enums/trainer-variant";
import { TeraAIMode } from "#enums/tera-ai-mode";
import { Trainer } from "#field/trainer";
import { FaintPhase } from "#phases/faint-phase";
import { NumberHolder } from "#utils/common";
import { GameManager } from "#test/framework/game-manager";
import Phaser from "phaser";
import fs from "node:fs";
import { expect,it,vi } from "vitest";
it("native repeated faint logs and their actual consumers",async()=>{
 const native=BattleScene.prototype.randBattleSeedInt;
 const g=new GameManager(new Phaser.Game({type:Phaser.HEADLESS}));
 g.override.battleStyle("double").ability(AbilityId.NONE).enemyAbility(AbilityId.NONE).passiveAbility(AbilityId.NONE).enemyPassiveAbility(AbilityId.NONE).moveset(MoveId.SPLASH).enemyMoveset(MoveId.SPLASH).enemySpecies(SpeciesId.MAGIKARP).disableTrainerWaves().mysteryEncounterChance(0);
 await g.classicMode.startBattle(SpeciesId.MEW,SpeciesId.MEW);BattleScene.prototype.randBattleSeedInt=native;
 const s:any=g.scene,b:any=s.currentBattle,pm:any=s.phaseManager;
 s.waveSeed="faint-history-native";const maxExpLevel=s.getMaxExpLevel();expect(maxExpLevel).toBeGreaterThan(0);
 const players:any[]=[...s.getPlayerParty()],enemies:any[]=[...s.getEnemyParty()];
 while(players.length<6)players.push(s.addPlayerPokemon(speciesDataRegistry.getSpecies(SpeciesId.MEW),30));
 while(enemies.length<6)enemies.push(s.addEnemyPokemon(speciesDataRegistry.getSpecies(SpeciesId.MAGIKARP),30,TrainerSlot.TRAINER));
 const actors=[...players,...enemies];expect(new Set(actors.map(p=>p.id)).size).toBe(12);
 const trainer:any=new Trainer(TrainerType.ACE_TRAINER,TrainerVariant.DEFAULT,0);
 trainer.config={...trainer.config,trainerAI:{...trainer.config.trainerAI,teraMode:TeraAIMode.INSTANT_TERA,instantTeras:[0,1,2,3,4,5]}};
 b.trainer=trainer;b.battleType=BattleType.TRAINER;
 enemies.forEach((p,k)=>p.initialTeamIndex=k);
 vi.spyOn(pm,"shiftPhase").mockImplementation(()=>{});
 vi.spyOn(audioManager,"playSound").mockImplementation(()=>{});
 vi.spyOn(s.tweens,"add").mockImplementation((config:any)=>{config.onComplete?.();return {} as any;});
 for(const p of actors){vi.spyOn(p,"updateInfo").mockImplementation(async()=>{});vi.spyOn(p,"hideInfo").mockImplementation(async()=>{});vi.spyOn(p,"faintCry").mockImplementation((callback:any)=>callback());}
 const attr:any=allMoves[MoveId.RETALIATE].getAttrs("MovePowerMultiplierAttr")[0];expect(attr).toBeDefined();
 Phaser.Math.RND.sow(["faint-history-global"]);const globalRng=Phaser.Math.RND.state();
 Phaser.Math.RND.sow(["faint-history-battle"]);const battleRng=Phaser.Math.RND.state();Phaser.Math.RND.state(globalRng);b.battleSeedState=battleRng;
 const rows:any[]=[];
 for(const side of [0,1])for(const pattern of [0,1,2,3])for(const length of [0,1,11,12,13,14,32,100,512]){
  pm.clearAllPhases();b.playerFaintsHistory=[];b.enemyFaintsHistory=[];b.enemyFaints=0;s.arena.playerFaints=0;
  for(const p of actors){p.hp=p.getMaxHp();p.status=null;p.switchOutStatus=false;p.resetSummonData();p.resetTurnData();s.field.remove(p);}
  s.getPlayerParty().splice(0,s.getPlayerParty().length,...players);b.enemyParty=enemies.slice();
  for(const p of [...players.slice(0,2),...enemies.slice(0,2)])s.field.add(p);
  const events:any[]=[];
  for(let i=0;i<length;i++){
   const index=pattern===0?0:pattern===1?i%6:pattern===2?(i<11?0:i===11?1:2):(i===0?5:i%5);
   const party=side===0?players:enemies,dead=party[index];
   const ordered=[dead,...party.filter(p=>p!==dead)];
   if(side===0)s.getPlayerParty().splice(0,6,...ordered);else b.enemyParty=ordered;
   dead.hp=0;dead.status=null;dead.switchOutStatus=false;dead.resetTurnData();s.field.add(dead);b.turn=i+1;
   pm.clearAllPhases();const before=[Phaser.Math.RND.state(),b.battleSeedState];
   const phase:any=new FaintPhase(side===0?0:2,true);pm.currentPhase=phase;phase.start();
   expect([Phaser.Math.RND.state(),b.battleSeedState]).toEqual(before);
   const history=side===0?b.playerFaintsHistory:b.enemyFaintsHistory;
   expect(history).toHaveLength(i+1);expect(history.at(-1).pokemon).toBe(dead);expect(history.at(-1).turn).toBe(i+1);
   events.push({index,turn:i+1});dead.hp=dead.getMaxHp();dead.status=null;s.field.add(dead);
  }
  const observations:any[]=[];
  for(const delta of [-1,0,1,2]){
   b.turn=length+delta;const rng=[Phaser.Math.RND.state(),b.battleSeedState];
   const power=[players[0],enemies[0]].map(user=>{const holder=new NumberHolder(1);attr.apply(user,user===players[0]?enemies[0]:players[0],allMoves[MoveId.RETALIATE],[holder]);return holder.value;});
   const tera=enemies.map(p=>trainer.shouldTera(p));
   expect([Phaser.Math.RND.state(),b.battleSeedState]).toEqual(rng);observations.push({delta,power,tera});
  }
  const logs=[b.playerFaintsHistory,b.enemyFaintsHistory].map(log=>log.map((entry:any)=>({index:actors.indexOf(entry.pokemon),turn:entry.turn})));
  rows.push({side,pattern,length,events,logs,playerFaints:s.arena.playerFaints,enemyFaints:b.enemyFaints,observations});
 }
 expect(rows).toHaveLength(72);fs.writeFileSync(process.env.FAINT_HISTORY_OUT!,JSON.stringify({version:1,waveSeed:s.waveSeed,maxExpLevel,globalRng,battleRng,rows})+"\n");
},300000);
