/* Actual changeForm under native fusion and actual Transform state. */
import { speciesDataRegistry } from "#app/globals/global-species-data-registry";
import { getGameMode } from "#app/game-mode";
import { GameModes } from "#enums/game-modes";
import { modifierTypes } from "#data/data-lists";
import { BaseStatModifier,PokemonBaseStatTotalModifier,PokemonBaseStatFlatModifier } from "#modifiers/modifier";
import { SpeciesFormChange } from "#data/pokemon-forms";
import { SpeciesFormChangeAbilityTrigger } from "#data/form-change-triggers";
import { AbilityId } from "#enums/ability-id";
import { BattlerTagType } from "#enums/battler-tag-type";
import { SpeciesId } from "#enums/species-id";
import { MoveId } from "#enums/move-id";
import { Stat } from "#enums/stat";
import { PokemonMove } from "#moves/pokemon-move";
import { GameManager } from "#test/framework/game-manager";
import Phaser from "phaser";
import * as fs from "node:fs";
import { it,vi } from "vitest";
function snapshot(p: any): number[] {
  const sd = p.summonData, form = p.getSpeciesForm(), transformed = p.isTransformed();
  const formIndex = Math.max(0, speciesDataRegistry.getSpecies(form.speciesId).forms.indexOf(form));
  const moves = (ms: any[]) => [ms.length, ...Array.from({ length: 4 }, (_, i) => ms[i]?.moveId ?? 0),
    ...Array.from({ length: 4 }, (_, i) => ms[i]?.ppUsed ?? 0), ...Array.from({ length: 4 }, (_, i) => ms[i]?.ppUp ?? 0),
    ...Array.from({ length: 4 }, (_, i) => ms[i]?.maxPpOverride ?? -1)];
  const types = p.getTypes();
  return [p.species.speciesId, p.formIndex, p.gender, p.id, p.level, p.nature, p.hp, p.status?.effect ?? 0,
    +p.shiny, p.getAbility(true).id, p.getAbility().id, p.passive ? p.getPassiveAbility().id : 0, +p.passive,
    p.customPokemonData.passive, p.fusionSpecies?.speciesId ?? 0, sd.illusion?.species ?? 0,
    +p.isOnField(), +p.switchOutStatus,
    ...p.stats, ...Array.from({ length: 6 }, (_, s) => sd.stats[s] ?? 0),
    ...[Stat.ATK, Stat.DEF, Stat.SPATK, Stat.SPDEF, Stat.SPD, Stat.ACC, Stat.EVA].map(s => p.getStatStage(s)),
    ...moves(p.getMoveset()), sd.types?.length ?? 0, ...Array.from({ length: 4 }, (_, i) => sd.types?.[i] ?? 0),
    sd.addedType ?? 0, +p.isTerastallized, p.teraType, +transformed,
    transformed ? sd.speciesForm.speciesId : 0, transformed ? formIndex : 0, transformed ? sd.gender ?? 0 : 0,
    ...(transformed ? moves(p.moveset) : [0, ...Array(12).fill(0), ...Array(4).fill(0)]),
    +!!p.getTag(BattlerTagType.POWER_TRICK), +!!p.getTag(BattlerTagType.SUBSTITUTE),
    form.speciesId, formIndex, p.getGender(), ...Array.from({ length: 6 }, (_, s) => p.getStat(s, false)),
    types.length, ...Array.from({ length: 5 }, (_, i) => types[i] ?? 0), p.variant, p.getSpriteScale(), +p.isMega(), +p.isMax(), ...p.ivs, p.friendship, +sd.abilitySuppressed, sd.abilitiesApplied.size, ...Array.from({length:4},(_,i)=>[...sd.abilitiesApplied][i]??0), p.waveData.abilitiesApplied.size, ...Array.from({length:4},(_,i)=>[...p.waveData.abilitiesApplied][i]??0), p.tempSummonData.turnCount, p.tempSummonData.waveTurnCount, sd.ability??-1];
}

it("actual form change with fusion and Transform",async()=>{
 const N=Number(process.env.FORMFUSION_N??8192),seed=Number(process.env.FORMFUSION_SEED??777);if(!Number.isSafeInteger(N)||N<64||!process.versions.node.startsWith("22."))throw Error("Node22,N>=64");
 const g=new GameManager(new Phaser.Game({type:Phaser.HEADLESS}));g.override.ability(AbilityId.BALL_FETCH).enemyAbility(AbilityId.BALL_FETCH).moveset(MoveId.SPLASH).enemyMoveset(MoveId.SPLASH).enemySpecies(SpeciesId.MAGIKARP);
 await g.classicMode.startBattle(SpeciesId.BULBASAUR);g.override.ability(AbilityId.NONE).enemyAbility(AbilityId.NONE).moveset([]).enemyMoveset([]);
 const s:any=g.scene,pm:any=s.phaseManager,ps:any[]=[g.field.getPlayerPokemon(),g.field.getEnemyPokemon()];vi.spyOn(pm,"shiftPhase").mockImplementation(()=>{});vi.spyOn(s,"updateFieldScale").mockImplementation(async()=>{});
 for(const p of ps){vi.spyOn(p,"loadAssets").mockImplementation(async()=>{});vi.spyOn(p,"updateInfo").mockImplementation(async()=>{});vi.spyOn(p,"playAnim").mockImplementation(()=>{});vi.spyOn(p,"updateSpritePipelineData").mockImplementation(()=>{});}
 const species=[SpeciesId.DARMANITAN,SpeciesId.MINIOR,SpeciesId.WISHIWASHI,SpeciesId.MORPEKO,SpeciesId.EISCUE,SpeciesId.MIMIKYU,SpeciesId.ZYGARDE,SpeciesId.AEGISLASH,SpeciesId.CASTFORM,SpeciesId.CHERRIM,SpeciesId.CHARIZARD,SpeciesId.GALAR_DARMANITAN];
 const rows=[`FORMFUSION 2 ${N} ${seed}`],rng=(r:string)=>r.replace(/^!rnd,/,"").replaceAll(","," ");
 for(let i=0;i<N;i++){
  const state=Math.floor(i/2)%32,p=ps[i%2],other=ps[1-i%2],sp=speciesDataRegistry.getSpecies(species[Math.floor(i/64)%species.length]),to=Math.floor(state/3)%sp.forms.length;
  for(const q of ps){q.resetSummonData();q.resetTurnData();q.resetBattleAndWaveData();q.fusionSpecies=null;q.fusionCustomPokemonData=null;q.fusionAbilityIndex=0;q.fusionFormIndex=0;q.formIndex=0;q.abilityIndex=0;q.customPokemonData.ability=-1;q.passive=false;q.customPokemonData.passive=AbilityId.NONE;q.switchOutStatus=false;q.status=null;q.isTerastallized=false;q.teraType=4;q.stellarTypesBoosted=[];q.moveset=[new PokemonMove(MoveId.SPLASH),new PokemonMove(MoveId.TACKLE)];}
  p.species=sp;p.formIndex=state%sp.forms.length;p.abilityIndex=state%3;other.species=speciesDataRegistry.getSpecies(SpeciesId.MAGIKARP);other.customPokemonData.ability=AbilityId.BALL_FETCH;
  if(state%4>=2){p.fusionSpecies=speciesDataRegistry.getSpecies(state%2?SpeciesId.PIKACHU:SpeciesId.MAGIKARP);p.fusionFormIndex=0;p.fusionAbilityIndex=0;}
  s.modifiers=[];s.enemyModifiers=[];const mods:any[]=[];s.gameMode=getGameMode(Math.floor(i/768)%2?GameModes.SPLICED_ENDLESS:GameModes.CLASSIC);
  const items=Math.floor(i/64)%8,owner=items===7?other.id:p.id,mt=modifierTypes.LEFTOVERS();
  if(items%4>=1)mods.push(new PokemonBaseStatTotalModifier(mt,owner,items%2?10:-15,1+i%2));
  if(items%4>=2)mods.push(new PokemonBaseStatFlatModifier(mt,owner,1+i%2));
  if(items%4===3)mods.push(new PokemonBaseStatTotalModifier(mt,owner,-15,1));
  if(items>=4)mods.push(new BaseStatModifier(mt,owner,i%6,1+i%3));
  if(p.isPlayer())s.modifiers=mods;else s.enemyModifiers=mods;
  for(const q of ps){q.level=30+i%21;q.nature=i%25;q.ivs=Array(6).fill(i%32);q.stats=Array(6).fill(0);q.hp=0;q.calculateStats();q.hp=q.getMaxHp();q.generateName();}
  s.arena.tags=[];s.arena.weather=null;s.arena.terrain=null;pm.clearAllPhases();pm.phaseQueue.deferredActive=false;
  if(state%4===1||state%4===3){const ph=pm.create("PokemonTransformPhase",p.getBattlerIndex(),other.getBattlerIndex(),false);pm.currentPhase=ph;ph.start();await Promise.resolve();await Promise.resolve();await Promise.resolve();if(!p.isTransformed())throw Error("actual Transform refused");}
  p.hp=state>=16?0:Math.max(1,Math.floor(p.getMaxHp()/2));p.isTerastallized=!!(state%5===0);p.teraType=4;p.stellarTypesBoosted=p.isTerastallized?[1,2]:[];
  Phaser.Math.RND.sow([`form-fusion-${seed}-${i}`]);s.currentBattle.battleSeedState=null;s.currentBattle.randSeedInt(2);
  rows.push(`C ${i} ${p.getBattlerIndex()} ${to} ${p.abilityIndex} ${p.customPokemonData.ability} ${p.fusionFormIndex} ${p.fusionAbilityIndex} ${p.fusionCustomPokemonData?.ability??-1}`,`R ${rng(Phaser.Math.RND.state())}`,`B ${rng(s.currentBattle.battleSeedState)}`,`G ${s.gameMode.modeId} ${mods.length}`);
  for(const m of mods)rows.push(`M ${m.constructor.name} ${m.pokemonId} ${m.stackCount} ${m.virtualStackCount??0} ${m.statModifier??0} ${m.stat??0} ${m.getMaxHeldItemCount(p)} ${+m.isTransferable}`);
  rows.push(`I ${snapshot(p).join(" ")}`);
  await p.changeForm(new SpeciesFormChange({speciesId:sp.speciesId,preFormKey:sp.forms[p.formIndex].formKey,evoFormKey:sp.forms[to].formKey,trigger:new SpeciesFormChangeAbilityTrigger(),quiet:true}));
  rows.push(`O ${snapshot(p).join(" ")}`,`X ${p.abilityIndex} ${p.fusionAbilityIndex}`,`RO ${rng(Phaser.Math.RND.state())}`,`BO ${rng(s.currentBattle.battleSeedState)}`,"E");
 }
 rows.push(`FORMFUSION_COMPLETE ${N}`);fs.writeFileSync(process.env.FORMFUSION_OUT??"/tmp/form-fusion.txt",rows.join("\n")+"\n");
},600000);
