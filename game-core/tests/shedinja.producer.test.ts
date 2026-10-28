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
 for(const offset of [20,25])for(const mode of [GameModes.CLASSIC,GameModes.ENDLESS,GameModes.SPLICED_ENDLESS,GameModes.DAILY,GameModes.CHALLENGE])for(const pair of [[SpeciesId.NINCADA,0],[SpeciesId.NINCADA,SpeciesId.BULBASAUR],[SpeciesId.BULBASAUR,SpeciesId.NINCADA],[SpeciesId.MEW,SpeciesId.NINCADA],[SpeciesId.NINCADA,SpeciesId.NINCADA]])for(const size of [1,2,3,4,5,6])for(const ball of [0,1])for(const held of ["LEFTOVERS","EVIOLITE","MYSTERY_ENCOUNTER_MACHO_BRACE","SOUL_DEW"] as const){
  for(const entry of initialProfile){s.gameData.dexData[entry.id]={...entry.dex,ivs:[...entry.dex.ivs]};if(entry.starter)s.gameData.starterData[entry.id]={...entry.starter};}
  if(size%2===0)for(const id of [1,151,290]){s.gameData.dexData[id].seenAttr=157n;s.gameData.dexData[id].caughtAttr=157n;}
  s.waveCycleOffset=offset;s.gameMode=getGameMode(mode);s.party=original.slice(0,size);s.modifiers=[];s.pokeballCounts[0]=ball;
  p.resetSummonData();p.resetTurnData();p.resetBattleAndWaveData();p.species=registry.getSpecies(pair[0]);p.formIndex=0;p.fusionSpecies=pair[1]?registry.getSpecies(pair[1]):null;p.fusionFormIndex=0;p.abilityIndex=size%2?0:2;p.fusionAbilityIndex=size%2?0:2;p.customPokemonData.ability=-1;p.customPokemonData.passive=-1;p.fusionCustomPokemonData=null;p.level=20;p.exp=getLevelTotalExp(20,p.species.growthRate)+17;p.ivs=[1,5,10,15,20,31];p.nature=0;p.customPokemonData.nature=size>=4?3:-1;p.status=size%2?new Status(StatusEffect.BURN):undefined;p.passive=true;p.pokerus=true;p.pauseEvolutions=false;p.teraType=p.getTypes({includeTeraType:false,bypassSummonData:true,ignoreThirdType:true})[0];p.gender=p.species.malePercent==null?-1:1;p.fusionGender=0;p.shiny=size>=3;p.variant=size>=3?size%3:0;p.fusionShiny=false;p.fusionVariant=0;p.luck=p.shiny?p.variant+1:0;p.fusionLuck=0;p.usedTMs=[MoveId.PROTECT];p.moveset=[new PokemonMove(MoveId.SCRATCH),new PokemonMove(MoveId.HARDEN)];p.moveset[0].ppUsed=2;p.moveset[0].ppUp=1;p.metLevel=5;p.metBiome=ball===0?0:-1;p.metSpecies=SpeciesId.NINCADA;p.metWave=-1;p.stats=Array(6).fill(0);p.hp=undefined;p.calculateStats();p.hp=Math.max(1,p.getMaxHp()-5);
  const f=modifierTypes[held],t=f().withIdFromFunc(f),mod=t.newModifier(p);mod.stackCount=held==="EVIOLITE"?1:2;s.modifiers.push(mod);p.stats=Array(6).fill(0);p.hp=undefined;p.calculateStats();p.hp=Math.max(1,p.getMaxHp()-5);
  Phaser.Math.RND.sow([`shedinja-owner-${rows.length}`]);s.currentBattle.battleSeedState=null;s.currentBattle.randSeedInt(2);
  const before=JSON.parse(JSON.stringify(frame(),(_,v)=>typeof v==="bigint"?v.toString():v)),e=p.getEvolution(),fusion=e instanceof FusionSpeciesFormEvolution;
  expect(e).not.toBeNull();await p.evolve(e,fusion?p.getFusionSpeciesForm():p.getSpeciesForm());await Promise.resolve();await Promise.resolve();
  const shared=s.party.length>size?s.party[s.party.length-1].usedTMs===p.usedTMs:false;
  const after=JSON.parse(JSON.stringify(frame(),(_,v)=>typeof v==="bigint"?v.toString():v)),steps:any[]=[];
  steps.push(await learn(0));if(s.party.length>size)steps.push(await learn(s.party.length-1));
  const extra:any[]=[];
  if(mode===GameModes.CLASSIC&&pair[0]===SpeciesId.NINCADA&&pair[1]===SpeciesId.NINCADA&&size===1&&ball===1&&held==="LEFTOVERS"){
   for(const index of [0,1]){const m=s.party[index],e=m.getEvolution();expect(e).not.toBeNull();expect(e).toBeInstanceOf(FusionSpeciesFormEvolution);await m.evolve(e,m.getFusionSpeciesForm());await Promise.resolve();await Promise.resolve();extra.push({kind:"evolve",index,selected:{species:e.speciesId,fusion:true},after:JSON.parse(JSON.stringify(frame(),(_,v)=>typeof v==="bigint"?v.toString():v))});}
   const step=await learn(2);extra.push({kind:"learn",...step});
   const retained=s.party[0];s.removePokemonFromPlayerParty(retained,false);await s.updateModifiers(true);extra.push({kind:"remove",index:0,after:JSON.parse(JSON.stringify(frame(),(_,v)=>typeof v==="bigint"?v.toString():v))});
   const replacement=original[3];replacement.usedTMs=[];replacement.id=s.party[0].id;s.party.push(replacement);await s.updateModifiers(true);extra.push({kind:"append",after:JSON.parse(JSON.stringify(frame(),(_,v)=>typeof v==="bigint"?v.toString():v))});
   extra.push({kind:"learn",...await learn(s.party.length-1)});
   extra.push({kind:"learn",...await learn(0)});
   expect(retained.usedTMs).toEqual(s.party[0].usedTMs);
  }
  const saved=JSON.parse(JSON.stringify(s.party.map((m:any)=>new PokemonData(m))));s.party=saved.map((raw:any)=>new PokemonData(raw).toPokemon());
  const restored=JSON.parse(JSON.stringify(frame(),(_,v)=>typeof v==="bigint"?v.toString():v)),sharedRestored=s.party.length>size?s.party[s.party.length-1].usedTMs===s.party[0].usedTMs:false;
  const postRestore=await learn(0);
  rows.push({offset,mode,pair,size,ball,held,selected:{species:e.speciesId,fusion},before,after,shared,steps,extra,restored,sharedRestored,postRestore});
 }
 fs.writeFileSync(process.env.SHEDINJA_OUT!,gzipSync(JSON.stringify({version:1,env:{seed:s.seed,waveSeed:s.waveSeed,wave:s.currentBattle.waveIndex,biome:s.arena.biomeId,offset:s.waveCycleOffset,weather:s.arena.weather?.weatherType??0,terrain:s.arena.terrain?.terrainType??0},scope:"actual PlayerPokemon.getEvolution/evolve/special owner/constructor and LearnMovePhase.learnMove, then PokemonData JSON reconstruction; init/loadAssets/updateInfo/modifier-bar, learning animation/audio/UI-text presentation bypassed",rows})+"\n"));
},240000);
