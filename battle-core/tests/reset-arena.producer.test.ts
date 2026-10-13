/* Unchanged doPostBattleCleanup, QuietFormChangePhase and ReturnPhase. Non-fused Battle Bond actors; encounter-controller phases are projected out. */
import { audioManager } from "#app/global-audio-manager";
import { speciesDataRegistry } from "#app/globals/global-species-data-registry";
import { AbilityId } from "#enums/ability-id";
import { BattlerTagType } from "#enums/battler-tag-type";
import { MoveId } from "#enums/move-id";
import { SpeciesId } from "#enums/species-id";
import { Stat } from "#enums/stat";
import { StatusEffect } from "#enums/status-effect";
import { PokemonMove } from "#moves/pokemon-move";
import { BattleType } from "#enums/battle-type";
import { GameManager } from "#test/framework/game-manager";
import Phaser from "phaser";
import * as fs from "node:fs";
import { it, vi } from "vitest";


const N=Number(process.env.RESETARENA_N??2000),SEED=Number(process.env.RESETARENA_SEED??777);
let seed=SEED;const rnd=(n:number)=>{seed=(Math.imul(seed,1103515245)+12345)>>>0;return(seed>>>8)%n;};
const stateRng=(s:string)=>s.replace(/^!rnd,/,"").replaceAll(","," ");
const rows=[`RESETARENA 2 ${N} ${SEED}`];
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

it("actual arena cleanup predicate and phase callbacks",async()=>{
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
 const timers:(()=>void)[]=[];let texts:string[]=[];
 vi.spyOn(scene.time,"delayedCall").mockImplementation((_delay:any,callback:any)=>{timers.push(callback);return{} as any;});
 vi.spyOn(scene.ui,"showText").mockImplementation((text:any,_delay:any,callback:any)=>{texts.push(text);callback?.();});
 vi.spyOn(pm,"shiftPhase").mockImplementation(()=>{});
 vi.spyOn(scene,"updateFieldScale").mockImplementation(async()=>{});
 const counts=Array(32).fill(0);
 for(let i=0;i<N;i++){
  const state=i%32,kind=Math.floor(i/32)%4,holder=state%2?2:0,victim=holder<2?2:0;counts[state]++;
  for(const p of slots){
   p.resetSummonData();p.resetTurnData();p.resetBattleAndWaveData();p.species=speciesDataRegistry.getSpecies(SpeciesId.RATTATA);p.formIndex=0;p.fusionSpecies=null;p.fusionCustomPokemonData=null;p.fusionFormIndex=0;p.fusionAbilityIndex=0;
   p.customPokemonData.ability=AbilityId.BALL_FETCH;p.customPokemonData.passive=AbilityId.NONE;p.passive=false;p.level=30+rnd(21);p.calculateStats();p.hp=p.getMaxHp();p.status=null;p.isTerastallized=false;p.stellarTypesBoosted=[];p.friendship=70;p.switchOutStatus=false;p.generateName();
   p.moveset=[new PokemonMove(MoveId.SPLASH)];if(!p.isOnField())scene.field.add(p);
  }
  const user=slots[holder],dead=slots[victim];
  dead.species=speciesDataRegistry.getSpecies(SpeciesId.PIDGEY);dead.formIndex=0;dead.calculateStats();dead.generateName();dead.hp=kind===2?0:dead.getMaxHp();
  user.customPokemonData.ability=AbilityId.BATTLE_BOND;
  if([2,3].includes(state))user.summonData.abilitiesApplied.add(AbilityId.BATTLE_BOND);
  if([4,5,6,7,8,9,28,29].includes(state)){user.species=speciesDataRegistry.getSpecies(SpeciesId.BATTLE_BOND_GRENINJA);user.formIndex=[6,7].includes(state)?1:0;}
  if([8,9].includes(state))user.fusionSpecies=speciesDataRegistry.getSpecies(SpeciesId.RATTATA);
  if([10,11].includes(state))user.fusionSpecies=speciesDataRegistry.getSpecies(SpeciesId.BATTLE_BOND_GRENINJA);
  if([12,13].includes(state)){user.customPokemonData.ability=AbilityId.BALL_FETCH;user.passive=true;user.customPokemonData.passive=AbilityId.BATTLE_BOND;}
  if([14,15].includes(state)){user.passive=true;user.customPokemonData.passive=AbilityId.BATTLE_BOND;}
  if([16,17].includes(state))user.summonData.abilitySuppressed=true;
  if([18,19].includes(state))user.hp=0;
  if([20,21].includes(state))user.switchOutStatus=true;
  if([22,23].includes(state))user.summonData.statStages.fill(6);
  if([24,25].includes(state)){user.passive=true;user.customPokemonData.passive=state===24?AbilityId.SIMPLE:AbilityId.CONTRARY;}
  if([26,27,28,29].includes(state)){user.summonData.speciesForm=speciesDataRegistry.getSpecies(state<28?SpeciesId.BATTLE_BOND_GRENINJA:SpeciesId.RATTATA);user.summonData.gender=1;}
  if([30,31].includes(state)){user.species=speciesDataRegistry.getSpecies(SpeciesId.BATTLE_BOND_GRENINJA);user.formIndex=1;}
  if(user.fusionSpecies){user.fusionFormIndex=0;user.fusionAbilityIndex=0;user.fusionCustomPokemonData={ability:AbilityId.BATTLE_BOND};}
  // Fusion naming is a separate outstanding source boundary.
  user.fusionSpecies=null;user.fusionCustomPokemonData=null;user.generateName();
  // Return ends without SwitchSummon.switchAndSummon: Natural Cure and
  // Regenerator must not gain ordinary switch-out effects at this boundary.
  slots[1].customPokemonData.ability=state%8<4?AbilityId.NATURAL_CURE:AbilityId.REGENERATOR;
  slots[1].hp=Math.floor(slots[1].getMaxHp()/2);slots[1].status={effect:StatusEffect.BURN,turnCount:0,cureTurn:0};
  scene.modifiers=[];scene.enemyModifiers=[];scene.arena.weather=null;scene.arena.terrain=null;scene.arena.tags=[];
  scene.arena.playerFaints=0;battle.enemyFaints=0;battle.playerFaintsHistory=[];battle.enemyFaintsHistory=[];
  battle.playerParticipantIds=new Set([slots[0].id,slots[1].id]);battle.postBattleLoot=[];battle.battleScore=0;
  pm.clearAllPhases();pm.phaseQueue.deferredActive=false;pm.dynamicQueueManager.clearLastTurnOrder();scene.abilityBar.setVisible(false);
  battle.battleSeed=`battlebond-${SEED}-${i}`;battle.battleSeedState=null;battle.turn=1;scene.waveSeed=`battlebond-wave-${SEED}`;
  Phaser.Math.RND.sow([`battlebond-global-${SEED}-${i}`]);battle.randSeedInt(2);
  scene.currentBattle.enemyParty=[];scene.currentBattle.waveIndex=kind===1?11:2;scene.currentBattle.battleType=kind===2?BattleType.TRAINER:kind===3?BattleType.MYSTERY_ENCOUNTER:BattleType.WILD;
  for(let b=2;b<4;b++){scene.field.remove(slots[b]);slots[b].switchOutStatus=true;}
  if(state%4>=2){scene.field.remove(slots[1]);slots[1].switchOutStatus=true;}
  for(const p of slots){p.visible=false;p.waveData.abilitiesApplied.add(AbilityId.BALL_FETCH);p.tempSummonData.waveTurnCount=3;p.tempSummonData.turnCount=4;p.waveData.endured=true;}
  if(process.env.RESETARENA_TERA==="1")for(const p of slots){p.isTerastallized=state>=16;p.teraType=4;p.stellarTypesBoosted=state>=16?[1,2]:[];}
  timers.length=0;texts=[];
  rows.push(`C ${i} ${kind} ${holder} ${victim} ${scene.getMaxExpLevel()}`,`R ${stateRng(Phaser.Math.RND.state())}`,`B ${stateRng(battle.battleSeedState)}`,...slots.map((p,b)=>`I ${b} ${snapshot(p).join(" ")}`));
  rows.push(`IW ${slots.map(p=>+p.waveData.endured).join(" ")}`);
  // Invoke the unchanged owning cleanup, with the new encounter's enemy party empty.
  scene.doPostBattleCleanup({waveIndex:kind===1?10:1,double:state%4<2,battleType:kind===0?BattleType.TRAINER:BattleType.WILD},scene.getMaxExpLevel());
  if(kind!==0&&scene.getPlayerParty().some((p:any)=>p.isTerastallized||p.stellarTypesBoosted.length))throw Error("source Tera reset failed");
  // The encounter/trainer/level-cap controller is outside this battle-state projection.
  pm.phaseQueue.levels.forEach((lv:any[])=>{for(let j=lv.length-1;j>=0;j--)if(["ShowTrainerPhase","NextEncounterPhase","NewBiomeEncounterPhase","LevelCapPhase"].includes(lv[j].phaseName))lv.splice(j,1);});
  const weather=scene.arena.weather,terrain=scene.arena.terrain,participants=[...battle.playerParticipantIds].sort((a:any,b:any)=>a-b);
  rows.push(`WORLD ${weather?.weatherType??0} ${weather?.turnsLeft??0} ${weather?.maxDuration??0} ${terrain?.terrainType??0} ${terrain?.turnsLeft??0} ${terrain?.maxDuration??0} ${scene.arena.playerFaints} ${battle.enemyFaints} ${battle.battleScore} ${battle.postBattleLoot.length} ${+scene.abilityBar.isVisible()} ${participants.length} ${participants[0]??0} ${participants[1]??0}`);
  rows.push(`OW ${slots.map(p=>+p.waveData.endured).join(" ")}`);
  rows.push(...slots.map((p,b)=>`O ${b} ${snapshot(p).join(" ")}`),`RO ${stateRng(Phaser.Math.RND.state())}`,`BO ${stateRng(battle.battleSeedState)}`);
  const queue:string[]=[];
  const emit=(scope:string,level:number,j:number,p:any)=>{
   const name=p.phaseName==="DynamicPhaseMarker"?`Marker:${p.phaseType}`:p.phaseName;
   const text=p.phaseName==="MessagePhase"?Buffer.from(p.text,"utf8").toString("hex"):"-";
   const stages=p.phaseName==="StatStageChangePhase"?p.options.changes.map((x:any)=>`${x.stat}:${x.stages}`).join(","):p.phaseName==="QuietFormChangePhase"?`F:${p.pokemon.species.forms.findIndex((f:any)=>f.formKey===p.formChange.formKey)}`:"-";
   queue.push(`Q ${scope} ${level} ${j} ${name} ${p.phaseName==="QuietFormChangePhase"?p.pokemon.getBattlerIndex():p.phaseName==="ReturnPhase"?p.fieldIndex:p.battlerIndex??-1} ${p.phaseName==="ShowAbilityPhase"?+p.passive:-1} ${text} ${stages}`);
  };
  pm.phaseQueue.levels.forEach((lv:any[],level:number)=>lv.forEach((p:any,j:number)=>emit("L",level,j,p)));
  [...pm.dynamicQueueManager.dynamicPhaseMap.entries()].filter(([,q]:any)=>q.queue.length).sort(([a]:any,[b]:any)=>a.localeCompare(b)).forEach(([,q]:any,k:number)=>q.queue.forEach((p:any,j:number)=>emit("D",k,j,p)));
  rows.push(`QCOUNT ${queue.length} ${pm.phaseQueue.levels.length} ${+pm.phaseQueue.deferredActive}`,...queue);
  let executed=0;const executedPhases:string[]=[];
  for(;;){let ph=pm.phaseQueue.getNextPhase();if(ph?.phaseName==="DynamicPhaseMarker")ph=pm.dynamicQueueManager.popNextPhase(ph.phaseType);if(!ph)break;
   if(!["QuietFormChangePhase","ReturnPhase","MessagePhase","ShowAbilityPhase","HideAbilityPhase"].includes(ph.phaseName))throw Error(`uncovered cleanup phase ${ph.phaseName}`);
   executedPhases.push(ph.phaseName+(typeof ph.battlerIndex==="number"?`:${ph.battlerIndex}`:""));
   if(++executed>64)throw Error("cleanup loop");pm.currentPhase=ph;
   if(ph.phaseName==="MessagePhase"){texts.push(ph.text);continue;}
   await ph.start();while(timers.length)timers.shift()!();await Promise.resolve();
  }
  rows.push(`TRACE ${executedPhases.length}`,...executedPhases.map(name=>`X ${name}`));
  rows.push(`TEXT ${texts.length}`,...texts.map(text=>`T ${Buffer.from(text,"utf8").toString("hex")}`));
  rows.push(`ZW ${slots.map(p=>+p.waveData.endured).join(" ")}`);
  rows.push(...slots.map((p,b)=>`Z ${b} ${snapshot(p).join(" ")}`),`ZR ${stateRng(Phaser.Math.RND.state())}`,`ZB ${stateRng(battle.battleSeedState)}`,"E");
 }
 rows.push(`RESETARENA_COMPLETE ${N}`);fs.writeFileSync(process.env.RESETARENA_OUT??"/tmp/reset-arena.txt",rows.join("\n")+"\n");console.log(`RESETARENA cases=${N} states=${counts.join("/")}`);
},600000);
