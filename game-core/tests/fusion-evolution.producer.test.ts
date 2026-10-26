import { BattleScene } from "#app/battle-scene";
import { speciesDataRegistry as registry } from "#app/globals/global-species-data-registry";
import { FusionSpeciesFormEvolution } from "#balance/pokemon-evolutions";
import { modifierTypes } from "#data/data-lists";
import { AbilityId } from "#enums/ability-id";
import { MoveId } from "#enums/move-id";
import { SpeciesId } from "#enums/species-id";
import { GameManager } from "#test/framework/game-manager";
import Phaser from "phaser";
import fs from "node:fs";
import {expect,it} from "vitest";
it("native primary and secondary evolution selection and mutation",async()=>{
 const native=BattleScene.prototype.randBattleSeedInt;
 const g=new GameManager(new Phaser.Game({type:Phaser.HEADLESS}));
 g.override.ability(AbilityId.NONE).enemyAbility(AbilityId.NONE).passiveAbility(AbilityId.NONE).enemyPassiveAbility(AbilityId.NONE).moveset(MoveId.SPLASH).enemyMoveset(MoveId.SPLASH).enemySpecies(SpeciesId.MAGIKARP).disableTrainerWaves().mysteryEncounterChance(0);
 await g.classicMode.startBattle(SpeciesId.MEW);BattleScene.prototype.randBattleSeedInt=native;
 const s:any=g.scene,p:any=s.getPlayerParty()[0];p.id=101;p.loadAssets=async()=>{};p.updateInfo=async()=>{};
 s.modifierBar.updateModifiers=()=>{};
 const pairs=[[SpeciesId.MEW,SpeciesId.GIMMIGHOUL],[SpeciesId.BULBASAUR,SpeciesId.GIMMIGHOUL],[SpeciesId.MEW,SpeciesId.COMBEE],[SpeciesId.MEW,SpeciesId.SALANDIT],[SpeciesId.MEW,SpeciesId.BULBASAUR],[SpeciesId.MEW,SpeciesId.TOGEPI],[SpeciesId.MEW,SpeciesId.EEVEE],[SpeciesId.MEW,SpeciesId.SLIGGOO]];
 const stock=()=>s.modifiers.map((m:any)=>({key:m.type.id,owner:m.pokemonId??-1,stack:m.stackCount,virtual:m.virtualStackCount}));
 const frame=()=>({id:p.id,species:p.species.speciesId,form:p.formIndex,fusion:p.fusionSpecies?.speciesId??0,fusionForm:p.fusionFormIndex,gender:p.gender,fusionGender:p.fusionGender,abilityIndex:p.abilityIndex,fusionAbilityIndex:p.fusionAbilityIndex,ability:p.getAbility().id,level:p.level,nature:p.nature,ivs:p.ivs,hp:p.hp,stats:p.stats,friendship:p.friendship,pause:p.pauseEvolutions,name:p.name,stock:stock(),global:Phaser.Math.RND.state(),battle:s.currentBattle.battleSeedState});
 const rows:any[]=[];
 const inputs:any[]=[];
 for(const offset of [20,5])for(const form of [0,1])for(const pair of pairs)for(const level of [5,16,21,33,50])for(const gender of [-1,0,1])for(const abilityIndex of [0,2])for(const trackers of [0,10]){
  if(!form||pair[1]===SpeciesId.GIMMIGHOUL)inputs.push({offset,form,pair,level,gender,abilityIndex,trackers});
 }
 // Append the hidden-ability-to-no-hidden-ability boundary without changing
 // any of the preceding 1200 native input/output observations.
 for(const offset of [20,5])for(const level of [5,16,21,33,50])for(const gender of [-1,0,1])for(const abilityIndex of [0,2])for(const trackers of [0,10])inputs.push({offset,form:0,pair:[SpeciesId.MEW,SpeciesId.CATERPIE],level,gender,abilityIndex,trackers});
 for(const offset of [20,5])for(const level of [5,16,21,33,50])for(const gender of [-1,0,1])for(const abilityIndex of [0,2])for(const trackers of [0,10])inputs.push({offset,form:0,pair:[SpeciesId.MEW,SpeciesId.TRAPINCH],level,gender,abilityIndex,trackers});
 for(const {offset,form,pair,level,gender,abilityIndex,trackers}of inputs){
  s.waveCycleOffset=offset;
  p.resetSummonData();p.resetTurnData();p.resetBattleAndWaveData();p.species=registry.getSpecies(pair[0]);p.formIndex=0;p.fusionSpecies=registry.getSpecies(pair[1]);p.fusionFormIndex=form;p.gender=gender===1?0:1;p.fusionGender=gender;p.abilityIndex=0;p.fusionAbilityIndex=abilityIndex;p.passive=false;p.customPokemonData.ability=-1;p.customPokemonData.passive=-1;p.fusionCustomPokemonData=null;p.level=level;p.friendship=220;p.nature=0;p.customPokemonData.nature=-1;p.ivs=Array(6).fill(15);p.pauseEvolutions=false;p.stats=Array(6).fill(0);p.hp=undefined;p.calculateStats();p.hp=p.getMaxHp();p.generateName();s.modifiers=[];
  if(trackers){const f=modifierTypes.EVOLUTION_TRACKER_GIMMIGHOUL,t=f().withIdFromFunc(f),m=t.newModifier(p);m.stackCount=trackers;s.modifiers.push(m);}
  Phaser.Math.RND.sow([`fusion-evolution-${rows.length%600}`]);s.currentBattle.battleSeedState=null;s.currentBattle.randSeedInt(2);
  const before=JSON.parse(JSON.stringify(frame())),e=p.getEvolution(),fusion=e instanceof FusionSpeciesFormEvolution;
  const selected=e?{species:e.speciesId,level:e.level,item:e.item,preForm:e.preFormKey,evoForm:e.evoFormKey,fusion}:null;
  if(e)await p.evolve(e,fusion?p.getFusionSpeciesForm():p.getSpeciesForm());
  rows.push({offset,before,selected,after:JSON.parse(JSON.stringify(frame()))});
 }
 expect(rows).toHaveLength(1440);fs.writeFileSync(process.env.FUSION_EVOLUTION_OUT!,JSON.stringify({version:1,biome:s.arena.biomeId,wave:s.currentBattle.waveIndex,offsets:[20,5],weather:s.arena.weather?.weatherType??0,terrain:s.arena.terrain?.terrainType??0,seed:s.seed,pokemon:JSON.parse(fs.readFileSync("locales/en/pokemon.json","utf8")),rows})+"\n");
},240000);
