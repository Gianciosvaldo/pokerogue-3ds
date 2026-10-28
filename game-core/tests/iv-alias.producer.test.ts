import { TrainingSessionEncounter } from "#app/data/mystery-encounters/encounters/training-session-encounter";
import { Status } from "#data/status-effect";
import { StatusEffect } from "#enums/status-effect";
import { getGameMode } from "#app/game-mode";
import { GameModes } from "#enums/game-modes";
import { BattleScene } from "#app/battle-scene";
import { speciesDataRegistry as registry } from "#app/globals/global-species-data-registry";
import { FusionSpeciesFormEvolution } from "#balance/pokemon-evolutions";
import { getLevelTotalExp } from "#data/exp";
import { audioManager } from "#app/global-audio-manager";
import { allMoves } from "#data/data-lists";
import { UiMode } from "#enums/ui-mode";
import { LearnMoveType } from "#enums/learn-move-type";
import { LearnMovePhase } from "#phases/learn-move-phase";
import { PokemonData } from "#system/pokemon-data";
import { modifierTypes } from "#data/data-lists";
import { AbilityId } from "#enums/ability-id";
import { MoveId } from "#enums/move-id";
import { SpeciesId } from "#enums/species-id";
import { Pokemon, PlayerPokemon } from "#field/pokemon";
import { PokemonMove } from "#moves/pokemon-move";
import { GameManager } from "#test/framework/game-manager";
import Phaser from "phaser";
import fs from "node:fs";
import {gzipSync} from "node:zlib";
import {expect,it,vi} from "vitest";
vi.mock("#mystery-encounters/encounter-phase-utils",async original=>({...await original<any>(),initBattleWithEnemyConfig:async()=>{}}));
vi.mock("#data/battle-anims",async original=>({...await original<any>(),initMoveAnim:async()=>{},loadMoveAnimAssets:()=>{}}));
it("native special evolution owner",async()=>{
 const native=BattleScene.prototype.randBattleSeedInt,evolve=PlayerPokemon.prototype.evolve,nativeLearn=(LearnMovePhase.prototype as any).learnMove;
 expect(vi.isMockFunction(native)).toBe(false);expect(vi.isMockFunction(evolve)).toBe(false);expect(vi.isMockFunction(nativeLearn)).toBe(false);
 const g=new GameManager(new Phaser.Game({type:Phaser.HEADLESS}));
 g.override.ability(AbilityId.NONE).enemyAbility(AbilityId.NONE).moveset(MoveId.SPLASH).enemyMoveset(MoveId.SPLASH).enemySpecies(SpeciesId.MAGIKARP).disableTrainerWaves().mysteryEncounterChance(0);
 await g.classicMode.startBattle(SpeciesId.NINCADA,SpeciesId.MAGIKARP,SpeciesId.BULBASAUR,SpeciesId.MEW,SpeciesId.EEVEE,SpeciesId.PIKACHU);
 BattleScene.prototype.randBattleSeedInt=native;
 g.override.playerIVs(null).nature(null).shiny(null);
 const s:any=g.scene,original=s.getPlayerParty().slice(),p:any=original[0];original.forEach((m:any,i:number)=>m.id=101+i);
 vi.spyOn(Pokemon.prototype,"loadAssets").mockResolvedValue(undefined);
 vi.spyOn(Pokemon.prototype,"updateInfo").mockResolvedValue(undefined);
 vi.spyOn(PlayerPokemon.prototype,"init").mockImplementation(()=>{});
 s.modifierBar.updateModifiers=()=>{};
 // Constructor choices of the untouched party are input controls too.
 for(const m of original){m.abilityIndex=0;m.nature=0;m.customPokemonData.nature=-1;m.shiny=false;m.variant=0;m.luck=0;m.ivs=[31,31,31,31,31,31];m.gender=m.species.malePercent==null?-1:1;m.teraType=m.getTypes({includeTeraType:false,bypassSummonData:true,ignoreThirdType:true})[0];m.stats=Array(6).fill(0);m.hp=undefined;m.calculateStats();}

 vi.spyOn(s.ui,"showText").mockImplementation(()=>{});vi.spyOn(s.ui,"setMode").mockResolvedValue(undefined);vi.spyOn(audioManager,"playSound").mockImplementation(()=>{});
 const learn=async(index:number)=>{const m=s.party[index],move=m.getCompatibleTms().find((id:number)=>!allMoves[id].isUnimplemented&&!m.usedTMs.includes(id));expect(move).toBeDefined();const phase:any=new LearnMovePhase(index,move,LearnMoveType.TM);phase.messageMode=UiMode.MESSAGE;await phase.learnMove(0,allMoves[move],m);return {index,move,after:JSON.parse(JSON.stringify(frame(),(_,v)=>typeof v==="bigint"?v.toString():v))};};
 const profileIds=[1,2,151,290,291,292];
 const profile=()=>profileIds.map(id=>({id,dex:s.gameData.dexData[id],starter:s.gameData.starterData[id]??null}));
 const frame=()=>({profile:profile(),party:s.party.map((m:any)=>({id:m.id,species:m.species.speciesId,form:m.formIndex,fusion:m.fusionSpecies?.speciesId??0,fusionForm:m.fusionFormIndex,abilityIndex:m.abilityIndex,fusionAbilityIndex:m.fusionAbilityIndex,ability:m.getAbility().id,gender:m.gender,fusionGender:m.fusionGender,level:m.level,exp:m.exp,nature:m.nature,effectiveNature:m.getNature(),customAbility:m.customPokemonData.ability,customPassive:m.customPokemonData.passive,customNature:m.customPokemonData.nature,ivs:m.ivs,hp:m.hp,stats:m.stats,status:m.status,friendship:m.friendship,passive:m.passive,pokerus:m.pokerus,ball:m.pokeball,shiny:m.shiny,variant:m.variant,luck:m.luck,fusionShiny:m.fusionShiny,fusionVariant:m.fusionVariant,fusionLuck:m.fusionLuck,fusionTera:m.fusionTeraType,tera:m.teraType,met:[m.metLevel,m.metBiome,m.metSpecies,m.metWave],used:[...m.usedTMs],moves:m.moveset.map((v:any)=>({move:v.moveId,used:v.ppUsed,up:v.ppUp,max:v.maxPpOverride??-1}))})),stock:s.modifiers.map((m:any)=>({key:m.type.id,owner:m.pokemonId??-1,stack:m.stackCount,virtual:m.virtualStackCount,locked:!m.isTransferable,cls:m.constructor.name})),balls:{...s.pokeballCounts},global:Phaser.Math.RND.state(),battle:s.currentBattle.battleSeedState});
 expect((LearnMovePhase.prototype as any).learnMove).toBe(nativeLearn);const rows:any[]=[],initialProfile=profile().map(e=>({...e,dex:{...e.dex,ivs:[...e.dex.ivs]},starter:e.starter?{...e.starter}:null}));

 for(const values of [[0,1,5,10,20,30],[1,10,11,20,21,30],[30,30,30,30,30,30],[31,31,31,31,31,31]])for(const trainChild of [false,true])for(const reload of [false,true]){
  for(const entry of initialProfile){s.gameData.dexData[entry.id]={...entry.dex,ivs:[...entry.dex.ivs]};if(entry.starter)s.gameData.starterData[entry.id]={...entry.starter};}
  s.gameMode=getGameMode(GameModes.CLASSIC);s.waveCycleOffset=20;s.party=[p,original[3]];s.modifiers=[];s.pokeballCounts[0]=1;
  p.resetSummonData();p.resetTurnData();p.resetBattleAndWaveData();p.species=registry.getSpecies(SpeciesId.NINCADA);p.formIndex=0;p.fusionSpecies=null;p.fusionFormIndex=0;p.abilityIndex=0;p.fusionAbilityIndex=0;p.customPokemonData.ability=-1;p.customPokemonData.passive=-1;p.fusionCustomPokemonData=null;p.level=20;p.exp=getLevelTotalExp(20,p.species.growthRate);p.ivs=[...values];p.nature=0;p.customPokemonData.nature=-1;p.passive=false;p.pokerus=false;p.pauseEvolutions=false;p.teraType=6;p.gender=1;p.shiny=false;p.variant=0;p.luck=0;p.usedTMs=[];p.moveset=[new PokemonMove(MoveId.SCRATCH),new PokemonMove(MoveId.HARDEN)];p.metLevel=5;p.metBiome=-1;p.metSpecies=SpeciesId.NINCADA;p.metWave=-1;p.status=undefined;p.stats=Array(6).fill(0);p.hp=undefined;p.calculateStats();
  Phaser.Math.RND.sow([`iv-owner-${rows.length}`]);s.currentBattle.battleSeedState=null;s.currentBattle.randSeedInt(2);
  const snap=()=>JSON.parse(JSON.stringify(frame(),(_,v)=>typeof v==="bigint"?v.toString():v));
  const before=snap(),e=p.getEvolution();await p.evolve(e,p.getSpeciesForm());await Promise.resolve();await Promise.resolve();
  const evolved=snap();expect(s.party[0].ivs).toBe(s.party[2].ivs);
  if(reload){const saved=JSON.parse(JSON.stringify(s.party.map((m:any)=>new PokemonData(m))));s.party=saved.map((v:any)=>new PokemonData(v).toPokemon());expect(s.party[0].ivs).not.toBe(s.party[2].ivs);}
  const ready=snap(),index=trainChild?2:0,retained=s.party[index];
  const encounter:any=TrainingSessionEncounter;encounter.misc={playerPokemon:retained};s.currentBattle.mysteryEncounter=encounter;
  await encounter.options[0].onOptionPhase();
  expect(s.party).not.toContain(retained);
  const away=snap();s.executeWithSeedOffset(()=>encounter.doEncounterRewards(),s.currentBattle.waveIndex*1000);await s.updateModifiers(true);
  const after=snap();rows.push({values,trainChild,reload,index,before,evolved,ready,away,after,retainedIvs:[...retained.ivs]});
 }
 fs.writeFileSync(process.env.IV_OWNER_OUT!,gzipSync(JSON.stringify({version:1,env:{seed:s.seed,waveSeed:s.waveSeed,wave:s.currentBattle.waveIndex,biome:s.arena.biomeId,offset:s.waveCycleOffset,weather:0,terrain:0},scope:"actual Shedinja constructor and TrainingSession light-training option/reward callback; battle installation, visuals and UI bypassed",rows})+"\n"));
},240000);
