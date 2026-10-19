import { HitResult } from "#enums/hit-result";
import { CommonBattleAnim } from "#data/battle-anims";
import { getArenaTag } from "#data/arena-tag";
import { ArenaTagType } from "#enums/arena-tag-type";
import { ArenaTagSide } from "#enums/arena-tag-side";
import { Status } from "#data/status-effect";
import { StatusEffect } from "#enums/status-effect";
import { BattleScene } from "#app/battle-scene";
import { getGameMode } from "#app/game-mode";
import { speciesDataRegistry } from "#app/globals/global-species-data-registry";
import { allMoves, allAbilities } from "#data/data-lists";
import { MoveUseMode } from "#enums/move-use-mode";
import { getBattlerTag } from "#data/battler-tags";
import { AbilityId } from "#enums/ability-id";
import { BattlerTagType } from "#enums/battler-tag-type";
import { GameModes } from "#enums/game-modes";
import { MoveId } from "#enums/move-id";
import { SpeciesId } from "#enums/species-id";
import { Stat } from "#enums/stat";
import { PokemonMove } from "#moves/pokemon-move";
import { GameManager } from "#test/framework/game-manager";
import Phaser from "phaser";
import * as fs from "node:fs";
import { it,vi } from "vitest";
const rng=(s:string)=>s.replace(/^!rnd,/,"").replaceAll(","," ");
const hex=(s:string)=>s?Buffer.from(s,"utf8").toString("hex"):"-";
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

import { TrainerSlot } from "#enums/trainer-slot";
import { BattlerTagLapseType } from "#enums/battler-tag-lapse-type";
import { SpeciesFormChangeAbilityTrigger, SpeciesFormChangeActiveTrigger } from "#data/form-change-triggers";

import { achvs } from "#system/achv";
import { SpeciesFormChangeTeraTrigger, SpeciesFormChangeLapseTeraTrigger } from "#data/form-change-triggers";
it("native Tera phase and form queue boundaries",async()=>{
 const N=16384,seed=Number(process.env.TERA_SEED??777),native=BattleScene.prototype.randBattleSeedInt;
 if(!process.versions.node.startsWith("22."))throw Error("Node22 required");
 const g=new GameManager(new Phaser.Game({type:Phaser.HEADLESS}));g.override.battleStyle("double").ability(AbilityId.BALL_FETCH).enemyAbility(AbilityId.BALL_FETCH).enemySpecies(SpeciesId.RATTATA).moveset(MoveId.SPLASH).enemyMoveset(MoveId.SPLASH).disableTrainerWaves().mysteryEncounterChance(0);
 await g.classicMode.startBattle(SpeciesId.BULBASAUR,SpeciesId.CHARMANDER);BattleScene.prototype.randBattleSeedInt=native;
 g.override.ability(AbilityId.NONE).enemyAbility(AbilityId.NONE).moveset([]).enemyMoveset([]);
 const scene:any=g.scene,battle:any=scene.currentBattle,pm:any=scene.phaseManager;
 const actors:any[]=[...scene.getPlayerField(),...scene.getEnemyField(),...Array.from({length:2},()=>scene.addPlayerPokemon(speciesDataRegistry.getSpecies(SpeciesId.PIDGEY),50)),...Array.from({length:2},()=>scene.addEnemyPokemon(speciesDataRegistry.getSpecies(SpeciesId.PIDGEY),50,TrainerSlot.NONE))];
 if(actors.length!==8)throw Error("natural doubles plus both real benches required");
 vi.spyOn(pm,"shiftPhase").mockImplementation(()=>{});
 for(const p of actors)vi.spyOn(p,"updateSpritePipelineData").mockImplementation(()=>{});
 const animations:any[]=[];vi.spyOn(CommonBattleAnim.prototype,"play").mockImplementation(function(this:any,_r:any,done:any){animations.push(this);done?.();});
 const achievement=vi.spyOn(scene,"validateAchv");
 const fd=fs.openSync(process.env.TERA_OUT??"/tmp/tera.txt","w"),write=(xs:string[])=>fs.writeSync(fd,xs.join("\n")+"\n"),actor=(p:any)=>actors.indexOf(p);
 write([`TERA 1 ${N} ${seed}`]);let quiet=0,stellar=0,player=0,cleared=0,animationsCount=0;
 const queues=()=>{
  const out:string[]=[];pm.phaseQueue.levels.forEach((lv:any[],l:number)=>lv.forEach((p:any,j:number)=>{
   let who=-1,target=-1,text="-",prompt=-1;
   if(p.phaseName==="QuietFormChangePhase"){who=actor(p.pokemon);target=p.pokemon.species.forms.findIndex((f:any)=>f.formKey===p.formChange.formKey);quiet++;}
   else if(p.phaseName==="MessagePhase"){text=hex(p.text);prompt=+!!p.prompt;}
   else throw Error(`unexpected queue ${p.phaseName}`);
   out.push(`P ${l} ${j} ${p.phaseName} ${who} ${target} ${prompt} ${text}`);
  }));
  if([...pm.dynamicQueueManager.dynamicPhaseMap.values()].some((q:any)=>q.queue.length))throw Error("unexpected dynamic phases");
  return [`Q ${out.length} ${pm.phaseQueue.levels.length} ${+pm.phaseQueue.deferredActive} ${pm.phaseQueue.currentLevel}`,...out];
 };
 for(let i=0;i<N;i++){
  const mode=i%4,u=Math.floor(i/4)%8,sp=[SpeciesId.BULBASAUR,SpeciesId.OGERPON,SpeciesId.TERAPAGOS][Math.floor(i/32)%3],shape=Math.floor(i/96)%8;
  scene.gameMode=getGameMode(GameModes.ENDLESS);battle.waveIndex=201;battle.turn=1;battle.double=true;scene.modifiers=[];scene.enemyModifiers=[];scene.arena.tags=[];scene.arena.weather=null;scene.arena.terrain=null;
  scene.getPlayerParty().splice(0,99,actors[0],actors[1],actors[4],actors[5]);scene.getEnemyParty().splice(0,99,actors[2],actors[3],actors[6],actors[7]);
  for(let k=0;k<8;k++){
   const p=actors[k];p.id=1000+k;p.resetSummonData();p.resetTurnData();p.resetBattleAndWaveData();p.species=speciesDataRegistry.getSpecies(k===u?sp:SpeciesId.BULBASAUR);p.formIndex=k===u?Math.floor(i/768)%Math.max(1,p.species.forms.length):0;p.fusionSpecies=null;p.customPokemonData.ability=AbilityId.NONE;p.summonData.ability=AbilityId.NONE;p.customPokemonData.passive=AbilityId.NONE;p.passive=false;p.nature=0;p.ivs=Array(6).fill(15);p.level=[1,50,256,1000000][(shape+k)%4];p.stats=Array(6).fill(0);p.hp=undefined;p.calculateStats();p.hp=shape===0?0:shape===1?1:p.getMaxHp();p.status=p.hp?null:new Status(StatusEffect.FAINT);p.switchOutStatus=false;p.moveset=[new PokemonMove(MoveId.SPLASH)];p.isTerastallized=!!(shape%2);p.teraType=Math.floor(i/6144)%19;p.summonData.addedType=shape%3===0?null:shape%3===1?7:11;p.generateName();if(!Number.isInteger(p.formIndex))throw Error("invalid fixture form index");
  }
  // Cycle every actual tera type independently of role/form selectors.
  actors[u].teraType=Math.floor(i/4)%19;
  scene.arena.playerTerasUsed=[0,1,2,3,255,65535,2147483647,9007199254740991][shape];
  pm.clearAllPhases();pm.dynamicQueueManager.clearLastTurnOrder();animations.length=0;achievement.mockClear();
  battle.battleSeed=`tera-${seed}-${i}`;battle.battleSeedState=null;Phaser.Math.RND.sow([`tera-${seed}-${i}`]);battle.randSeedInt(2);
  const p=actors[u],rows=[`C ${i} ${mode} ${u}`,`W ${scene.waveSeed}`,`R ${rng(Phaser.Math.RND.state())}`,`B ${rng(battle.battleSeedState)}`,`A ${scene.arena.playerTerasUsed}`,...actors.map((p,k)=>`I ${k} ${snapshot(p).join(" ")}`),`IQ ${pm.phaseQueue.levels.length} ${+pm.phaseQueue.deferredActive} ${pm.phaseQueue.currentLevel}`];
  if(mode<2){const phase=pm.create("TeraPhase",p);pm.currentPhase=phase;if(mode===0)phase.start();else phase.end();}
  else scene.triggerPokemonFormChange(p,mode===2?SpeciesFormChangeTeraTrigger:SpeciesFormChangeLapseTeraTrigger);
  rows.push(`AO ${scene.arena.playerTerasUsed}`,...actors.map((p,k)=>`O ${k} ${snapshot(p).join(" ")}`),...queues(),`ANIM ${animations.length}`);
  for(const a of animations){rows.push(`AN ${a.commonAnim} ${actor(a.user)} ${actor(a.target)} ${+a.playRegardlessOfIssues}`);animationsCount++;}
  rows.push(`ACH ${achievement.mock.calls.length}`);
  for(const [a] of achievement.mock.calls){const name=Object.entries(achvs).find(([,v])=>v===a)?.[0];if(!name)throw Error("unknown achievement");rows.push(`AC ${name}`);stellar+=+(name==="STELLAR_TERASTALLIZE");}
  player+=+(mode<2&&p.isPlayer());cleared+=+(mode<2&&p.summonData.addedType==null);rows.push(`RO ${rng(Phaser.Math.RND.state())}`,`BO ${rng(battle.battleSeedState)}`,"E");write(rows);
 }
 if(quiet<100||stellar<100||player<100||cleared<100||animationsCount<100)throw Error(`coverage ${quiet}/${stellar}/${player}/${cleared}/${animationsCount}`);
 write([`TERA_COMPLETE ${N}`]);fs.closeSync(fd);console.log(`TERA quiet=${quiet} stellar=${stellar} player=${player} clear=${cleared} anim=${animationsCount}`);
},600000);
