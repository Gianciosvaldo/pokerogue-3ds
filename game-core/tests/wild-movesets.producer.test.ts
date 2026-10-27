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
it("native fused wild moveset constructor boundary",async()=>{
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
 const input=(p:any)=>({id:p.id,species:p.species.speciesId,form:p.formIndex,level:p.level,gender:p.gender,nature:p.nature,abilityIndex:p.abilityIndex,ability:p.getAbility().id,fusion:p.fusionSpecies?.speciesId??0,fusionForm:p.fusionFormIndex??0,fusionGender:p.fusionGender??-1,fusionAbilityIndex:p.fusionAbilityIndex??0,tera:p.teraType,ivs:[...p.ivs],stats:[...p.stats],hp:p.hp,boss:p.isBoss(),passive:p.hasPassive(),trainer:p.hasTrainer(),types:p.getTypes({includeTeraType:false,bypassSummonData:true,ignoreThirdType:true})});
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
  const emit=(mode:number,species:SpeciesId,level:number,sample:number,charms=0)=>{
   scene.modifiers=[];if(charms){const m:any=generateModifierType(modifierTypes.ABILITY_CHARM).newModifier();m.stackCount=charms;scene.modifiers.push(m);}
   const gm=getGameMode(mode);scene.gameMode=gm;battle.gameMode=gm;battle.waveIndex=1;scene.arena.biomeId=0;scene.arena.lastTimeOfDay=undefined;scene.waveCycleOffset=5;scene.seed="enemy-fusion-projection";scene.waveSeed="enemy-fusion-wave";
   Phaser.Math.RND.sow([`enemy-fusion-projection-${rows.length}`]);const before=Phaser.Math.RND.state();
   const p:any=scene.addEnemyPokemon(registry.getSpecies(species),level,0,false,true);
   rows.push({mode,species,level,sample,charms,before,generation,after:Phaser.Math.RND.state(),mon:{id:p.id,species:p.species.speciesId,form:p.formIndex,gender:p.gender,nature:p.nature,abilityIndex:p.abilityIndex,ability:p.getAbility().id,fusion:p.fusionSpecies?.speciesId??0,fusionForm:p.fusionFormIndex??0,fusionGender:p.fusionGender??-1,fusionAbilityIndex:p.fusionAbilityIndex??0,tera:p.teraType,ivs:[...p.ivs],stats:[...p.stats],hp:p.hp,friendship:p.friendship,exp:p.exp,shiny:p.shiny,variant:p.variant,luck:p.luck,fusionShiny:p.fusionShiny??false,fusionVariant:p.fusionVariant??0,fusionLuck:p.fusionLuck??0,name:p.name,types:p.getTypes({includeTeraType:false,bypassSummonData:true,ignoreThirdType:true})}});
   p.destroy();
  };
  for(const mode of [GameModes.ENDLESS,GameModes.SPLICED_ENDLESS])for(const species of [SpeciesId.BULBASAUR,SpeciesId.DITTO,SpeciesId.MEW,SpeciesId.MAGNEMITE])for(const level of [1,100,70000])for(const sample of [0,1,2])emit(mode,species,level,sample);
  const species=[SpeciesId.BULBASAUR,SpeciesId.EEVEE,SpeciesId.MAGNEMITE,SpeciesId.MEW];
  for(let sample=0;sample<500;sample++)emit(GameModes.SPLICED_ENDLESS,species[sample%4],sample%2?100:70000,sample,4);
  for(const mode of [GameModes.ENDLESS,GameModes.SPLICED_ENDLESS])for(const species of [SpeciesId.BULBASAUR,SpeciesId.EEVEE,SpeciesId.MAGNEMITE,SpeciesId.MEW])for(const sample of [0,1])emit(mode,species,65536,sample);
 }finally{EnemyPokemon.prototype.generateAndPopulateMoveset=original;}
 expect(rows).toHaveLength(588);expect(rows.filter(r=>r.mon.fusion).length).toBe(532);
 fs.writeFileSync(process.env.FUSED_MOVESET_OUT!,JSON.stringify({version:1,scope:"actual native constructor moveset generation; wild non-boss shiny-locked inputs",weather:scene.arena.weather?.weatherType??0,terrain:scene.arena.terrain?.terrainType??0,double:battle.double,pokemon:JSON.parse(fs.readFileSync("locales/en/pokemon.json","utf8")),rows})+"\n");
},240000);
