import { getArenaTag } from "#data/arena-tag";
import { ArenaTagType } from "#enums/arena-tag-type";
import { ArenaTagSide } from "#enums/arena-tag-side";
import { Status } from "#data/status-effect";
import { StatusEffect } from "#enums/status-effect";
import { BattleScene } from "#app/battle-scene";
import { getGameMode } from "#app/game-mode";
import { speciesDataRegistry } from "#app/globals/global-species-data-registry";
import { allMoves } from "#data/data-lists";
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

it("actual Spectral Thief raw attribute and queued sources",async()=>{
 const N=8192,SEED=Number(process.env.SPECTRAL_SEED??777),native=BattleScene.prototype.randBattleSeedInt;
 if(!process.versions.node.startsWith("22."))throw Error("Node22 required");
 const g=new GameManager(new Phaser.Game({type:Phaser.HEADLESS}));g.override.battleStyle("double").ability(AbilityId.BALL_FETCH).enemyAbility(AbilityId.BALL_FETCH).passiveAbility(AbilityId.NONE).enemyPassiveAbility(AbilityId.NONE).enemySpecies(SpeciesId.RATTATA).moveset(MoveId.SPLASH).enemyMoveset(MoveId.SPLASH).disableTrainerWaves().mysteryEncounterChance(0);
 await g.classicMode.startBattle(SpeciesId.RATTATA,SpeciesId.RATTATA);BattleScene.prototype.randBattleSeedInt=native;
 g.override.ability(AbilityId.NONE).enemyAbility(AbilityId.NONE).moveset([]).enemyMoveset([]);
 const scene:any=g.scene,battle:any=scene.currentBattle,pm:any=scene.phaseManager,actors:any[]=[...scene.getPlayerField(),...scene.getEnemyField()];if(actors.length!==4)throw Error("natural doubles required");
 vi.spyOn(pm,"shiftPhase").mockImplementation(()=>{});for(const p of actors)vi.spyOn(p,"updateInfo").mockImplementation(async()=>{});
 const attr=allMoves[MoveId.SPECTRAL_THIEF].getAttrs("SpectralThiefAttr")[0];if(!attr)throw Error("owning registered attribute required");
 const fd=fs.openSync(process.env.SPECTRAL_OUT??"/tmp/spectral-thief.txt","w"),write=(rows:string[])=>fs.writeSync(fd,rows.join("\n")+"\n");write([`SPECTRAL_THIEF 1 ${N} ${SEED}`]);let empty=0,zero=0,queued=0;
 for(let i=0;i<N;i++){
  const u=i%4,t=Math.floor(i/4)%4;scene.gameMode=getGameMode(GameModes.ENDLESS);battle.gameMode=scene.gameMode;battle.waveIndex=201;battle.turn=1;scene.modifiers=[];scene.enemyModifiers=[];scene.arena.tags=[];scene.arena.weather=null;scene.arena.terrain=null;
  for(let k=0;k<4;k++){
   const p=actors[k];p.resetSummonData();p.resetTurnData();p.resetBattleAndWaveData();p.species=speciesDataRegistry.getSpecies([SpeciesId.BULBASAUR,SpeciesId.CHARMANDER,SpeciesId.SQUIRTLE,SpeciesId.MAGIKARP][k]);p.formIndex=0;p.fusionSpecies=null;p.customPokemonData.ability=[AbilityId.BALL_FETCH,AbilityId.SIMPLE,AbilityId.CONTRARY][Math.floor(i/256)%3];p.customPokemonData.passive=AbilityId.NONE;p.passive=false;p.summonData.ability=p.customPokemonData.ability;p.nature=0;p.ivs=Array(6).fill(15);p.level=[1,50,256,1000000][(Math.floor(i/64)+k)%4];p.stats=Array(6).fill(0);p.hp=undefined;p.calculateStats();p.hp=(Math.floor(i/128)+k)%5===0?0:p.getMaxHp();p.status=null;p.switchOutStatus=false;p.moveset=[new PokemonMove(MoveId.SPECTRAL_THIEF)];p.generateName();
   for(let stat=1;stat<=7;stat++)p.setStatStage(stat,((Math.floor(i/16)+k*3+stat)%13)-6);
   if(i%17===0)for(let stat=1;stat<=7;stat++)p.setStatStage(stat,-6);
   if(i%19===0&&k===u)for(let stat=1;stat<=7;stat++)p.setStatStage(stat,6);
  }
  pm.clearAllPhases();battle.battleSeed=`spectral-${SEED}-${i}`;battle.battleSeedState=null;Phaser.Math.RND.sow([`spectral-${SEED}-${i}`]);battle.randSeedInt(2);
  const rows=[`C ${i} ${u} ${t}`,`R ${rng(Phaser.Math.RND.state())}`,`B ${rng(battle.battleSeedState)}`,...actors.map((p,k)=>`I ${k} ${snapshot(p).join(" ")}`)];
  if(!attr.apply(actors[u],actors[t],allMoves[MoveId.SPECTRAL_THIEF],[]))throw Error("native override always returns true");
  rows.push(...actors.map((p,k)=>`O ${k} ${snapshot(p).join(" ")}`));
  const flat=pm.phaseQueue.levels.flat();rows.push(`QCOUNT ${flat.length} ${pm.phaseQueue.levels.length} ${+pm.phaseQueue.deferredActive}`);
  pm.phaseQueue.levels.forEach((lv:any[],l:number)=>lv.forEach((p:any,k:number)=>{
   if(p.phaseName==="MessagePhase")rows.push(`Q ${l} ${k} M ${hex(p.text)}`);
   else if(p.phaseName==="DynamicPhaseMarker"&&p.phaseType==="StatStageChangePhase")rows.push(`Q ${l} ${k} S -`);
   else throw Error(`unexpected native phase ${p.phaseName}`);
  }));
  const dynamic:any[]=[...pm.dynamicQueueManager.dynamicPhaseMap.values()].flatMap((q:any)=>q.queue);rows.push(`DCOUNT ${dynamic.length}`);empty+=+(dynamic.length===0);queued+=dynamic.length;
  dynamic.forEach((p:any,k:number)=>{const o=p.options;if(p.phaseName!=="StatStageChangePhase"||o.changes.length>1)throw Error("unexpected dynamic payload");zero+=+(o.changes.length===0);rows.push(`D ${k} ${p.battlerIndex} ${actors.indexOf(o.sourcePokemon)} ${+p.selfTarget} ${o.changes.length} ${o.changes[0]?.stat??0} ${o.changes[0]?.stages??0} ${+!!o.ignoreAbilities} ${+!!o.processed} ${o.source??0} ${+(o.message!==undefined)}`);});
  rows.push(`RO ${rng(Phaser.Math.RND.state())}`,`BO ${rng(battle.battleSeedState)}`,"E");write(rows);
 }
 if(empty<100||zero<100||queued<100)throw Error(`spectral coverage missing ${empty}/${zero}/${queued}`);write([`SPECTRAL_THIEF_COMPLETE ${N}`]);fs.closeSync(fd);console.log(`SPECTRAL_THIEF empty=${empty} zeroChanges=${zero} queued=${queued}`);
},600000);
