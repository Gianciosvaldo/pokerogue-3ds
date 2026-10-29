import { ModifierData } from "#system/modifier-data";
import * as modifierClasses from "#modifiers/modifier";
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
it("native manual Eviolite clone owner",async()=>{
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
 const frame=()=>({profile:profile(),party:s.party.map((m:any)=>({id:m.id,species:m.species.speciesId,form:m.formIndex,fusion:m.fusionSpecies?.speciesId??0,fusionForm:m.fusionFormIndex,abilityIndex:m.abilityIndex,fusionAbilityIndex:m.fusionAbilityIndex,ability:m.getAbility().id,gender:m.gender,fusionGender:m.fusionGender,level:m.level,exp:m.exp,nature:m.nature,effectiveNature:m.getNature(),customAbility:m.customPokemonData.ability,customPassive:m.customPokemonData.passive,customNature:m.customPokemonData.nature,ivs:m.ivs,hp:m.hp,stats:m.stats,status:m.status,friendship:m.friendship,passive:m.passive,pokerus:m.pokerus,ball:m.pokeball,shiny:m.shiny,variant:m.variant,luck:m.luck,fusionShiny:m.fusionShiny,fusionVariant:m.fusionVariant,fusionLuck:m.fusionLuck,fusionTera:m.fusionTeraType,tera:m.teraType,met:[m.metLevel,m.metBiome,m.metSpecies,m.metWave],used:[...m.usedTMs],moves:m.moveset.map((v:any)=>({move:v.moveId,used:v.ppUsed,up:v.ppUp,max:v.maxPpOverride??-1}))})),stock:s.modifiers.map((m:any)=>({key:m.type.id,owner:m.pokemonId??-1,stack:m.stackCount,virtual:m.virtualStackCount,locked:!m.isTransferable,cls:m.constructor.name,tier:m.type.tier??-1})),balls:{...s.pokeballCounts},global:Phaser.Math.RND.state(),battle:s.currentBattle.battleSeedState});
 expect((LearnMovePhase.prototype as any).learnMove).toBe(nativeLearn);const rows:any[]=[],initialProfile=profile().map(e=>({...e,dex:{...e.dex,ivs:[...e.dex.ivs]},starter:e.starter?{...e.starter}:null}));

 for(const species of [SpeciesId.BULBASAUR,SpeciesId.VENUSAUR])for(const sourceClone of [false,true])for(const targetShape of [0,1,2])for(const tier of [-1,0,1,2,3,4]){
  s.gameMode=getGameMode(GameModes.CLASSIC);s.waveCycleOffset=20;s.party=[original[0],original[1]];s.modifiers=[];
  for(let i=0;i<2;i++){const m=s.party[i];m.resetSummonData();m.resetTurnData();m.species=registry.getSpecies(i?species:SpeciesId.BULBASAUR);m.formIndex=0;m.fusionSpecies=null;m.abilityIndex=0;m.customPokemonData.ability=-1;m.customPokemonData.passive=-1;m.passive=false;m.nature=0;m.customPokemonData.nature=-1;m.shiny=false;m.variant=0;m.luck=0;m.teraType=m.getTypes({includeTeraType:false,bypassSummonData:true,ignoreThirdType:true})[0];m.ivs=[31,31,31,31,31,31];m.level=20;m.exp=getLevelTotalExp(20,m.species.growthRate);m.hp=undefined;m.stats=Array(6).fill(0);m.calculateStats();m.hp=m.getMaxHp();m.usedTMs=[];m.status=undefined;}
  const make=(index:number,clone:boolean)=>{const f=modifierTypes.EVIOLITE,t=f().withIdFromFunc(f),original=t.newModifier(s.party[index]);return clone?original.clone():original;};
  s.phaseManager.clearAllPhases();const source=make(0,sourceClone);source.type.tier=tier<0?undefined:tier;s.modifiers.push(source);if(targetShape)s.modifiers.push(make(1,targetShape===2));await s.updateModifiers(true);
  Phaser.Math.RND.sow([`manual-eviolite-${rows.length}`]);s.currentBattle.battleSeedState=null;s.currentBattle.randSeedInt(2);
  const snap=()=>JSON.parse(JSON.stringify(frame(),(_,v)=>typeof v==="bigint"?v.toString():v));
  const before=snap(),can=s.canTransferHeldItemModifier(source,s.party[1],1),result=s.tryTransferHeldItemModifier(source,s.party[1],false,1,true,undefined,false);
  await s.updateModifiers(true);const after=snap();
  const messages=s.phaseManager.phaseQueue.levels.flat().filter((p:any)=>p.phaseName==="MessagePhase").map((p:any)=>p.text);const savedParty=JSON.parse(JSON.stringify(s.party.map((m:any)=>new PokemonData(m)))),savedItems=JSON.parse(JSON.stringify(s.modifiers.map((m:any)=>new ModifierData(m,true))));
  s.party=savedParty.map((v:any)=>new PokemonData(v).toPokemon());s.modifiers=savedItems.map((v:any)=>new ModifierData(v,true).toModifier((modifierClasses as any)[v.className]));expect(s.modifiers.every((v:any)=>!!v)).toBe(true);
  const restored=snap();rows.push({species,sourceClone,targetShape,tier,can,result,before,after,messages,restored});
 }
 fs.writeFileSync(process.env.MANUAL_EVIOLITE_OUT!,gzipSync(JSON.stringify({version:1,locales:Object.fromEntries(["battle","modifier-type","pokeball"].map(name=>[name,fs.readFileSync(`locales/en/${name}.json`,"utf8")])),env:{seed:s.seed,waveSeed:s.waveSeed,wave:s.currentBattle.waveIndex,biome:s.arena.biomeId,offset:s.waveCycleOffset,weather:0,terrain:0},scope:"actual scene canTransferHeldItemModifier and tryTransferHeldItemModifier; presentation bypassed",rows})+"\n"));
},240000);
