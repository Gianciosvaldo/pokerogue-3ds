import { regenerateModifierPoolThresholds } from "#modifiers/modifier-type";
import { ModifierPoolType } from "#enums/modifier-pool-type";
import { Weather } from "#data/weather";
import { Terrain } from "#data/terrain";
import { BattleScene } from "#app/battle-scene";
import { getGameMode } from "#app/game-mode";
import { speciesDataRegistry as registry } from "#app/globals/global-species-data-registry";
import { activeOverrides } from "#app/overrides";
import { Move } from "#moves/move";
import { modifierTypes, allMoves } from "#data/data-lists";
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
it("native boss constructor finish chain",async()=>{
 expect(Number(process.versions.node.split(".")[0])).toBe(22);
 const native=BattleScene.prototype.randBattleSeedInt;
 expect(vi.isMockFunction(native)).toBe(false);
 const nativePower=Move.prototype.calculateEffectivePower;
 expect(vi.isMockFunction(nativePower)).toBe(false);
 const nativeBaseMovegen=Pokemon.prototype.generateAndPopulateMoveset;
 expect(vi.isMockFunction(nativeBaseMovegen)).toBe(false);
 const nativeMovegen=EnemyPokemon.prototype.generateAndPopulateMoveset;
 expect(vi.isMockFunction(nativeMovegen)).toBe(false);
 const game=new GameManager(new Phaser.Game({type:Phaser.HEADLESS}));
 game.override.battleStyle("single").startingWave(5).enemySpecies(SpeciesId.MAGIKARP).enemyMoveset(MoveId.SPLASH);
 await game.classicMode.startBattle(SpeciesId.MEW);BattleScene.prototype.randBattleSeedInt=native;
 expect(Pokemon.prototype.generateAndPopulateMoveset).toBe(nativeBaseMovegen);
 expect(Move.prototype.calculateEffectivePower).toBe(nativePower);
 const scene:any=game.scene,battle=scene.currentBattle;expect(vi.isMockFunction(scene.updateModifiers)).toBe(false);expect(vi.isMockFunction(scene.updatePartyForModifiers)).toBe(false);
 for(const [key,value]of Object.entries({ENEMY_MOVESET_OVERRIDE:[],ENEMY_LEVEL_OVERRIDE:0,ENEMY_SPECIES_OVERRIDE:null,ENEMY_FUSION_OVERRIDE:false,ENEMY_FUSION_SPECIES_OVERRIDE:null,ENEMY_IVS_OVERRIDE:null,ENEMY_NATURE_OVERRIDE:null,ENEMY_STATUS_OVERRIDE:0,ENEMY_GENDER_OVERRIDE:null,ENEMY_FORM_OVERRIDES:{},ENEMY_SHINY_OVERRIDE:null,ENEMY_VARIANT_OVERRIDE:null}))vi.spyOn(activeOverrides as any,key,"get").mockReturnValue(value);
 // Actual native generation, observed at its constructor boundary.
 const original=nativeMovegen;
 let generation:any=null;
 const input=(p:any)=>({id:p.id,species:p.species.speciesId,form:p.formIndex,level:p.level,gender:p.gender,nature:p.nature,abilityIndex:p.abilityIndex,ability:p.getAbility().id,fusion:p.fusionSpecies?.speciesId??0,fusionForm:p.fusionFormIndex??0,fusionGender:p.fusionGender??-1,fusionAbilityIndex:p.fusionAbilityIndex??0,tera:p.teraType,ivs:[...p.ivs],stats:[...p.stats],hp:p.hp,boss:p.isBoss(),bossSegments:p.bossSegments,passive:p.hasPassive(),passiveAbility:p.getPassiveAbility().id,trainer:p.hasTrainer(),activeTypeChanger:p.getAbility().hasAttr("MoveTypeChangeAbAttr"),passiveTypeChanger:p.getPassiveAbility().hasAttr("MoveTypeChangeAbAttr"),types:p.getTypes({includeTeraType:false,bypassSummonData:true,ignoreThirdType:true})});
 EnemyPokemon.prototype.generateAndPopulateMoveset=function(...args:any[]){
  const before=Phaser.Math.RND.state(),frame=input(this);
  const pool=this.getLevelMoves({startingLevel:1,includeEvolutionMoves:true,includePrevolutionMoves:true});
  expect(Phaser.Math.RND.state()).toBe(before);
  const implemented=pool.map((r:any)=>!allMoves[r[1]].isUnimplemented);
  const previousFlag=scene.movesetGenInProgress;scene.movesetGenInProgress=true;
  let powers:any[]=[];
  try{powers=[...new Set(pool.filter((_r:any,i:number)=>implemented[i]).map((r:any)=>r[1]))].map((id:any)=>{expect(vi.isMockFunction(allMoves[id].calculateEffectivePower)).toBe(false);return [id,allMoves[id].calculateEffectivePower(this)];});}
  finally{scene.movesetGenInProgress=previousFlag;}
  expect(Phaser.Math.RND.state()).toBe(before);
  const result=original.apply(this,args);
  generation={before,after:Phaser.Math.RND.state(),input:frame,pool,implemented,powers,moves:this.moveset.map((m:any)=>({move:m.moveId,used:m.ppUsed,up:m.ppUp,max:m.maxPpOverride??-1}))};
  return result;
 };
 scene.modifiers=[];scene.enemyModifiers=[];battle.battleType=BattleType.WILD;battle.trainer=null;
 const rows:any[]=[];
 try{
  const emit=(species:number,level:number,double:boolean,sample:number,charms=0,mode=GameModes.SPLICED_ENDLESS)=>{
   battle.enemyParty=[];scene.modifiers=[];if(charms){const m:any=generateModifierType(modifierTypes.ABILITY_CHARM).newModifier();m.stackCount=charms;scene.modifiers.push(m);}
   scene.arena.weather=null;scene.arena.terrain=null;battle.double=double;
   const gm=getGameMode(mode);scene.gameMode=gm;battle.gameMode=gm;battle.waveIndex=1;scene.arena.biomeId=0;scene.arena.lastTimeOfDay=undefined;scene.waveCycleOffset=5;scene.seed="fused-boss-movesets";scene.waveSeed="fused-boss-wave";
   Phaser.Math.RND.sow([`fused-boss-movesets-${rows.length}`]);const before=Phaser.Math.RND.state();
   const p:any=scene.addEnemyPokemon(registry.getSpecies(species),level,0,true,true);
   expect(generation.input.boss).toBe(true);expect(generation.input.passive).toBe(true);if(mode===GameModes.SPLICED_ENDLESS)expect(generation.input.fusion).toBeGreaterThan(0);else expect(generation.input.fusion).toBe(0);
   const after=Phaser.Math.RND.state(),final={...input(p),friendship:p.friendship,exp:p.exp,name:p.name,shiny:p.shiny,variant:p.variant,luck:p.luck};
   battle.enemyParty=[p];vi.spyOn(p,"updateInfo").mockResolvedValue(undefined);scene.updateModifiers(false,true);expect(Phaser.Math.RND.state()).toBe(after);const refreshed=input(p);regenerateModifierPoolThresholds(scene.getEnemyField(),ModifierPoolType.WILD);const poolAfter=Phaser.Math.RND.state();
   rows.push({mode,species,level,sample,charms,double,weather:0,terrain:0,before,generation,after,final,refreshed,poolAfter});battle.enemyParty=[];p.destroy();
  };
  const species=[1,81,133,151,163,164,175,176,209,210,468,582,583,702,974,975,1016];
  for(const mode of [GameModes.SPLICED_ENDLESS,GameModes.ENDLESS]){
  for(const id of species)for(const level of [100,65536,70000])for(const double of [false,true])for(const sample of [0,1])emit(id,level,double,sample,0,mode);
  for(let sample=0;sample<600;sample++)emit(species[sample%species.length],sample%2?100:70000,!!(sample%3),sample,4,mode);
  }
  for(const mode of [GameModes.SPLICED_ENDLESS,GameModes.ENDLESS])for(const id of species)for(const double of [false,true])for(const sample of [0,1])emit(id,65535,double,sample,0,mode);
 }finally{EnemyPokemon.prototype.generateAndPopulateMoveset=original;}
 expect(rows).toHaveLength(1744);
 fs.writeFileSync(process.env.CONSTRUCTOR_FINISH_OUT!,JSON.stringify({version:1,scope:"native boss base, moveset, finish, stat-update and threshold APIs in both Endless modes",rows})+"\n");
},240000);
