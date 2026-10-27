import { Weather } from "#data/weather";
import { Terrain } from "#data/terrain";
import { BattleScene } from "#app/battle-scene";
import { getGameMode } from "#app/game-mode";
import { speciesDataRegistry as registry } from "#app/globals/global-species-data-registry";
import { activeOverrides } from "#app/overrides";
import { modifierTypes } from "#data/data-lists";
import { generateModifierType } from "#mystery-encounters/encounter-phase-utils";
import { BattleType } from "#enums/battle-type";
import { GameModes } from "#enums/game-modes";
import { MoveId } from "#enums/move-id";
import { SpeciesId } from "#enums/species-id";
import { EnemyPokemon, Pokemon } from "#field/pokemon";
import { PokemonMove } from "#moves/pokemon-move";
import { GameManager } from "#test/framework/game-manager";
import Phaser from "phaser";
import fs from "node:fs";
import { it,expect,vi } from "vitest";
it("native unfused boss constructor generator boundary",async()=>{
 const native=BattleScene.prototype.randBattleSeedInt;
 const nativeBaseMovegen=Pokemon.prototype.generateAndPopulateMoveset;
 expect(vi.isMockFunction(nativeBaseMovegen)).toBe(false);
 const nativeMovegen=EnemyPokemon.prototype.generateAndPopulateMoveset;
 expect(vi.isMockFunction(nativeMovegen)).toBe(false);
 const game=new GameManager(new Phaser.Game({type:Phaser.HEADLESS}));
 game.override.battleStyle("single").startingWave(5).enemySpecies(SpeciesId.MAGIKARP).enemyMoveset(MoveId.SPLASH);
 await game.classicMode.startBattle(SpeciesId.MEW);BattleScene.prototype.randBattleSeedInt=native;
 expect(Pokemon.prototype.generateAndPopulateMoveset).toBe(nativeBaseMovegen);
 const scene:any=game.scene,battle=scene.currentBattle;
 for(const [key,value]of Object.entries({ENEMY_MOVESET_OVERRIDE:[],ENEMY_LEVEL_OVERRIDE:0,ENEMY_SPECIES_OVERRIDE:null,ENEMY_FUSION_OVERRIDE:false,ENEMY_FUSION_SPECIES_OVERRIDE:null,ENEMY_IVS_OVERRIDE:null,ENEMY_NATURE_OVERRIDE:null,ENEMY_STATUS_OVERRIDE:0,ENEMY_GENDER_OVERRIDE:null,ENEMY_FORM_OVERRIDES:{},ENEMY_SHINY_OVERRIDE:null,ENEMY_VARIANT_OVERRIDE:null}))vi.spyOn(activeOverrides as any,key,"get").mockReturnValue(value);
 // Actual native generation, observed at its constructor boundary.
 const original=nativeMovegen;
 let generation:any=null;
 const input=(p:any)=>({id:p.id,species:p.species.speciesId,form:p.formIndex,level:p.level,gender:p.gender,nature:p.nature,abilityIndex:p.abilityIndex,ability:p.getAbility().id,fusion:p.fusionSpecies?.speciesId??0,fusionForm:p.fusionFormIndex??0,fusionGender:p.fusionGender??-1,fusionAbilityIndex:p.fusionAbilityIndex??0,tera:p.teraType,ivs:[...p.ivs],stats:[...p.stats],hp:p.hp,boss:p.isBoss(),bossSegments:p.bossSegments,passive:p.hasPassive(),passiveAbility:p.getPassiveAbility().id,trainer:p.hasTrainer(),types:p.getTypes({includeTeraType:false,bypassSummonData:true,ignoreThirdType:true})});
 EnemyPokemon.prototype.generateAndPopulateMoveset=function(...args:any[]){
  const before=Phaser.Math.RND.state(),frame=input(this);
  const pool=this.getLevelMoves({startingLevel:1,includeEvolutionMoves:true,includePrevolutionMoves:true});
  expect(Phaser.Math.RND.state()).toBe(before);
  const result=original.apply(this,args);
  generation={before,after:Phaser.Math.RND.state(),input:frame,pool,moves:this.moveset.map((m:any)=>({move:m.moveId,used:m.ppUsed,up:m.ppUp,max:m.maxPpOverride??-1}))};
  return result;
 };
 scene.modifiers=[];scene.enemyModifiers=[];battle.battleType=BattleType.WILD;battle.trainer=null;
 const rows:any[]=[];
 try{
  const emit=(species:number,level:number,double:boolean,weather:number,terrain:number)=>{
   const mode=GameModes.CLASSIC,sample=rows.length;
   scene.modifiers=[];scene.arena.weather=weather?new Weather(weather):null;scene.arena.terrain=terrain?new Terrain(terrain):null;battle.double=double;
   const gm=getGameMode(mode);scene.gameMode=gm;battle.gameMode=gm;battle.waveIndex=1;scene.arena.biomeId=0;scene.arena.lastTimeOfDay=undefined;scene.waveCycleOffset=5;scene.seed="wild-boss-movesets";scene.waveSeed="wild-boss-wave";
   Phaser.Math.RND.sow([`wild-boss-movesets-${sample}`]);const before=Phaser.Math.RND.state();
   const p:any=scene.addEnemyPokemon(registry.getSpecies(species),level,0,true,true);
   expect(generation.input.boss).toBe(true);expect(generation.input.passive).toBe(true);expect(generation.input.fusion).toBe(0);
   rows.push({mode,species,level,sample,double,weather,terrain,before,generation,after:Phaser.Math.RND.state()});p.destroy();
  };
  for(const species of [1,10,13,19,25,129,132,133,151,212,230,235,382,383,479,555,658,798])for(const level of [1,50,70,100,65536,70000])for(const double of [false,true])emit(species,level,double,0,0);
  for(const species of [230,382,383,555])for(const level of [100,70000])for(let weather=0;weather<10;weather++)for(let terrain=0;terrain<5;terrain++)for(const double of [false,true])emit(species,level,double,weather,terrain);
 }finally{EnemyPokemon.prototype.generateAndPopulateMoveset=original;}
 expect(rows).toHaveLength(1016);
 fs.writeFileSync(process.env.BOSS_MOVESET_OUT!,JSON.stringify({version:1,scope:"actual native unfused boss constructor generator boundary; finish RNG recorded separately",rows})+"\n");
},240000);
