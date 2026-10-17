import { BattleScene } from "#app/battle-scene";
/* Actual registered weather-form scene triggers and held Quiet phases. */
import { speciesDataRegistry } from "#app/globals/global-species-data-registry";
import { SpeciesFormChangeWeatherTrigger,SpeciesFormChangeRevertWeatherFormTrigger,SpeciesFormChangeActiveTrigger } from "#data/form-change-triggers";
import { Weather } from "#data/weather";
import { WeatherType } from "#enums/weather-type";
import { AbilityId } from "#enums/ability-id";
import { BattlerTagType } from "#enums/battler-tag-type";
import { SpeciesId } from "#enums/species-id";
import { MoveUseMode } from "#enums/move-use-mode";
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

it("actual Xerneas registered active quiet transitions",async()=>{
 const N=Number(process.env.XERNEAS_N??8192),seed=Number(process.env.XERNEAS_SEED??777);if(!Number.isSafeInteger(N)||N<8192||!process.versions.node.startsWith("22."))throw Error("Node22,N>=8192");
 const native=BattleScene.prototype.randBattleSeedInt;
 const game=new GameManager(new Phaser.Game({type:Phaser.HEADLESS}));game.override.ability(AbilityId.BALL_FETCH).enemyAbility(AbilityId.BALL_FETCH).moveset(MoveId.SPLASH).enemyMoveset(MoveId.SPLASH).enemySpecies(SpeciesId.MAGIKARP);
 await game.classicMode.startBattle(SpeciesId.BULBASAUR);BattleScene.prototype.randBattleSeedInt=native;game.override.ability(AbilityId.NONE).enemyAbility(AbilityId.NONE).moveset([]).enemyMoveset([]);
 const s:any=game.scene,pm:any=s.phaseManager,ps:any[]=[game.field.getPlayerPokemon(),game.field.getEnemyPokemon()];
 vi.spyOn(s,"updateFieldScale").mockImplementation(async()=>{});vi.spyOn(pm,"shiftPhase").mockImplementation(()=>{});
 for(const p of ps){vi.spyOn(p,"loadAssets").mockImplementation(async()=>{});vi.spyOn(p,"updateInfo").mockImplementation(async()=>{});vi.spyOn(p,"updateSpritePipelineData").mockImplementation(()=>{});}
 let texts:string[]=[];vi.spyOn(s.ui,"showText").mockImplementation((t:any,_d:any,cb:any)=>{texts.push(t);cb?.();});
 const rng=(r:string)=>r.replace(/^!rnd,/,"").replaceAll(","," ");const rows=[`XERNEAS 1 ${N} ${seed}`];
 for(let i=0;i<N;i++){
  const state=Math.floor(i/2)%128,p=ps[i%2],kind=state%3,variant=(state>>4)%4,activity=Math.floor(i/256)%4,delayed=!!(Math.floor(i/1024)%2),cmd=[MoveId.NONE,MoveId.TACKLE,MoveId.KINGS_SHIELD,MoveId.SPLASH][(state>>1)%4];
  p.resetSummonData();p.resetTurnData();p.resetBattleAndWaveData();const sp=SpeciesId.XERNEAS,native=AbilityId.FAIRY_AURA;p.species=speciesDataRegistry.getSpecies(sp);p.formIndex=(state>>3)%p.species.forms.length;p.gender=-1;p.abilityIndex=0;p.fusionSpecies=null;p.customPokemonData.ability=variant===1||variant===2?AbilityId.BALL_FETCH:native;p.customPokemonData.passive=variant===2?native:AbilityId.NONE;p.passive=variant===2;
  p.summonData.ability=variant===3?AbilityId.BALL_FETCH:undefined;
  if(state%8===6)p.summonData.ability=AbilityId.NONE;else if(state%8===7)p.summonData.ability=native;p.level=30+(i%21);p.calculateStats();p.hp=state>=64?0:p.getMaxHp();p.switchOutStatus=activity%2===1;p.status=null;p.moveset=[new PokemonMove(MoveId.TACKLE),new PokemonMove(MoveId.KINGS_SHIELD),new PokemonMove(MoveId.SPLASH)];p.generateName();p.visible=false;p.isTerastallized=!!(i%3===0);p.teraType=4;p.stellarTypesBoosted=p.isTerastallized?[1,2]:[];
  if(activity>=2)s.field.remove(p);else if(!s.field.getIndex(p)||!p.isOnField()){p.switchOutStatus=false;s.field.add(p);p.switchOutStatus=activity%2===1;}
  p.summonData.abilitySuppressed=!!(i%5===0);
  const other=ps[1-i%2],cloud=!!(Math.floor(i/2048)%2);other.resetSummonData();other.species=speciesDataRegistry.getSpecies(SpeciesId.MAGIKARP);other.formIndex=0;other.customPokemonData.ability=cloud?AbilityId.CLOUD_NINE:AbilityId.BALL_FETCH;other.passive=false;other.hp=100;other.switchOutStatus=false;
  s.modifiers=[];s.enemyModifiers=[];s.arena.tags=[];const weather=(Math.floor(i/512)%10);s.arena.weather=weather?new Weather(weather):null;s.arena.terrain=null;
  const b=p.getBattlerIndex();s.currentBattle.turnCommands=Array(4).fill(null);if(cmd!==MoveId.NONE)s.currentBattle.turnCommands[b]={command:0,move:{move:cmd}};
  pm.clearAllPhases();pm.phaseQueue.deferredActive=false;texts=[];
  pm.pushNew("MessagePhase","anchor");pm.currentPhase=pm.create("MovePhase",p,[p.isPlayer()?2:0],new PokemonMove(cmd===MoveId.TACKLE?MoveId.KINGS_SHIELD:MoveId.TACKLE),MoveUseMode.NORMAL);
  Phaser.Math.RND.sow([`xerneas-active-${seed}-${i}`]);s.currentBattle.battleSeedState=null;s.currentBattle.randSeedInt(2);
  rows.push(`C ${i} ${i%2?2:0} ${kind} ${+delayed} ${cmd} ${p.customPokemonData.ability} ${p.customPokemonData.passive} ${+p.passive} ${+p.summonData.abilitySuppressed}`,
   `W ${p.species.speciesId} ${weather} ${+cloud}`,`R ${rng(Phaser.Math.RND.state())}`,`B ${rng(s.currentBattle.battleSeedState)}`,`I ${snapshot(p).join(" ")}`);
  s.triggerPokemonFormChange(p,[SpeciesFormChangeWeatherTrigger,SpeciesFormChangeRevertWeatherFormTrigger,SpeciesFormChangeActiveTrigger][kind],delayed);
  const q=pm.phaseQueue.levels.flat();rows.push(`Q ${q.length} ${pm.phaseQueue.levels.length} ${pm.phaseQueue.currentLevel} ${+pm.phaseQueue.deferredActive}`);
  pm.phaseQueue.levels.forEach((level:any[],l:number)=>level.forEach((ph:any,k:number)=>rows.push(`P ${l} ${k} ${ph.phaseName} ${ph.phaseName==="QuietFormChangePhase"?p.species.forms.findIndex((f:any)=>f.formKey===ph.formChange.formKey):-1}`)));rows.push(`A ${snapshot(p).join(" ")}`);
  for(const ph of q.filter((ph:any)=>ph.phaseName==="QuietFormChangePhase")){pm.currentPhase=ph;await ph.start();}
  rows.push(`O ${snapshot(p).join(" ")}`,`RO ${rng(Phaser.Math.RND.state())}`,`BO ${rng(s.currentBattle.battleSeedState)}`,`F ${p.turnData.formChangeAbilitiesApplied.size} ${Array.from({length:16},(_,k)=>[...p.turnData.formChangeAbilitiesApplied][k]??0).join(" ")}`,`T ${texts.length?Buffer.from(texts.join("\n")).toString("hex"):"-"}`,"E");
 }
 rows.push(`XERNEAS_COMPLETE ${N}`);fs.writeFileSync(process.env.XERNEAS_OUT??"/tmp/weather-form.txt",rows.join("\n")+"\n");
},600000);
