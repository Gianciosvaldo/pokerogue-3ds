import { BattleScene } from "#app/battle-scene";
import { allMoves } from "#data/data-lists";
import { Weather } from "#data/weather";
import { WeatherType } from "#enums/weather-type";
/* Actual suppression leaves, condition and loss/weather callbacks; no owning turn claim. */
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
import { ArenaTagSide } from "#enums/arena-tag-side";
import { GameManager } from "#test/framework/game-manager";
import Phaser from "phaser";
import * as fs from "node:fs";
import { it, vi } from "vitest";


const N=Number(process.env.SUPPRESSION_N??16384),SEED=Number(process.env.SUPPRESSION_SEED??777);
let seed=SEED;const rnd=(n:number)=>{seed=(Math.imul(seed,1103515245)+12345)>>>0;return(seed>>>8)%n;};
const stateRng=(s:string)=>s.replace(/^!rnd,/,"").replaceAll(","," ");
const rows=[`SUPPRESSION 1 ${N} ${SEED}`];
function snapshot(p: any): number[] {
  const sd = p.summonData, form = p.getSpeciesForm(), transformed = p.isTransformed();
  const formIndex = Math.max(0, speciesDataRegistry.getSpecies(form.speciesId).forms.indexOf(form));
  const moves = (ms: any[]) => [ms.length, ...Array.from({ length: 4 }, (_, i) => ms[i]?.moveId ?? 0),
    ...Array.from({ length: 4 }, (_, i) => ms[i]?.ppUsed ?? 0), ...Array.from({ length: 4 }, (_, i) => ms[i]?.ppUp ?? 0),
    ...Array.from({ length: 4 }, (_, i) => ms[i]?.maxPpOverride ?? -1)];
  const types = p.getTypes();
  return [p.species.speciesId, p.formIndex, p.gender, p.id, p.level, p.nature, p.hp, p.status?.effect ?? 0,
    +p.shiny, p.getAbility(true).id, p.getAbility().id, p.hasPassive() ? p.getPassiveAbility().id : 0, +p.hasPassive(),
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

it("actual suppression attributes and native loss callbacks", async () => {
 if (!process.versions.node.startsWith("22.") || !Number.isSafeInteger(N) || N < 16384) throw Error("Node22 and N>=16384 required");
 const native = BattleScene.prototype.randBattleSeedInt;
 const game = new GameManager(new Phaser.Game({type:Phaser.HEADLESS}));
 game.override.battleStyle("double").ability(AbilityId.BALL_FETCH).enemyAbility(AbilityId.BALL_FETCH)
  .passiveAbility(AbilityId.NONE).enemyPassiveAbility(AbilityId.NONE).enemySpecies(SpeciesId.MAGIKARP)
  .moveset(MoveId.SPLASH).enemyMoveset(MoveId.SPLASH).disableTrainerWaves().mysteryEncounterChance(0);
 await game.classicMode.startBattle(SpeciesId.RATTATA,SpeciesId.RATTATA);
 BattleScene.prototype.randBattleSeedInt = native;
 game.override.ability(AbilityId.NONE).enemyAbility(AbilityId.NONE).moveset([]).enemyMoveset([]);
 const scene:any=game.scene, pm:any=scene.phaseManager, battle:any=scene.currentBattle;
 const slots:any[]=[...scene.getPlayerField(),...scene.getEnemyField()];
 if(slots.length!==4 || slots.some(p=>!p))throw Error("natural doubles required");
 vi.spyOn(pm,"shiftPhase").mockImplementation(()=>{});
 for(const p of slots){vi.spyOn(p,"loadAssets").mockImplementation(async()=>{});vi.spyOn(p,"updateInfo").mockImplementation(async()=>{});}
 const base=allMoves[MoveId.GASTRO_ACID].getAttrs("SuppressAbilitiesAttr")[0];
 // The child is not exported in the string-attribute registry at this pin.
 const core=allMoves[MoveId.CORE_ENFORCER].attrs.find((a:any)=>a.constructor.name==="SuppressAbilitiesIfActedAttr");
 if(!base||!core)throw Error("native suppression attributes missing");
 const primaries=[AbilityId.NONE,AbilityId.BALL_FETCH,AbilityId.MULTITYPE,AbilityId.COMATOSE,AbilityId.FORECAST,AbilityId.FLOWER_GIFT,AbilityId.PRIMORDIAL_SEA,AbilityId.DESOLATE_LAND,AbilityId.DELTA_STREAM,AbilityId.NEUTRALIZING_GAS,AbilityId.LEVITATE];
 const passives=[AbilityId.NONE,AbilityId.BALL_FETCH,AbilityId.MULTITYPE,AbilityId.FORECAST,AbilityId.FLOWER_GIFT,AbilityId.PRIMORDIAL_SEA,AbilityId.DESOLATE_LAND,AbilityId.DELTA_STREAM,AbilityId.NEUTRALIZING_GAS,AbilityId.INTREPID_SWORD,AbilityId.INTIMIDATE];
 for(let i=0;i<N;i++){
  const mode=i%3, primary=primaries[Math.floor(i/3)%11], passive=passives[Math.floor(i/33)%11];
  const target=Math.floor(i/363)%4, holder=Math.floor(i/1452)%4;
  for(const p of slots){
   p.resetSummonData();p.resetTurnData();p.resetBattleAndWaveData();
   p.species=speciesDataRegistry.getSpecies(SpeciesId.RATTATA);p.formIndex=0;p.fusionSpecies=null;p.fusionCustomPokemonData=null;
   p.customPokemonData.ability=AbilityId.BALL_FETCH;p.customPokemonData.passive=AbilityId.NONE;p.passive=false;
   p.level=30+rnd(21);p.nature=rnd(25);p.ivs=Array.from({length:6},()=>rnd(32));p.status=null;
   p.friendship=70;p.switchOutStatus=false;p.moveset=[new PokemonMove(MoveId.SPLASH)];
  }
  slots[1].species=speciesDataRegistry.getSpecies(SpeciesId.CASTFORM);slots[1].formIndex=1;slots[1].customPokemonData.ability=AbilityId.FORECAST;
  slots[3].species=speciesDataRegistry.getSpecies(SpeciesId.CHERRIM);slots[3].formIndex=1;slots[3].customPokemonData.ability=AbilityId.FLOWER_GIFT;
  const p=slots[target];
  p.species=speciesDataRegistry.getSpecies(primary===AbilityId.FORECAST?SpeciesId.CASTFORM:primary===AbilityId.FLOWER_GIFT?SpeciesId.CHERRIM:SpeciesId.RATTATA);
  p.formIndex=primary===AbilityId.FORECAST||primary===AbilityId.FLOWER_GIFT?1:0;
  p.customPokemonData.ability=primary;p.customPokemonData.passive=passive;p.passive=Math.floor(i/726)%2===1;
  p.turnData.acted=Math.floor(i/2904)%2===1;p.summonData.abilitySuppressed=Math.floor(i/5808)%2===1;
  for(const q of slots){q.calculateStats();q.hp=q.getMaxHp();q.generateName();}
  if(Math.floor(i/1452)%2)p.hp=0;
  scene.modifiers=[];scene.enemyModifiers=[];scene.arena.tags=[];scene.arena.terrain=null;
  const weatherType=[WeatherType.NONE,WeatherType.SUNNY,WeatherType.RAIN,WeatherType.HEAVY_RAIN,WeatherType.HARSH_SUN,WeatherType.STRONG_WINDS][Math.floor(i/121)%6];
  scene.arena.weather=weatherType===WeatherType.NONE?null:new Weather(weatherType);
  const initial=Math.floor(i/97)%4, sid=i%17===0?undefined:slots[(target+Math.floor(i/194)%2)%4].id;
  if(initial){const tag:any=new SuppressAbilitiesTag(sid);tag.sourceCount=initial;scene.arena.tags=[tag];}
  scene.arena.playerFaints=0;battle.enemyFaints=0;battle.playerFaintsHistory=[];battle.enemyFaintsHistory=[];
  battle.playerParticipantIds=new Set([slots[0].id,slots[1].id]);battle.postBattleLoot=[];battle.battleScore=0;
  pm.clearAllPhases();pm.phaseQueue.deferredActive=false;pm.dynamicQueueManager.clearLastTurnOrder();scene.abilityBar.setVisible(false);
  battle.battleSeed=`suppression-${SEED}-${i}`;battle.battleSeedState=null;battle.turn=1;scene.waveSeed=`suppression-wave-${SEED}`;
  Phaser.Math.RND.sow([`suppression-global-${SEED}-${i}`]);battle.randSeedInt(2);
  const active=()=>slots.flatMap(p=>[+p.canApplyAbility(false),+p.canApplyAbility(true)]);
  const w=scene.arena.weather;
  rows.push(`C ${i} ${mode} ${holder} ${target} ${+p.turnData.acted}`,`G ${initial} ${sid??-1}`,
   `W ${w?.weatherType??0} ${w?.turnsLeft??0} ${w?.maxDuration??0}`,`CAN ${+base.getCondition()(slots[holder],p,allMoves[MoveId.GASTRO_ACID])}`,
   `AC ${active().join(" ")}`,`R ${stateRng(Phaser.Math.RND.state())}`,`B ${stateRng(battle.battleSeedState)}`,...slots.map((p,b)=>`I ${b} ${snapshot(p).join(" ")}`));
  const result=mode===2?base.getCondition()(slots[holder],p,allMoves[MoveId.GASTRO_ACID])
   :(mode===0?base:core).apply(slots[holder],p,allMoves[mode===0?MoveId.GASTRO_ACID:MoveId.CORE_ENFORCER],[]);
  rows.push(`RESULT ${+result}`);
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
 rows.push(`SUPPRESSION_COMPLETE ${N}`);fs.writeFileSync(process.env.SUPPRESSION_OUT??"/tmp/suppression.txt",rows.join("\n")+"\n");
},600000);
