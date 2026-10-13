/* Additive owning boundary proof. Executes pinned TransformAttr, Imposter's
 * real canApply/apply and PokemonTransformPhase; only presentation is stubbed. */
import { BattleScene } from "#app/battle-scene";
import { audioManager } from "#app/global-audio-manager";
import { speciesDataRegistry } from "#app/globals/global-species-data-registry";
import { allMoves, allAbilities } from "#data/data-lists";
import { AbilityId } from "#enums/ability-id";
import { BattlerTagType } from "#enums/battler-tag-type";
import { MoveId } from "#enums/move-id";
import { SpeciesId } from "#enums/species-id";
import { Stat } from "#enums/stat";
import { PokemonMove } from "#moves/pokemon-move";
import { applyAbAttrs } from "#abilities/apply-ab-attrs";
import { FaintPhase } from "#phases/faint-phase";
import { GameManager } from "#test/framework/game-manager";
import Phaser from "phaser";
import * as fs from "node:fs";
import { it, vi } from "vitest";


const N=Number(process.env.RECEIVER_N??2000),SEED=Number(process.env.RECEIVER_SEED??777);
let seed=SEED;const rnd=(n:number)=>{seed=(Math.imul(seed,1103515245)+12345)>>>0;return(seed>>>8)%n;};
const stateRng=(s:string)=>s.replace(/^!rnd,/,"").replaceAll(","," ");
const rows=[`RECEIVER 1 ${N} ${SEED}`];
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

it("actual fainted-ally ability copying",async()=>{
 if(!process.versions.node.startsWith("22.")||!Number.isSafeInteger(N)||N<96)throw Error("Node22 and N>=96 required");
 const game=new GameManager(new Phaser.Game({type:Phaser.HEADLESS}));
 game.override.battleStyle("double").ability(AbilityId.BALL_FETCH).enemyAbility(AbilityId.BALL_FETCH)
  .passiveAbility(AbilityId.NONE).enemyPassiveAbility(AbilityId.NONE).enemySpecies(SpeciesId.MAGIKARP)
  .moveset(MoveId.SPLASH).enemyMoveset(MoveId.SPLASH).disableTrainerWaves().mysteryEncounterChance(0);
 await game.classicMode.startBattle(SpeciesId.RATTATA,SpeciesId.RATTATA);
 game.override.ability(AbilityId.NONE).enemyAbility(AbilityId.NONE).moveset([]).enemyMoveset([]);
 const scene:any=game.scene,pm:any=scene.phaseManager,battle:any=scene.currentBattle,slots:any[]=scene.getField();
 if(slots.length!==4||slots.some(p=>!p))throw Error("natural doubles required");
 BattleScene.prototype.randBattleSeedInt=function(range:number,min=0){return(this as any).currentBattle.randSeedInt(range,min);};
 const copied=[AbilityId.NONE,AbilityId.BALL_FETCH,AbilityId.INTIMIDATE,AbilityId.INTREPID_SWORD,AbilityId.DOWNLOAD,AbilityId.DRIZZLE,AbilityId.WONDER_GUARD,AbilityId.RECEIVER,AbilityId.POWER_OF_ALCHEMY,AbilityId.MULTITYPE,AbilityId.IMPOSTER];
 for(const p of slots){
  vi.spyOn(p,"loadAssets").mockImplementation(()=>Promise.resolve());vi.spyOn(p,"updateInfo").mockImplementation(()=>Promise.resolve());
  vi.spyOn(p,"playAnim").mockImplementation(()=>{});vi.spyOn(p,"hideInfo").mockImplementation(()=>Promise.resolve());
  vi.spyOn(p,"faintCry").mockImplementation((cb:any)=>cb());
 }
 vi.spyOn(audioManager,"playSound").mockImplementation(()=>{});
 scene.tweens.add=(config:any)=>{config.onComplete?.();return{};};
 const counts=Array(32).fill(0);
 for(let i=0;i<N;i++){
  const state=i%32,kind=Math.floor(i/32)%3,holder=state%2?2:0,victim=([14,15,22,23].includes(state))?holder+1:state%4<2?holder+1:(holder<2?3:1);counts[state]++;
  for(const p of slots){
   p.resetSummonData();p.resetTurnData();p.resetBattleAndWaveData();p.species=speciesDataRegistry.getSpecies(SpeciesId.RATTATA);p.formIndex=0;p.fusionSpecies=null;
   p.customPokemonData.ability=AbilityId.BALL_FETCH;p.customPokemonData.passive=AbilityId.NONE;p.passive=false;p.level=30+rnd(21);p.calculateStats();p.hp=p.getMaxHp();p.status=null;p.friendship=70;p.switchOutStatus=false;p.generateName();
   p.moveset=[new PokemonMove(MoveId.SPLASH)];if(!p.isOnField())scene.field.add(p);
  }
  const user=slots[holder],dead=slots[victim];
  dead.species=speciesDataRegistry.getSpecies(SpeciesId.PIDGEY);dead.formIndex=0;dead.calculateStats();dead.generateName();
  user.customPokemonData.ability=state%8<4?AbilityId.RECEIVER:AbilityId.POWER_OF_ALCHEMY;
  dead.customPokemonData.ability=copied[Math.floor(state/2)%copied.length];dead.hp=kind===2?0:state===24?dead.getMaxHp():0;
  if(state===4||state===5){user.customPokemonData.ability=AbilityId.BALL_FETCH;user.passive=true;user.customPokemonData.passive=AbilityId.RECEIVER;}
  if(state===6||state===7){user.customPokemonData.ability=AbilityId.BALL_FETCH;user.passive=true;user.customPokemonData.passive=AbilityId.POWER_OF_ALCHEMY;}
  if(state===8||state===9)user.summonData.abilitySuppressed=true;
  if(state===10||state===11)user.hp=0;
  if(state===12||state===13){dead.passive=true;dead.customPokemonData.passive=AbilityId.INTIMIDATE;dead.customPokemonData.ability=AbilityId.BALL_FETCH;}
  if(state===14||state===15)dead.summonData.ability=AbilityId.DRIZZLE;
  if(state===16||state===17)dead.customPokemonData.ability=AbilityId.NONE;
  if(state===18||state===19){user.passive=true;user.customPokemonData.passive=AbilityId.POWER_OF_ALCHEMY;}
  if(state===20||state===21)dead.customPokemonData.ability=AbilityId.INTIMIDATE;
  if(state===22||state===23)dead.customPokemonData.ability=AbilityId.MULTITYPE;
  if(state===26||state===27)dead.customPokemonData.ability=AbilityId.INTREPID_SWORD;
  if(state===28||state===29)dead.customPokemonData.ability=AbilityId.DOWNLOAD;
  if(state===30||state===31){dead.customPokemonData.ability=AbilityId.DRIZZLE;user.summonData.statStages[Stat.ATK-1]=rnd(13)-6;}
  scene.modifiers=[];scene.enemyModifiers=[];scene.arena.weather=null;scene.arena.terrain=null;scene.arena.tags=[];
  scene.arena.playerFaints=0;battle.enemyFaints=0;battle.playerFaintsHistory=[];battle.enemyFaintsHistory=[];
  battle.playerParticipantIds=new Set([slots[0].id,slots[1].id]);battle.postBattleLoot=[];battle.battleScore=0;
  pm.clearAllPhases();pm.phaseQueue.deferredActive=false;pm.dynamicQueueManager.clearLastTurnOrder();scene.abilityBar.setVisible(false);
  battle.battleSeed=`receiver-${SEED}-${i}`;battle.battleSeedState=null;battle.turn=1;scene.waveSeed=`receiver-wave-${SEED}`;
  Phaser.Math.RND.sow([`receiver-global-${SEED}-${i}`]);battle.randSeedInt(2);
  rows.push(`C ${i} ${kind} ${holder} ${victim} ${scene.getMaxExpLevel()}`,`R ${stateRng(Phaser.Math.RND.state())}`,`B ${stateRng(battle.battleSeedState)}`,...slots.map((p,b)=>`I ${b} ${snapshot(p).join(" ")}`));
  if(kind===2){const ph:any=new FaintPhase(victim,true);vi.spyOn(ph,"end").mockImplementation(()=>{});pm.currentPhase=ph;ph.start();await Promise.resolve();await Promise.resolve();}
  else applyAbAttrs("PostKnockOutAbAttr",{pokemon:user,victim:dead,simulated:kind===1});
  const weather=scene.arena.weather,terrain=scene.arena.terrain,participants=[...battle.playerParticipantIds].sort((a:any,b:any)=>a-b);
  rows.push(`WORLD ${weather?.weatherType??0} ${weather?.turnsLeft??0} ${weather?.maxDuration??0} ${terrain?.terrainType??0} ${terrain?.turnsLeft??0} ${terrain?.maxDuration??0} ${scene.arena.playerFaints} ${battle.enemyFaints} ${battle.battleScore} ${battle.postBattleLoot.length} ${+scene.abilityBar.isVisible()} ${participants.length} ${participants[0]??0} ${participants[1]??0}`);
  rows.push(...slots.map((p,b)=>`O ${b} ${snapshot(p).join(" ")}`),`RO ${stateRng(Phaser.Math.RND.state())}`,`BO ${stateRng(battle.battleSeedState)}`);
  const queue:string[]=[];
  const emit=(scope:string,level:number,j:number,p:any)=>{
   const name=p.phaseName==="DynamicPhaseMarker"?`Marker:${p.phaseType}`:p.phaseName;
   const text=p.phaseName==="MessagePhase"?Buffer.from(p.text,"utf8").toString("hex"):"-";
   const stages=p.phaseName==="StatStageChangePhase"?p.options.changes.map((x:any)=>`${x.stat}:${x.stages}`).join(","):"-";
   queue.push(`Q ${scope} ${level} ${j} ${name} ${p.battlerIndex??-1} ${p.phaseName==="ShowAbilityPhase"?+p.passive:-1} ${text} ${stages}`);
  };
  pm.phaseQueue.levels.forEach((lv:any[],level:number)=>lv.forEach((p:any,j:number)=>emit("L",level,j,p)));
  [...pm.dynamicQueueManager.dynamicPhaseMap.entries()].filter(([,q]:any)=>q.queue.length).sort(([a]:any,[b]:any)=>a.localeCompare(b)).forEach(([,q]:any,k:number)=>q.queue.forEach((p:any,j:number)=>emit("D",k,j,p)));
  rows.push(`QCOUNT ${queue.length} ${pm.phaseQueue.levels.length} ${+pm.phaseQueue.deferredActive}`,...queue);
  for(const p of slots)p.resetSummonData();
  rows.push(...slots.map((p,b)=>`Z ${b} ${snapshot(p).join(" ")}`),"E");
 }
 rows.push(`RECEIVER_COMPLETE ${N}`);fs.writeFileSync(process.env.RECEIVER_OUT??"/tmp/receiver.txt",rows.join("\n")+"\n");console.log(`RECEIVER cases=${N} states=${counts.join("/")}`);
},600000);
