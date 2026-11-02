/* Actual Trainer.genPartyMember with its real moveset and constructor finish.
 * Default plain Gym constructors; every generation and postProcess flag retained. */
import { speciesDataRegistry } from "#app/global-species-data-registry";
import { BattleScene } from "#app/battle-scene";
import { timedEventManager } from "#app/global-event-manager";
import { activeOverrides } from "#app/overrides";
import { allMoves } from "#data/data-lists";
import { trainerConfigs } from "#trainers/trainer-config";
import { BattleType } from "#enums/battle-type";
import { SpeciesId } from "#enums/species-id";
import { TrainerType } from "#enums/trainer-type";
import { TrainerVariant } from "#enums/trainer-variant";
import { EnemyPokemon,Pokemon } from "#field/pokemon";
import { Move } from "#moves/move";
import { Trainer } from "#field/trainer";
import { willTerastallize } from "#utils/pokemon-utils";
import { GameManager } from "#test/framework/game-manager";
import Phaser from "phaser";
import fs from "node:fs";
import { gzipSync } from "node:zlib";
import { it,expect,vi } from "vitest";
it('actual trainer member moveset and constructor finish',async()=>{
 const native=BattleScene.prototype.randBattleSeedInt;expect(vi.isMockFunction(native)).toBe(false);
 const nativeMoves=EnemyPokemon.prototype.generateAndPopulateMoveset,nativeBaseMoves=Pokemon.prototype.generateAndPopulateMoveset,nativePower=Move.prototype.calculateEffectivePower;
 for(const method of [nativeMoves,nativeBaseMoves,nativePower])expect(vi.isMockFunction(method)).toBe(false);
 const game=new GameManager(new Phaser.Game({type:Phaser.HEADLESS}));game.override.startingWave(3).enemySpecies(SpeciesId.MAGIKARP);
 await game.classicMode.startBattle(SpeciesId.BULBASAUR);BattleScene.prototype.randBattleSeedInt=native;
 expect(EnemyPokemon.prototype.generateAndPopulateMoveset).toBe(nativeMoves);expect(Pokemon.prototype.generateAndPopulateMoveset).toBe(nativeBaseMoves);expect(Move.prototype.calculateEffectivePower).toBe(nativePower);
 for(const[key,value]of Object.entries({ENEMY_MOVESET_OVERRIDE:[],ENEMY_LEVEL_OVERRIDE:0,ENEMY_SPECIES_OVERRIDE:null,ENEMY_FUSION_OVERRIDE:false,ENEMY_FUSION_SPECIES_OVERRIDE:null,ENEMY_IVS_OVERRIDE:null,ENEMY_NATURE_OVERRIDE:null,ENEMY_STATUS_OVERRIDE:0,ENEMY_GENDER_OVERRIDE:null,ENEMY_FORM_OVERRIDES:{},ENEMY_SHINY_OVERRIDE:null,ENEMY_VARIANT_OVERRIDE:null}))vi.spyOn(activeOverrides as any,key,'get').mockReturnValue(value);
 const s:any=game.scene,rows:any[]=[];s.gameData.trainerId=12345;s.gameData.secretId=54321;let atMoves:any,atConstructor:any;const moveCalls:any[]=[];
 const state=()=>Phaser.Math.RND.state().replace(/^!rnd,/,'').split(',').map(Number);
 const snapshot=(p:any)=>({slot:p.trainerSlot,initialTeamIndex:p.initialTeamIndex,bossSegments:p.bossSegments,bossSegmentIndex:p.bossSegmentIndex,storedPassive:p.passive,passiveAbility:p.getPassiveAbility().id,onField:p.isOnField(),trainer:p.hasTrainer(),boss:p.isBoss(),passive:p.hasPassive(),willTera:willTerastallize(p),species:p.species.speciesId,form:p.formIndex,level:p.level,id:p.id,abilityIndex:p.abilityIndex,ability:p.getAbility().id,gender:p.gender,nature:p.nature,ivs:p.ivs.slice(),stats:p.stats.slice(),fusion:p.fusionSpecies?.speciesId??-1,tera:p.teraType,hp:p.hp,friendship:p.friendship,shiny:p.shiny,variant:p.variant,luck:p.luck,moves:p.getMoveset(true)?.map((m:any)=>({move:m.moveId,ppUsed:m.ppUsed,ppUp:m.ppUp,maxPp:m.maxPpOverride}))??null});
 const original=nativeMoves;
 const add=s.addEnemyPokemon;
 s.addEnemyPokemon=function(...args:any[]){
  const before=state(),first=moveCalls.length;
  const input={species:args[0].speciesId,level:args[1],slot:args[2],boss:args[3]??false,shinyLock:args[4]??false,dataSource:args[5]!=null,postProcess:typeof args[6]==='function',forRival:args[7]??false};
  const p=add.apply(this,args);atConstructor={before,input,after:state(),generations:moveCalls.slice(first),output:snapshot(p)};return p;
 };
 vi.spyOn(EnemyPokemon.prototype,'generateAndPopulateMoveset').mockImplementation(function(this:any,...args:any[]){
  const before=state(),input=snapshot(this);original.apply(this,args as any);
  atMoves={before,input,after:state(),output:snapshot(this)};moveCalls.push(atMoves);
 });
 const cases:any[]=[];
 const types=[TrainerType.BROCK,TrainerType.ROXANNE,TrainerType.TATE,TrainerType.LIZA];
 for(const type of types)for(const wave of [31,91,131])for(const level of [79,80,120,121,169,170,200,201])for(let sample=0;sample<2;sample++)cases.push({type,wave,level,double:false});
 for(const type of [TrainerType.TATE,TrainerType.LIZA])for(const wave of [31,91,131])for(const level of [79,80,120,121,169,170,200,201])for(let sample=0;sample<2;sample++)cases.push({type,wave,level,double:true});
 for(const {type,wave,level,double:requestedDouble}of cases){
  const index=rows.length;s.currentBattle.waveIndex=wave;s.currentBattle.battleType=BattleType.TRAINER;s.currentBattle.enemyParty=[];s.currentBattle.enemyLevels=[level];s.enemyModifiers=[];s.modifiers=[];
  s.setSeed(`gym-egg-full-${index}`);s.waveSeed=`gym-egg-full-wave-${index}`;
  const double=requestedDouble;s.currentBattle.double=double;
  const trainer:any=new Trainer(type,double?TrainerVariant.DOUBLE:TrainerVariant.DEFAULT,0);s.currentBattle.trainer=trainer;
  Phaser.Math.RND.sow([`gym-egg-full-ambient-${index}`]);const before=state();atMoves=null;atConstructor=null;
  const p:any=trainer.genPartyMember(0);expect(atMoves).not.toBeNull();expect(state()).toEqual(before);
  expect(atConstructor).not.toBeNull();const t=trainer.getPartyTemplate();
  const template={size:t.size,strength:t.strength,kind:t.evoLevelThresholdKind,subSizes:(t.templates??[]).map((v:any)=>v.size),members:Array.from({length:t.size},(_:any,k:number)=>({strength:t.getStrength(k),kind:t.getEvoThresholdKind(k),same:t.isSameSpecies(k),balanced:t.isBalanced(k)}))};
  const environment={trainerId:s.gameData.trainerId,secretId:s.gameData.secretId,biome:s.arena.biomeId,cycle:s.waveCycleOffset,weather:s.arena.weather?.weatherType??0,terrain:s.arena.terrain?.terrainType??0,allowEggMoves:trainer.config.allowEggMoves,configBoss:trainer.config.isBoss,staticParty:trainer.config.hasStaticParty,instantTeras:trainer.config.trainerAI.instantTeras.slice(),eventActive:timedEventManager.isEventActive(),eventChance:timedEventManager.getClassicTrainerShinyChance(),eventMultiplier:timedEventManager.getShinyEncounterMultiplier()};
  expect(atConstructor.input.postProcess).toBe(false);expect(atConstructor.generations.length).toBe(1);expect(atMoves.input.willTera).toBe(false);
  rows.push({index,type,wave,level,double,atMoves,atConstructor,template,environment,finished:snapshot(p),ambient:before});
  s.fieldUI.remove(p.battleInfo);s.field.remove(p);p.destroy();trainer.destroy();s.currentBattle.trainer=null;
 }
 expect(rows.length).toBe(cases.length);
 const parties:any[]=[];
 const partyCases=cases;
 for(const {type,wave,level,double:requestedDouble}of partyCases){
  const index=parties.length;s.currentBattle.waveIndex=wave;s.currentBattle.battleType=BattleType.TRAINER;s.currentBattle.enemyParty=[];s.enemyModifiers=[];s.modifiers=[];
  s.setSeed(`gym-egg-party-${index}`);s.waveSeed=`gym-egg-party-wave-${index}`;
  const double=requestedDouble;s.currentBattle.double=double;
  const trainer:any=new Trainer(type,double?TrainerVariant.DOUBLE:TrainerVariant.DEFAULT,0);s.currentBattle.trainer=trainer;
  const t=trainer.getPartyTemplate();s.currentBattle.enemyLevels=Array(t.size).fill(level);
  const template={size:t.size,strength:t.strength,kind:t.evoLevelThresholdKind,subSizes:(t.templates??[]).map((v:any)=>v.size),members:Array.from({length:t.size},(_:any,k:number)=>({strength:t.getStrength(k),kind:t.getEvoThresholdKind(k),same:t.isSameSpecies(k),balanced:t.isBalanced(k)}))};
  const environment={trainerId:s.gameData.trainerId,secretId:s.gameData.secretId,biome:s.arena.biomeId,cycle:s.waveCycleOffset,weather:s.arena.weather?.weatherType??0,terrain:s.arena.terrain?.terrainType??0,allowEggMoves:trainer.config.allowEggMoves,configBoss:trainer.config.isBoss,staticParty:trainer.config.hasStaticParty,instantTeras:trainer.config.trainerAI.instantTeras.slice(),eventActive:timedEventManager.isEventActive(),eventChance:timedEventManager.getClassicTrainerShinyChance(),eventMultiplier:timedEventManager.getShinyEncounterMultiplier()};
  Phaser.Math.RND.sow([`gym-egg-party-ambient-${index}`]);const ambient=state(),members:any[]=[];
  for(let member=0;member<t.size;member++){
   atMoves=null;atConstructor=null;const p:any=trainer.genPartyMember(member);
   expect(atMoves).not.toBeNull();expect(atConstructor).not.toBeNull();expect(state()).toEqual(ambient);
   expect(atConstructor.input.postProcess).toBe(false);expect(atConstructor.generations.length).toBe(1);expect(atMoves.input.willTera).toBe(false);
   members.push({member,atMoves,atConstructor,finished:snapshot(p)});s.currentBattle.enemyParty.push(p);
  }
  s.updateModifiers(false);expect(state()).toEqual(ambient);
  const refreshed=s.currentBattle.enemyParty.map(snapshot);
  parties.push({index,type,wave,level,double,template,environment,ambient,members,refreshed});
  for(const p of s.currentBattle.enemyParty){s.fieldUI.remove(p.battleInfo);s.field.remove(p);p.destroy();}
  s.currentBattle.enemyParty=[];trainer.destroy();s.currentBattle.trainer=null;
 }
 expect(parties.length).toBe(partyCases.length);
 // Direct generator API controls. Source catalogue discovery found level/egg
 // overlap in these fixed Revavroom forms. This validates pool exclusion only;
 // it does not implement form changes or the team's boss constructor callbacks.
 const api:any[]=[];
 for(const form of [2,3,5])for(const level of [80,170,201])for(let sample=0;sample<2;sample++){
  const index=api.length,type=TrainerType.BROCK,wave=91;
  s.currentBattle.waveIndex=wave;s.currentBattle.battleType=BattleType.TRAINER;s.currentBattle.double=false;s.currentBattle.enemyParty=[];s.enemyModifiers=[];s.modifiers=[];
  s.setSeed(`gym-egg-api-${index}`);s.waveSeed=`gym-egg-api-wave-${index}`;
  const trainer:any=new Trainer(type,TrainerVariant.DEFAULT,0);s.currentBattle.trainer=trainer;
  const p:any=s.addEnemyPokemon(speciesDataRegistry.getSpecies(SpeciesId.REVAVROOM),level,1);
  p.formIndex=form;p.abilityIndex=0;p.calculateStats();p.moveset=[];
  Phaser.Math.RND.sow([`gym-egg-api-rng-${index}`]);atMoves=null;
  p.generateAndPopulateMoveset();expect(atMoves).not.toBeNull();expect(atMoves.input.form).toBe(form);
  const environment={trainerId:s.gameData.trainerId,secretId:s.gameData.secretId,biome:s.arena.biomeId,cycle:s.waveCycleOffset,weather:s.arena.weather?.weatherType??0,terrain:s.arena.terrain?.terrainType??0,allowEggMoves:trainer.config.allowEggMoves,configBoss:trainer.config.isBoss,staticParty:trainer.config.hasStaticParty,instantTeras:trainer.config.trainerAI.instantTeras.slice(),eventActive:timedEventManager.isEventActive(),eventChance:timedEventManager.getClassicTrainerShinyChance(),eventMultiplier:timedEventManager.getShinyEncounterMultiplier()};
  api.push({index,type,wave,level,form,environment,atMoves});
  s.fieldUI.remove(p.battleInfo);s.field.remove(p);p.destroy();trainer.destroy();s.currentBattle.trainer=null;
 }
 expect(api.length).toBe(18);

 fs.writeFileSync(process.env.TRAINER_FULL_OUT??'/tmp/gym-egg-native.json.gz',gzipSync(JSON.stringify({version:2,scope:"default plain Gym trainer constructors with egg moves",upstream:'8555c08c823b856cbec4eb99ca84ea52a955836d',rows,parties,api})+'\n'));
 fs.writeFileSync(process.env.TRAINER_FACTS_OUT??'/tmp/trainer-move-facts.json',JSON.stringify(allMoves.map((m:any)=>(m.getAttrs('StatStageChangeAttr').some((a:any)=>a.stages>1&&a.selfTarget)?1:0)|(m.hasAttr('OneHitKOAttr')?2:0)))+'\n');
},120000);
