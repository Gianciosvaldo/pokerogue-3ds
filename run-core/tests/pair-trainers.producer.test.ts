/* Actual configured pair-trainer factories. Observe the addEnemyPokemon entry
 * and RNG there, then execute its real constructor. Does not claim full
 * constructor/moveset parity from observations before that boundary. */
import {BattleScene} from "#app/battle-scene";
import {activeOverrides} from "#app/overrides";
import {trainerConfigs} from "#trainers/trainer-config";
import {TrainerType} from "#enums/trainer-type";
import {TrainerVariant} from "#enums/trainer-variant";
import {SpeciesId} from "#enums/species-id";
import {Trainer} from "#field/trainer";
import {GameManager} from "#test/framework/game-manager";
import Phaser from "phaser";
import fs from "node:fs";
import {it,expect,vi} from "vitest";
it('pair trainer factory constructor inputs',async()=>{
 const native=BattleScene.prototype.randBattleSeedInt;expect(vi.isMockFunction(native)).toBe(false);
 const game=new GameManager(new Phaser.Game({type:Phaser.HEADLESS}));game.override.startingWave(3).enemySpecies(SpeciesId.MAGIKARP);
 await game.classicMode.startBattle(SpeciesId.BULBASAUR);BattleScene.prototype.randBattleSeedInt=native;
 for(const[key,value]of Object.entries({ENEMY_MOVESET_OVERRIDE:[],ENEMY_LEVEL_OVERRIDE:0,ENEMY_SPECIES_OVERRIDE:null,ENEMY_FUSION_OVERRIDE:false,ENEMY_IVS_OVERRIDE:null,ENEMY_NATURE_OVERRIDE:null,ENEMY_STATUS_OVERRIDE:0,ENEMY_GENDER_OVERRIDE:null,ENEMY_FORM_OVERRIDES:{},ENEMY_SHINY_OVERRIDE:null,ENEMY_VARIANT_OVERRIDE:null}))vi.spyOn(activeOverrides as any,key,'get').mockReturnValue(value);
 const s:any=game.scene,original=s.addEnemyPokemon,rows:any[]=[];let observed:any;
 const state=()=>Phaser.Math.RND.state().replace(/^!rnd,/,'').split(',').map(Number);
 s.addEnemyPokemon=function(species:any,level:number,slot:number,...args:any[]){observed={species:species.speciesId,level,slot,rng:state()};return original.call(this,species,level,slot,...args);};
 for(const mode of [{waves:[11,45,99,180],kind:-1},{waves:[20],kind:-1},{waves:[11,45,99,180,20],kind:0}])for(const type of [TrainerType.TWINS,TrainerType.MYSTERIOUS_SISTERS]){
  const config=trainerConfigs[type],trainer:any=new Trainer(type,TrainerVariant.DOUBLE,0,undefined,undefined,config);s.currentBattle.trainer=trainer;
  for(let key=0;key<2;key++)for(const wave of mode.waves)for(const level of [5,35,80,125])for(let sample=0;sample<4;sample++){
   const index=rows.length;s.currentBattle.waveIndex=wave;s.currentBattle.enemyParty=[];s.enemyModifiers=[];
   const template=trainer.getPartyTemplate(),strength=template.getStrength(key),kind=mode.kind<0?template.getEvoThresholdKind(key):mode.kind;
   Phaser.Math.RND.sow([`pair-factory-${index}`]);const before=state();observed=null;
   const pokemon=config.partyMemberFuncs[key](level,strength,kind);expect(observed).not.toBeNull();expect(pokemon.species.speciesId).toBe(observed.species);
   rows.push({index,type,key,wave,level,strength,kind,before,observed});pokemon.destroy();
  }
  trainer.destroy();s.currentBattle.trainer=null;
 }
 expect(rows.length).toBe(640);fs.writeFileSync(process.env.PAIR_TRAINERS_OUT??'/tmp/pair-trainers.json',JSON.stringify(rows)+'\n');
},120000);
