/* Actual Trainer.genPartyMember with its real moveset and constructor finish.
 * Retains 696 single-member observations, then observes complete ordinary parties. */
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
import { it,expect,vi } from "vitest";
it('actual trainer member moveset and constructor finish',async()=>{
 const native=BattleScene.prototype.randBattleSeedInt;expect(vi.isMockFunction(native)).toBe(false);
 const nativeMoves=EnemyPokemon.prototype.generateAndPopulateMoveset,nativeBaseMoves=Pokemon.prototype.generateAndPopulateMoveset,nativePower=Move.prototype.calculateEffectivePower;
 for(const method of [nativeMoves,nativeBaseMoves,nativePower])expect(vi.isMockFunction(method)).toBe(false);
 const game=new GameManager(new Phaser.Game({type:Phaser.HEADLESS}));game.override.startingWave(3).enemySpecies(SpeciesId.MAGIKARP);
 await game.classicMode.startBattle(SpeciesId.BULBASAUR);BattleScene.prototype.randBattleSeedInt=native;
 expect(EnemyPokemon.prototype.generateAndPopulateMoveset).toBe(nativeMoves);expect(Pokemon.prototype.generateAndPopulateMoveset).toBe(nativeBaseMoves);expect(Move.prototype.calculateEffectivePower).toBe(nativePower);
 for(const[key,value]of Object.entries({ENEMY_MOVESET_OVERRIDE:[],ENEMY_LEVEL_OVERRIDE:0,ENEMY_SPECIES_OVERRIDE:null,ENEMY_FUSION_OVERRIDE:false,ENEMY_FUSION_SPECIES_OVERRIDE:null,ENEMY_IVS_OVERRIDE:null,ENEMY_NATURE_OVERRIDE:null,ENEMY_STATUS_OVERRIDE:0,ENEMY_GENDER_OVERRIDE:null,ENEMY_FORM_OVERRIDES:{},ENEMY_SHINY_OVERRIDE:null,ENEMY_VARIANT_OVERRIDE:null}))vi.spyOn(activeOverrides as any,key,'get').mockReturnValue(value);
 const s:any=game.scene,rows:any[]=[];s.gameData.trainerId=12345;s.gameData.secretId=54321;let atMoves:any,atConstructor:any;
 const state=()=>Phaser.Math.RND.state().replace(/^!rnd,/,'').split(',').map(Number);
 const snapshot=(p:any)=>({slot:p.trainerSlot,onField:p.isOnField(),trainer:p.hasTrainer(),boss:p.isBoss(),passive:p.hasPassive(),willTera:willTerastallize(p),species:p.species.speciesId,form:p.formIndex,level:p.level,id:p.id,abilityIndex:p.abilityIndex,ability:p.getAbility().id,gender:p.gender,nature:p.nature,ivs:p.ivs.slice(),stats:p.stats.slice(),fusion:p.fusionSpecies?.speciesId??-1,tera:p.teraType,hp:p.hp,friendship:p.friendship,shiny:p.shiny,variant:p.variant,luck:p.luck,moves:p.getMoveset(true)?.map((m:any)=>({move:m.moveId,ppUsed:m.ppUsed,ppUp:m.ppUp,maxPp:m.maxPpOverride}))??null});
 const original=nativeMoves;
 const add=s.addEnemyPokemon;
 s.addEnemyPokemon=function(...args:any[]){const before=state();const p=add.apply(this,args);atConstructor={before,after:state(),output:snapshot(p)};return p;};
 vi.spyOn(EnemyPokemon.prototype,'generateAndPopulateMoveset').mockImplementation(function(this:any,...args:any[]){
  const before=state(),input=snapshot(this);original.apply(this,args as any);
  atMoves={before,input,after:state(),output:snapshot(this)};
 });
 const cases:any[]=[];
 for(const type of [TrainerType.YOUNGSTER,TrainerType.BREEDER])for(const wave of [11,99])for(const level of [5,24,25,69,70,100])for(let sample=0;sample<8;sample++)cases.push({type,wave,level});
 const generic=Object.values(trainerConfigs).filter((c:any)=>c.trainerType>0&&c.trainerType<=TrainerType.YOUNGSTER).map((c:any)=>c.trainerType);
 for(const type of generic)for(const wave of [11,99])for(const level of [5,24])for(let sample=0;sample<2;sample++)cases.push({type,wave,level});
 for(const {type,wave,level}of cases){
  const index=rows.length;s.currentBattle.waveIndex=wave;s.currentBattle.battleType=BattleType.TRAINER;s.currentBattle.enemyParty=[];s.currentBattle.enemyLevels=[level];s.enemyModifiers=[];s.modifiers=[];
  s.setSeed(`trainer-full-${index}`);s.waveSeed=`trainer-full-wave-${index}`;
  const double=!!trainerConfigs[type].doubleOnly;s.currentBattle.double=double;
  const trainer:any=new Trainer(type,double?TrainerVariant.DOUBLE:TrainerVariant.DEFAULT,0);s.currentBattle.trainer=trainer;
  Phaser.Math.RND.sow([`trainer-full-ambient-${index}`]);const before=state();atMoves=null;atConstructor=null;
  const p:any=trainer.genPartyMember(0);expect(atMoves).not.toBeNull();expect(state()).toEqual(before);
  expect(atConstructor).not.toBeNull();const t=trainer.getPartyTemplate();
  const template={size:t.size,strength:t.strength,kind:t.evoLevelThresholdKind,subSizes:(t.templates??[]).map((v:any)=>v.size),members:Array.from({length:t.size},(_:any,k:number)=>({strength:t.getStrength(k),kind:t.getEvoThresholdKind(k),same:t.isSameSpecies(k),balanced:t.isBalanced(k)}))};
  const environment={trainerId:s.gameData.trainerId,secretId:s.gameData.secretId,biome:s.arena.biomeId,cycle:s.waveCycleOffset,weather:s.arena.weather?.weatherType??0,terrain:s.arena.terrain?.terrainType??0,allowEggMoves:trainer.config.allowEggMoves,eventActive:timedEventManager.isEventActive(),eventChance:timedEventManager.getClassicTrainerShinyChance(),eventMultiplier:timedEventManager.getShinyEncounterMultiplier()};
  rows.push({index,type,wave,level,double,atMoves,atConstructor,template,environment,finished:snapshot(p),ambient:before});
  s.fieldUI.remove(p.battleInfo);s.field.remove(p);p.destroy();trainer.destroy();s.currentBattle.trainer=null;
 }
 expect(rows.length).toBe(cases.length);
 const parties:any[]=[];
 for(const type of generic)for(const wave of [11,99])for(const level of [5,24])for(let sample=0;sample<2;sample++){
  const index=parties.length;s.currentBattle.waveIndex=wave;s.currentBattle.battleType=BattleType.TRAINER;s.currentBattle.enemyParty=[];s.enemyModifiers=[];s.modifiers=[];
  s.setSeed(`trainer-party-${index}`);s.waveSeed=`trainer-party-wave-${index}`;
  const double=!!trainerConfigs[type].doubleOnly;s.currentBattle.double=double;
  const trainer:any=new Trainer(type,double?TrainerVariant.DOUBLE:TrainerVariant.DEFAULT,0);s.currentBattle.trainer=trainer;
  const t=trainer.getPartyTemplate();s.currentBattle.enemyLevels=Array(t.size).fill(level);
  const template={size:t.size,strength:t.strength,kind:t.evoLevelThresholdKind,subSizes:(t.templates??[]).map((v:any)=>v.size),members:Array.from({length:t.size},(_:any,k:number)=>({strength:t.getStrength(k),kind:t.getEvoThresholdKind(k),same:t.isSameSpecies(k),balanced:t.isBalanced(k)}))};
  const environment={trainerId:s.gameData.trainerId,secretId:s.gameData.secretId,biome:s.arena.biomeId,cycle:s.waveCycleOffset,weather:s.arena.weather?.weatherType??0,terrain:s.arena.terrain?.terrainType??0,allowEggMoves:trainer.config.allowEggMoves,eventActive:timedEventManager.isEventActive(),eventChance:timedEventManager.getClassicTrainerShinyChance(),eventMultiplier:timedEventManager.getShinyEncounterMultiplier()};
  Phaser.Math.RND.sow([`trainer-party-ambient-${index}`]);const ambient=state(),members:any[]=[];
  for(let member=0;member<t.size;member++){
   atMoves=null;atConstructor=null;const p:any=trainer.genPartyMember(member);
   expect(atMoves).not.toBeNull();expect(atConstructor).not.toBeNull();expect(state()).toEqual(ambient);
   members.push({member,atMoves,atConstructor,finished:snapshot(p)});s.currentBattle.enemyParty.push(p);
  }
  s.updateModifiers(false);expect(state()).toEqual(ambient);
  const refreshed=s.currentBattle.enemyParty.map(snapshot);
  parties.push({index,type,wave,level,double,template,environment,ambient,members,refreshed});
  for(const p of s.currentBattle.enemyParty){s.fieldUI.remove(p.battleInfo);s.field.remove(p);p.destroy();}
  s.currentBattle.enemyParty=[];trainer.destroy();s.currentBattle.trainer=null;
 }
 expect(parties.length).toBe(504);
 fs.writeFileSync(process.env.TRAINER_FULL_OUT??'/tmp/trainer-full.json',JSON.stringify({version:1,upstream:'8555c08c823b856cbec4eb99ca84ea52a955836d',rows,parties})+'\n');
 fs.writeFileSync(process.env.TRAINER_FACTS_OUT??'/tmp/trainer-move-facts.json',JSON.stringify(allMoves.map((m:any)=>(m.getAttrs('StatStageChangeAttr').some((a:any)=>a.stages>1&&a.selfTarget)?1:0)|(m.hasAttr('OneHitKOAttr')?2:0)))+'\n');
},120000);
