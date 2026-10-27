import { BattleScene } from "#app/battle-scene";
import { getGameMode } from "#app/game-mode";
import { speciesDataRegistry as registry } from "#app/globals/global-species-data-registry";
import { activeOverrides } from "#app/overrides";
import { modifierTypes } from "#data/data-lists";
import { generateModifierType } from "#mystery-encounters/encounter-phase-utils";
import { BattleType } from "#enums/battle-type";
import { GameModes } from "#enums/game-modes";
import { NumberHolder } from "#utils/common";
import { AbilityId } from "#enums/ability-id";
import { allMoves } from "#data/data-lists";
import { MoveId } from "#enums/move-id";
import { SpeciesId } from "#enums/species-id";
import { EnemyPokemon, Pokemon } from "#field/pokemon";
import { PokemonMove } from "#moves/pokemon-move";
import { GameManager } from "#test/framework/game-manager";
import Phaser from "phaser";
import fs from "node:fs";
import { it,expect,vi } from "vitest";
it("native fusion variable move type owners",async()=>{
 const native=BattleScene.prototype.randBattleSeedInt;
 expect(process.versions.node.split(".")[0]).toBe("22");
 const nativeType=Pokemon.prototype.getMoveType;expect(vi.isMockFunction(nativeType)).toBe(false);
 const nativeBaseMovegen=Pokemon.prototype.generateAndPopulateMoveset;
 expect(vi.isMockFunction(nativeBaseMovegen)).toBe(false);
 const nativeMovegen=EnemyPokemon.prototype.generateAndPopulateMoveset;
 expect(vi.isMockFunction(nativeMovegen)).toBe(false);
 const game=new GameManager(new Phaser.Game({type:Phaser.HEADLESS}));
 game.override.battleStyle("single").startingWave(5).enemySpecies(SpeciesId.MAGIKARP).enemyMoveset(MoveId.SPLASH);
 await game.classicMode.startBattle(SpeciesId.MEW);BattleScene.prototype.randBattleSeedInt=native;
 expect(Pokemon.prototype.generateAndPopulateMoveset).toBe(nativeBaseMovegen);expect(Pokemon.prototype.getMoveType).toBe(nativeType);
 const scene:any=game.scene,battle=scene.currentBattle;
 for(const [key,value]of Object.entries({ENEMY_MOVESET_OVERRIDE:[],ENEMY_LEVEL_OVERRIDE:0,ENEMY_SPECIES_OVERRIDE:null,ENEMY_FUSION_OVERRIDE:false,ENEMY_FUSION_SPECIES_OVERRIDE:null,ENEMY_IVS_OVERRIDE:null,ENEMY_NATURE_OVERRIDE:null,ENEMY_STATUS_OVERRIDE:0,ENEMY_GENDER_OVERRIDE:null,ENEMY_FORM_OVERRIDES:{},ENEMY_SHINY_OVERRIDE:null,ENEMY_VARIANT_OVERRIDE:null}))vi.spyOn(activeOverrides as any,key,"get").mockReturnValue(value);
 scene.modifiers=[];scene.enemyModifiers=[];battle.battleType=BattleType.WILD;battle.trainer=null;
 const rows:any[]=[];
 scene.gameMode=getGameMode(GameModes.CLASSIC);battle.gameMode=scene.gameMode;scene.arena.weather=null;scene.arena.terrain=null;
 const p:any=scene.addEnemyPokemon(registry.getSpecies(SpeciesId.MAGIKARP),50,0,false,true);
 const definitions=[{moves:[MoveId.JUDGMENT,MoveId.MULTI_ATTACK],species:[SpeciesId.ARCEUS,SpeciesId.SILVALLY]},{moves:[MoveId.TECHNO_BLAST],species:[SpeciesId.GENESECT]},{moves:[MoveId.AURA_WHEEL],species:[SpeciesId.MORPEKO]},{moves:[MoveId.RAGING_BULL],species:[SpeciesId.PALDEA_TAUROS]},{moves:[MoveId.IVY_CUDGEL],species:[SpeciesId.OGERPON]}];
 const forms=(s:number)=>Math.max(registry.getSpecies(s).forms.length,1);
 const emit=(move:number,primary:number,form:number,fusion:number,fusionForm:number)=>{
  p.resetSummonData();p.resetTurnData();p.resetBattleAndWaveData();p.species=registry.getSpecies(primary);p.formIndex=form;
  p.fusionSpecies=fusion?registry.getSpecies(fusion):null;p.fusionFormIndex=fusionForm;p.fusionAbilityIndex=0;p.fusionGender=p.fusionSpecies?(p.fusionSpecies.malePercent===null?-1:p.fusionSpecies.malePercent===0?1:0):-1;
  p.gender=p.species.malePercent===null?-1:p.species.malePercent===0?1:0;p.abilityIndex=0;p.customPokemonData.ability=AbilityId.BALL_FETCH;p.customPokemonData.passive=AbilityId.NONE;
  p.passive=false;p.bossSegments=0;p.moveset=[];p.nature=0;p.ivs=Array(6).fill(15);p.level=50;p.calculateStats();p.hp=p.stats[0];
  expect(p.getAbility().hasAttr("MoveTypeChangeAbAttr")).toBe(false);expect(p.hasPassive()).toBe(false);
  const m:any=allMoves[move],attr:any=m.getAttrs("VariableMoveTypeAttr").at(-1);expect(attr).toBeDefined();expect(vi.isMockFunction(attr.apply)).toBe(false);expect(vi.isMockFunction(p.getMoveType)).toBe(false);
  const before=Phaser.Math.RND.state(),h=new NumberHolder(m.type),applied=attr.apply(p,p,m,[h]);
  const variable=h.value,actual=p.getMoveType(m),items=attr.getTypesForItemSpawn(p,m),movegen=attr.getTypeForMovegen(p,m,false);
  expect(Phaser.Math.RND.state()).toBe(before);
  rows.push({move,primary,form,fusion,fusionForm,fusionSpeciesForm:p.fusionSpecies?.formIndex??-1,fusionGender:p.fusionGender,gender:p.gender,ability:p.getAbility().id,applied,variable,actual,items,movegen,before,after:Phaser.Math.RND.state()});
 };
 for(const d of definitions)for(const move of d.moves){
  emit(move,SpeciesId.MEW,0,0,0);emit(move,SpeciesId.MEW,0,SpeciesId.RATTATA,0);
  for(const id of d.species){
   for(let f=0;f<forms(id);f++){emit(move,id,f,0,0);emit(move,id,f,SpeciesId.RATTATA,0);emit(move,SpeciesId.MEW,0,id,f);}
   for(const other of d.species)for(let f=0;f<forms(id);f++)for(let ff=0;ff<forms(other);ff++)emit(move,id,f,other,ff);
  }
 }
 p.destroy();expect(rows.length).toBeGreaterThan(2000);
 fs.writeFileSync(process.env.FUSION_VARIABLE_TYPES_OUT!,JSON.stringify({version:1,scope:"native variable attribute apply, Pokemon.getMoveType, item-spawn and moveset type owners; raw legal species/form snapshots",definitions:definitions.map(d=>({...d,forms:d.species.map(s=>[s,forms(s)])})),rows})+"\n");
},240000);
