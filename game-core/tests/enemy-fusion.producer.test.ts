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
import { EnemyPokemon } from "#field/pokemon";
import { PokemonMove } from "#moves/pokemon-move";
import { GameManager } from "#test/framework/game-manager";
import Phaser from "phaser";
import fs from "node:fs";
import { it,expect,vi } from "vitest";
it("native enemy fusion constructor boundary",async()=>{
 const native=BattleScene.prototype.randBattleSeedInt;
 const game=new GameManager(new Phaser.Game({type:Phaser.HEADLESS}));
 game.override.battleStyle("single").startingWave(5).enemySpecies(SpeciesId.MAGIKARP).enemyMoveset(MoveId.SPLASH);
 await game.classicMode.startBattle(SpeciesId.MEW);BattleScene.prototype.randBattleSeedInt=native;
 const scene:any=game.scene,battle=scene.currentBattle;
 for(const [key,value]of Object.entries({ENEMY_LEVEL_OVERRIDE:0,ENEMY_SPECIES_OVERRIDE:null,ENEMY_FUSION_OVERRIDE:false,ENEMY_FUSION_SPECIES_OVERRIDE:null,ENEMY_IVS_OVERRIDE:null,ENEMY_NATURE_OVERRIDE:null,ENEMY_STATUS_OVERRIDE:0,ENEMY_GENDER_OVERRIDE:null,ENEMY_FORM_OVERRIDES:{},ENEMY_SHINY_OVERRIDE:null,ENEMY_VARIANT_OVERRIDE:null}))vi.spyOn(activeOverrides as any,key,"get").mockReturnValue(value);
 // Explicit source-constructor boundary: fixed moves, no RNG draws. This does
 // not claim the owning moveset generator or a complete generated encounter.
 const original=EnemyPokemon.prototype.generateAndPopulateMoveset;
 EnemyPokemon.prototype.generateAndPopulateMoveset=function(){this.moveset=[new PokemonMove(MoveId.SPLASH)];};
 scene.modifiers=[];scene.enemyModifiers=[];battle.battleType=BattleType.WILD;battle.trainer=null;
 const rows:any[]=[];
 try{
  const emit=(mode:number,species:SpeciesId,level:number,sample:number,charms=0)=>{
   scene.modifiers=[];if(charms){const m:any=generateModifierType(modifierTypes.ABILITY_CHARM).newModifier();m.stackCount=charms;scene.modifiers.push(m);}
   const gm=getGameMode(mode);scene.gameMode=gm;battle.gameMode=gm;battle.waveIndex=1;scene.arena.biomeId=0;scene.arena.lastTimeOfDay=undefined;scene.waveCycleOffset=5;scene.seed="enemy-fusion-projection";scene.waveSeed="enemy-fusion-wave";
   Phaser.Math.RND.sow([`enemy-fusion-projection-${rows.length}`]);const before=Phaser.Math.RND.state();
   const p:any=scene.addEnemyPokemon(registry.getSpecies(species),level,0,false,true);
   rows.push({mode,species,level,sample,charms,before,after:Phaser.Math.RND.state(),mon:{id:p.id,species:p.species.speciesId,form:p.formIndex,gender:p.gender,nature:p.nature,abilityIndex:p.abilityIndex,ability:p.getAbility().id,fusion:p.fusionSpecies?.speciesId??0,fusionForm:p.fusionFormIndex??0,fusionGender:p.fusionGender??-1,fusionAbilityIndex:p.fusionAbilityIndex??0,tera:p.teraType,ivs:[...p.ivs],stats:[...p.stats],hp:p.hp,friendship:p.friendship,exp:p.exp,shiny:p.shiny,variant:p.variant,luck:p.luck,fusionShiny:p.fusionShiny??false,fusionVariant:p.fusionVariant??0,fusionLuck:p.fusionLuck??0,name:p.name,types:p.getTypes({includeTeraType:false,bypassSummonData:true,ignoreThirdType:true})}});
   p.destroy();
  };
  for(const mode of [GameModes.ENDLESS,GameModes.SPLICED_ENDLESS])for(const species of [SpeciesId.BULBASAUR,SpeciesId.DITTO,SpeciesId.MEW,SpeciesId.MAGNEMITE])for(const level of [1,100,70000])for(const sample of [0,1,2])emit(mode,species,level,sample);
  const species=[SpeciesId.BULBASAUR,SpeciesId.EEVEE,SpeciesId.MAGNEMITE,SpeciesId.MEW];
  for(let sample=0;sample<500;sample++)emit(GameModes.SPLICED_ENDLESS,species[sample%4],sample%2?100:70000,sample,4);
 }finally{EnemyPokemon.prototype.generateAndPopulateMoveset=original;}
 expect(rows).toHaveLength(572);expect(rows.filter(r=>r.mon.fusion).length).toBe(524);
 fs.writeFileSync(process.env.ENEMY_FUSION_OUT!,JSON.stringify({version:1,scope:"native constructor with fixed no-RNG moveset; no enemy items/boss/complete encounter claim",pokemon:JSON.parse(fs.readFileSync("locales/en/pokemon.json","utf8")),rows})+"\n");
},240000);
