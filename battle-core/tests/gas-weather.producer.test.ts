/* Actual Neutralizing Gas arena lifecycle on four source actors. API proof, not complete turns. */
import { audioManager } from "#app/global-audio-manager";
import { speciesDataRegistry } from "#app/globals/global-species-data-registry";
import { AbilityId } from "#enums/ability-id";
import { BattlerTagType } from "#enums/battler-tag-type";
import { MoveId } from "#enums/move-id";
import { SpeciesId } from "#enums/species-id";
import { Stat } from "#enums/stat";
import { PokemonMove } from "#moves/pokemon-move";
import { applyAbAttrs } from "#abilities/apply-ab-attrs";
import { SuppressAbilitiesTag } from "#data/arena-tag";
import { ArenaTagType } from "#enums/arena-tag-type";
import { Weather } from "#data/weather";
import { WeatherType } from "#enums/weather-type";
import { ArenaTagSide } from "#enums/arena-tag-side";
import { GameManager } from "#test/framework/game-manager";
import Phaser from "phaser";
import * as fs from "node:fs";
import { it, vi } from "vitest";


const N=Number(process.env.GASLIFE_N??2000),SEED=Number(process.env.GASLIFE_SEED??777);
let seed=SEED;const rnd=(n:number)=>{seed=(Math.imul(seed,1103515245)+12345)>>>0;return(seed>>>8)%n;};
const stateRng=(s:string)=>s.replace(/^!rnd,/,"").replaceAll(","," ");
const rows=[`GASLIFE 1 ${N} ${SEED}`];
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

it("actual Neutralizing Gas arena methods",async()=>{
 if(!process.versions.node.startsWith("22.")||!Number.isSafeInteger(N)||N<192)throw Error("Node22 and N>=192 required");
 const game=new GameManager(new Phaser.Game({type:Phaser.HEADLESS}));
 game.override.battleStyle("double").ability(AbilityId.BALL_FETCH).enemyAbility(AbilityId.BALL_FETCH)
  .passiveAbility(AbilityId.NONE).enemyPassiveAbility(AbilityId.NONE).enemySpecies(SpeciesId.MAGIKARP)
  .moveset(MoveId.SPLASH).enemyMoveset(MoveId.SPLASH).disableTrainerWaves().mysteryEncounterChance(0);
 await game.classicMode.startBattle(SpeciesId.RATTATA,SpeciesId.RATTATA);
 game.override.ability(AbilityId.NONE).enemyAbility(AbilityId.NONE).moveset([]).enemyMoveset([]);
 const scene:any=game.scene,pm:any=scene.phaseManager,battle:any=scene.currentBattle,slots:any[]=scene.getField();
 if(slots.length!==4||slots.some(p=>!p))throw Error("natural doubles required");

 for(const p of slots){
  vi.spyOn(p,"loadAssets").mockImplementation(()=>Promise.resolve());vi.spyOn(p,"updateInfo").mockImplementation(()=>Promise.resolve());
  vi.spyOn(p,"playAnim").mockImplementation(()=>{});vi.spyOn(p,"hideInfo").mockImplementation(()=>Promise.resolve());
  vi.spyOn(p,"faintCry").mockImplementation((cb:any)=>cb());
 }
 vi.spyOn(audioManager,"playSound").mockImplementation(()=>{});
 scene.tweens.add=(config:any)=>{config.onComplete?.();return{};};
 const counts=Array(32).fill(0);
 for(let i=0;i<N;i++){
  const state=i%32,kind=Math.floor(i/32)%6,holder=state%2?2:0,victim=holder<2?2:0;counts[state]++;
  for(const p of slots){
   p.resetSummonData();p.resetTurnData();p.resetBattleAndWaveData();p.species=speciesDataRegistry.getSpecies(SpeciesId.RATTATA);p.formIndex=0;p.fusionSpecies=null;p.fusionCustomPokemonData=null;p.fusionFormIndex=0;p.fusionAbilityIndex=0;
   p.customPokemonData.ability=AbilityId.BALL_FETCH;p.customPokemonData.passive=AbilityId.NONE;p.passive=false;p.level=30+rnd(21);p.calculateStats();p.hp=p.getMaxHp();p.status=null;p.friendship=70;p.switchOutStatus=false;p.generateName();
   p.moveset=[new PokemonMove(MoveId.SPLASH)];if(!p.isOnField())scene.field.add(p);
  }
  const user=slots[holder];
  user.customPokemonData.ability=AbilityId.NEUTRALIZING_GAS;
  if(state>=8&&state<16){user.customPokemonData.ability=AbilityId.BALL_FETCH;user.passive=true;user.customPokemonData.passive=AbilityId.NEUTRALIZING_GAS;}
  slots[1].customPokemonData.ability=AbilityId.INTREPID_SWORD;
  if(state%8>=4){slots[1].passive=true;slots[1].customPokemonData.passive=AbilityId.INTIMIDATE;}
  if(state%4>=2)slots[victim].customPokemonData.ability=AbilityId.NEUTRALIZING_GAS;
  if(state>=16&&state<20)slots[1].summonData.abilitySuppressed=true;
  if(state>=20&&state<24){slots[1].hp=0;}
  if(state>=24&&state<28){user.hp=0;}
  if(state>=28){slots[1].switchOutStatus=true;scene.field.remove(slots[1]);}
  scene.modifiers=[];scene.enemyModifiers=[];scene.arena.weather=null;scene.arena.terrain=null;scene.arena.tags=[];
  slots[3].customPokemonData.ability=state%2?AbilityId.DESOLATE_LAND:AbilityId.PRIMORDIAL_SEA;
  slots[1].customPokemonData.ability=state%2?AbilityId.DROUGHT:AbilityId.DRIZZLE;
  scene.arena.weather=new Weather(state%2?WeatherType.HARSH_SUN:WeatherType.HEAVY_RAIN,0);
  scene.arena.playerFaints=0;battle.enemyFaints=0;battle.playerFaintsHistory=[];battle.enemyFaintsHistory=[];
  battle.playerParticipantIds=new Set([slots[0].id,slots[1].id]);battle.postBattleLoot=[];battle.battleScore=0;
  pm.clearAllPhases();pm.phaseQueue.deferredActive=false;pm.dynamicQueueManager.clearLastTurnOrder();scene.abilityBar.setVisible(false);
  battle.battleSeed=`battlebond-${SEED}-${i}`;battle.battleSeedState=null;battle.turn=1;scene.waveSeed=`battlebond-wave-${SEED}`;
  Phaser.Math.RND.sow([`battlebond-global-${SEED}-${i}`]);battle.randSeedInt(2);
  const initial=kind===0?0:kind===2?1:kind===3?2:1+state%3;
  const sid=state%8===7?undefined:user.id;
  const tag:any=initial?new SuppressAbilitiesTag(sid):null;
  if(tag){tag.sourceCount=initial;scene.arena.tags=[tag];}
  const active=()=>slots.flatMap(p=>[+p.canApplyAbility(false),+p.canApplyAbility(true)]);
  rows.push(`C ${i} ${kind} ${holder} ${victim} ${scene.getMaxExpLevel()}`,`G ${initial} ${sid??-1} ${state%2}`,`W ${scene.arena.weather.weatherType} ${scene.arena.weather.turnsLeft} ${scene.arena.weather.maxDuration}`,`AC ${active().join(" ")}`,`R ${stateRng(Phaser.Math.RND.state())}`,`B ${stateRng(battle.battleSeedState)}`,...slots.map((p,b)=>`I ${b} ${snapshot(p).join(" ")}`));
  if(kind===0)scene.arena.addTag(ArenaTagType.NEUTRALIZING_GAS,0,undefined,sid,ArenaTagSide.BOTH,!!(state%2));
  else if(kind===1)scene.arena.addTag(ArenaTagType.NEUTRALIZING_GAS,0,undefined,state%8===7?undefined:slots[victim].id);
  else if(kind===2||kind===3)tag.onSourceLeave(scene.arena);
  else if(kind===4)scene.arena.removeTagOnSide(ArenaTagType.NEUTRALIZING_GAS,ArenaTagSide.BOTH,!!(state%2));
  else scene.arena.removeAllTags();
  const remaining:any=scene.arena.getTag(ArenaTagType.NEUTRALIZING_GAS);
  rows.push(`GA ${scene.arena.tags.length} ${remaining?.sourceCount??0} ${+(remaining?.beingRemoved??false)}`,`ACO ${active().join(" ")}`);
  const weather=scene.arena.weather,terrain=scene.arena.terrain,participants=[...battle.playerParticipantIds].sort((a:any,b:any)=>a-b);
  rows.push(`WORLD ${weather?.weatherType??0} ${weather?.turnsLeft??0} ${weather?.maxDuration??0} ${terrain?.terrainType??0} ${terrain?.turnsLeft??0} ${terrain?.maxDuration??0} ${scene.arena.playerFaints} ${battle.enemyFaints} ${battle.battleScore} ${battle.postBattleLoot.length} ${+scene.abilityBar.isVisible()} ${participants.length} ${participants[0]??0} ${participants[1]??0}`);
  rows.push(...slots.map((p,b)=>`O ${b} ${snapshot(p).join(" ")}`),`RO ${stateRng(Phaser.Math.RND.state())}`,`BO ${stateRng(battle.battleSeedState)}`);
  const queue:string[]=[];
  const emit=(scope:string,level:number,j:number,p:any)=>{
   const name=p.phaseName==="DynamicPhaseMarker"?`Marker:${p.phaseType}`:p.phaseName;
   const text=p.phaseName==="MessagePhase"?Buffer.from(p.text,"utf8").toString("hex"):"-";
   const stages=p.phaseName==="StatStageChangePhase"?p.options.changes.map((x:any)=>`${x.stat}:${x.stages}`).join(","):p.phaseName==="QuietFormChangePhase"?`F:${p.pokemon.species.forms.findIndex((f:any)=>f.formKey===p.formChange.formKey)}`:"-";
   queue.push(`Q ${scope} ${level} ${j} ${name} ${p.phaseName==="QuietFormChangePhase"?p.pokemon.getBattlerIndex():p.battlerIndex??-1} ${p.phaseName==="ShowAbilityPhase"?+p.passive:-1} ${text} ${stages}`);
  };
  pm.phaseQueue.levels.forEach((lv:any[],level:number)=>lv.forEach((p:any,j:number)=>emit("L",level,j,p)));
  [...pm.dynamicQueueManager.dynamicPhaseMap.entries()].filter(([,q]:any)=>q.queue.length).sort(([a]:any,[b]:any)=>a.localeCompare(b)).forEach(([,q]:any,k:number)=>q.queue.forEach((p:any,j:number)=>emit("D",k,j,p)));
  rows.push(`QCOUNT ${queue.length} ${pm.phaseQueue.levels.length} ${+pm.phaseQueue.deferredActive}`,...queue);
  for(const p of slots)p.resetSummonData();
  rows.push(...slots.map((p,b)=>`Z ${b} ${snapshot(p).join(" ")}`),"E");
 }
 rows.push(`GASLIFE_COMPLETE ${N}`);fs.writeFileSync(process.env.GASLIFE_OUT??"/tmp/battlebond.txt",rows.join("\n")+"\n");console.log(`GASLIFE cases=${N} states=${counts.join("/")}`);
},600000);
