import { BattleScene } from "#app/battle-scene";
import { activeOverrides } from "#app/overrides";
import { audioManager } from "#app/global-audio-manager";
import { timedEventManager } from "#app/global-event-manager";
import { modifierTypes } from "#data/data-lists";
import { BiomeId } from "#enums/biome-id";
import { SpeciesId } from "#enums/species-id";
import { MoveId } from "#enums/move-id";
import { MysteryEncounterType } from "#enums/mystery-encounter-type";
import { PokemonMove } from "#moves/pokemon-move";
import * as Dialogue from "#mystery-encounters/encounter-dialogue-utils";
import * as Utils from "#mystery-encounters/encounter-phase-utils";
import * as Mystery from "#mystery-encounters/mystery-encounter-biomes";
import { GameManager } from "#test/framework/game-manager";
import Phaser from "phaser";
import fs from "node:fs";
import { it,expect,vi } from "vitest";
it("native Trash to Treasure option owners",async()=>{
 const native=BattleScene.prototype.randBattleSeedInt;expect(vi.isMockFunction(native)).toBe(false);
 const game=new GameManager(new Phaser.Game({type:Phaser.HEADLESS}));game.override.mysteryEncounterChance(100).startingWave(113).startingBiome(BiomeId.PLAINS).disableTrainerWaves().startingLevel(50);
 vi.spyOn(Mystery,"mysteryEncountersByBiome","get").mockReturnValue(new Map([[BiomeId.PLAINS,[MysteryEncounterType.TRASH_TO_TREASURE]]]));
 vi.spyOn(timedEventManager,"getEventEncounters").mockReturnValue([]);vi.spyOn(timedEventManager,"activeEvent").mockReturnValue(undefined as any);vi.spyOn(audioManager,"playSound").mockImplementation((()=>{})as any);
 await game.runToMysteryEncounter(MysteryEncounterType.TRASH_TO_TREASURE,[SpeciesId.MEW,SpeciesId.MEW,SpeciesId.MEW]);BattleScene.prototype.randBattleSeedInt=native;
 const s:any=game.scene,enc=s.currentBattle.mysteryEncounter,original=[...s.party],pm=s.phaseManager;expect(enc.options.length).toBe(2);
 for(const[key,value]of Object.entries({ENEMY_MOVESET_OVERRIDE:[],ENEMY_LEVEL_OVERRIDE:0,ENEMY_SPECIES_OVERRIDE:null,ENEMY_FUSION_OVERRIDE:false,ENEMY_IVS_OVERRIDE:null,ENEMY_NATURE_OVERRIDE:null,ENEMY_STATUS_OVERRIDE:0,ENEMY_GENDER_OVERRIDE:null,ENEMY_FORM_OVERRIDES:{},ENEMY_SHINY_OVERRIDE:null,ENEMY_VARIANT_OVERRIDE:null}))vi.spyOn(activeOverrides as any,key,"get").mockReturnValue(value);
 for(const p of original)vi.spyOn(p,"updateInfo").mockResolvedValue(undefined);
 const leave=vi.spyOn(Utils,"leaveEncounterWithoutBattle").mockImplementation(()=>{}),rewards=vi.spyOn(Utils,"setEncounterRewards").mockImplementation(()=>{});
 vi.spyOn(Utils,"transitionMysteryEncounterIntroVisuals").mockResolvedValue(undefined as any);const texts:string[]=[];
 vi.spyOn(Dialogue,"showEncounterText").mockImplementation((async(key:any)=>{texts.push(key);})as any);
 const arg=(t:any)=>t.id==="BERRY"?t.berryType:t.id==="BASE_STAT_BOOSTER"?t.stat:-1;
 const items=(list:any[])=>list.map((m:any)=>({key:m.type.id,arg:arg(m.type),owner:m.pokemonId??0,stack:m.stackCount}));
 const mon=(p:any)=>({species:p.species.speciesId,form:p.formIndex,level:p.level,id:p.id,ivs:[...p.ivs],nature:p.nature,abilityIndex:p.abilityIndex,gender:p.gender,friendship:p.friendship,hp:p.hp,stats:[...p.stats],boss:p.bossSegments,shiny:p.shiny,variant:p.variant,luck:p.luck,passive:p.passive,moves:p.getMoveset().map((m:any)=>[m.moveId,m.ppUsed,m.ppUp,m.maxPpOverride??-1])});
 const rows:any[]=[];
 for(const wave of [100,123,180])for(let option=0;option<2;option++)for(let stock=0;stock<4;stock++)for(let sample=0;sample<8;sample++){
  const index=rows.length,order=sample%2?[2,0,1]:[0,1,2];s.party=order.map(k=>original[k]);s.currentBattle.enemyParty=[];s.modifiers=[];s.enemyModifiers=[];s.currentBattle.double=false;s.currentBattle.waveIndex=wave;s.currentBattle.mysteryEncounter=enc;
  s.setSeed(`trash-run-${index}`);s.waveSeed=`trash-wave-${index}`;s.waveCycleOffset=5;s.arena.biomeId=BiomeId.PLAINS;s.arena.weather=null;s.arena.terrain=null;s.gameData.trainerId=12345;s.gameData.secretId=34567;
  for(let k=0;k<3;k++){const p=original[k];p.id=101+k;p.level=50;p.nature=0;p.customPokemonData.nature=-1;p.ivs=Array(6).fill(15);p.fusionSpecies=null;p.shiny=false;p.variant=0;p.luck=0;p.resetSummonData();p.resetBattleAndWaveData();p.moveset=[new PokemonMove(MoveId.SPLASH)];p.hp=undefined;p.calculateStats();p.hp=p.getMaxHp();}
  const add=(key:string,p:any,stack:number)=>{const t:any=Utils.generateModifierType((modifierTypes as any)[key]);const m:any=t.newModifier(p);m.stackCount=stack;s.addModifier(m,true,false,false,true);};
  if(stock)for(let k=0;k<(stock===2?3:1);k++){add("LEFTOVERS",s.party[k],stock===3?3:4);if(stock!==3)add("SHELL_BELL",s.party[k],4);}
  if(sample%2)add("EXP_SHARE",undefined,1);
  enc.seedOffset=undefined;enc.updateSeedOffset();enc.misc=undefined;enc.startOfBattleEffects=[];enc.enemyPartyConfigs=[];enc.selectedOption=enc.options[option];leave.mockClear();rewards.mockClear();texts.length=0;pm.clearAllPhases();
  Phaser.Math.RND.sow([`trash-ambient-${index}`]);const before=Phaser.Math.RND.state();s.executeWithSeedOffset(()=>enc.onInit(),wave);const afterInit=Phaser.Math.RND.state();
  const config=enc.enemyPartyConfigs[0],input=items(s.modifiers),cfg=config.pokemonConfigs[0];
  const queued:any[]=[];const unshift=vi.spyOn(pm,"unshiftNew").mockImplementation(((name:string,...args:any[])=>queued.push([name,...args.map(a=>typeof a==='number'||typeof a==='boolean'?a:null)]))as any),push=vi.spyOn(pm,"pushNew").mockImplementation(((name:string,...args:any[])=>queued.push([name,...args.map(a=>typeof a==='number'||typeof a==='boolean'?a:null)]))as any);
  let done:any;s.executeWithSeedOffset(()=>{done=enc.options[option].onOptionPhase();},enc.getSeedOffset()*500);await done;unshift.mockRestore();push.mockRestore();
  const reward:any=rewards.mock.calls[0]?.[0];rows.push({index,wave,option,stock,sample,order,before,afterInit,after:Phaser.Math.RND.state(),input,output:items(s.modifiers),config:{form:cfg.formIndex,add:config.levelAdditiveModifier,segments:cfg.bossSegmentModifier,moves:cfg.moveSet,items:cfg.modifierConfigs.map((m:any)=>({key:m.modifier.id,arg:arg(m.modifier),stack:m.stackCount??1}))},cost:enc.dialogueTokens.costMultiplier,requirements:enc.options.map((o:any)=>o.meetsRequirements()),leave:leave.mock.calls.map(a=>a[0]),texts:[...texts],queued,reward:reward?{funcs:reward.guaranteedModifierTypeFuncs.map((f:any)=>Utils.generateModifierType(f).id),tiers:reward.guaranteedModifierTiers,fill:reward.fillRemaining}:null,effects:enc.startOfBattleEffects.map((e:any)=>[e.sourceBattlerIndex,e.targets,e.move.moveId,e.useMode]),enemy:option===0?mon(s.currentBattle.enemyParty[0]):null,enemyItems:items(s.enemyModifiers)});
 }
 expect(rows).toHaveLength(192);fs.writeFileSync(process.env.TRASH_OUT!,JSON.stringify({version:1,battleLocale:fs.readFileSync("locales/en/battle.json","utf8"),rows})+'\n');
},240000);
