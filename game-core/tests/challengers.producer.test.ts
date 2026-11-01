/* Actual pinned Mysterious Challengers initialization and option configuration.
 * initBattleWithEnemyConfig is an observation boundary here: this does not
 * claim native generated parties, movesets or complete battle parity. */
import {BattleScene} from "#app/battle-scene";
import {modifierTypes} from "#data/data-lists";
import {BiomeId} from "#enums/biome-id";
import {MysteryEncounterType} from "#enums/mystery-encounter-type";
import {SpeciesId} from "#enums/species-id";
import {TrainerVariant} from "#enums/trainer-variant";
import {Trainer} from "#field/trainer";
import * as Utils from "#mystery-encounters/encounter-phase-utils";
import * as Mystery from "#mystery-encounters/mystery-encounter-biomes";
import {GameManager} from "#test/framework/game-manager";
import Phaser from "phaser";
import {expect,it,vi} from "vitest";
import fs from "node:fs";
import i18next from "i18next";
it('Mysterious Challengers native configuration owners',async()=>{
 const native=BattleScene.prototype.randBattleSeedInt;expect(vi.isMockFunction(native)).toBe(false);
 const game=new GameManager(new Phaser.Game({type:Phaser.HEADLESS}));
 game.override.mysteryEncounterChance(100).startingWave(113).startingBiome(BiomeId.PLAINS).disableTrainerWaves().startingLevel(50);
 vi.spyOn(Mystery,'mysteryEncountersByBiome','get').mockReturnValue(new Map([[BiomeId.PLAINS,[MysteryEncounterType.MYSTERIOUS_CHALLENGERS]]]));
 await game.runToMysteryEncounter(MysteryEncounterType.MYSTERIOUS_CHALLENGERS,[SpeciesId.MEW,SpeciesId.MEW]);BattleScene.prototype.randBattleSeedInt=native;
 const s:any=game.scene,enc=s.currentBattle.mysteryEncounter,rows:any[]=[];
 let constructed:any;
 const battle=vi.spyOn(Utils,'initBattleWithEnemyConfig').mockImplementation(async(config:any)=>{
  const variant=config.trainerConfig.doubleOnly?TrainerVariant.DOUBLE:config.female?TrainerVariant.FEMALE:TrainerVariant.DEFAULT;
  const t:any=new Trainer(config.trainerConfig.trainerType,variant,undefined,undefined,undefined,config.trainerConfig);
  const nameIndex=(key:string|undefined)=>{if(!key)return -1;const dot=key.lastIndexOf('.');return Object.keys(i18next.t(key.slice(0,dot),{returnObjects:true}) as any).indexOf(key.slice(dot+1));};
  const template=t.getPartyTemplate(),levels=t.getPartyLevels(s.currentBattle.waveIndex).map((n:number)=>n+Math.max(Math.round(s.currentBattle.waveIndex/10*(config.levelAdditiveModifier??0)),0));
  constructed={variant,templateIndex:t.partyTemplateIndex,name:t.nameKey??null,partner:t.partnerNameKey??null,nameIndex:nameIndex(t.nameKey),partnerIndex:nameIndex(t.partnerNameKey),levels,template:{size:template.size,strength:template.strength,members:levels.map((_:number,i:number)=>[template.getStrength(i),template.getEvoThresholdKind(i),+template.isSameSpecies(i),+template.isBalanced(i)])}};t.destroy();
 }),rewards=vi.spyOn(Utils,'setEncounterRewards').mockImplementation(()=>{});
 for(const wave of [11,19,45,99,123,180,21,31,61,91])for(const biome of [BiomeId.PLAINS,BiomeId.FOREST,BiomeId.BEACH,BiomeId.VOLCANO])for(let sample=0;sample<8;sample++){
  const index=rows.length,option=index%3;s.currentBattle.waveIndex=wave;s.newArena(biome);s.waveCycleOffset=5;s.setSeed(`challengers-run-${index}`);s.waveSeed=`challengers-wave-${index}`;
  enc.seedOffset=undefined;enc.updateSeedOffset();enc.enemyPartyConfigs=[];enc.selectedOption=enc.options[option];enc.expMultiplier=1;battle.mockClear();rewards.mockClear();
  Phaser.Math.RND.sow([`challengers-ambient-${index}`]);const before=Phaser.Math.RND.state();s.executeWithSeedOffset(()=>enc.onInit(),wave);expect(Phaser.Math.RND.state()).toBe(before);
  expect(enc.enemyPartyConfigs.length).toBe(3);
  const configs=enc.enemyPartyConfigs.map((c:any,k:number)=>({type:c.trainerConfig.trainerType,female:c.female,doubleOnly:c.trainerConfig.doubleOnly,add:c.levelAdditiveModifier??0,sprite:enc.spriteConfigs[k].spriteKey}));
  let done:any;s.executeWithSeedOffset(()=>{done=enc.options[option].onOptionPhase();},enc.getSeedOffset()*500);await done;
  expect(battle.mock.calls.length).toBe(1);expect(battle.mock.calls[0][0]).toBe(enc.enemyPartyConfigs[option]);expect(rewards.mock.calls.length).toBe(1);
  const r:any=rewards.mock.calls[0][0];const funcs=(r.guaranteedModifierTypeFuncs??[]).map((f:any)=>{const keys=Object.entries(modifierTypes).filter(([,v])=>v===f).map(([k])=>k);expect(keys.length).toBe(1);return keys[0];});
  rows.push({index,wave,biome,option,configs,constructed,rewards:{funcs,tiers:r.guaranteedModifierTiers??[],fill:r.fillRemaining}});
 }
 expect(rows.length).toBe(320);fs.writeFileSync(process.env.CHALLENGERS_OUT??'/tmp/challengers.json',JSON.stringify(rows)+'\n');
},120000);
