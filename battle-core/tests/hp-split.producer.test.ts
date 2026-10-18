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

import { HitResult } from "#enums/hit-result";

it("actual Pain Split HP changes and shield bypass",async()=>{
 const N=8320,SEED=Number(process.env.HPSPLIT_SEED??777),native=BattleScene.prototype.randBattleSeedInt;
 if(!process.versions.node.startsWith("22."))throw Error("Node22 required");
 const g=new GameManager(new Phaser.Game({type:Phaser.HEADLESS}));g.override.battleStyle("double").ability(AbilityId.BALL_FETCH).enemyAbility(AbilityId.BALL_FETCH).passiveAbility(AbilityId.NONE).enemyPassiveAbility(AbilityId.NONE).enemySpecies(SpeciesId.RATTATA).moveset(MoveId.SPLASH).enemyMoveset(MoveId.SPLASH).disableTrainerWaves().mysteryEncounterChance(0);
 await g.classicMode.startBattle(SpeciesId.RATTATA,SpeciesId.RATTATA);BattleScene.prototype.randBattleSeedInt=native;
 g.override.ability(AbilityId.NONE).enemyAbility(AbilityId.NONE).moveset([]).enemyMoveset([]);
 const scene:any=g.scene,battle:any=scene.currentBattle,pm:any=scene.phaseManager,actors:any[]=[...scene.getPlayerField(),...scene.getEnemyField()];if(actors.length!==4)throw Error("natural doubles required");
 vi.spyOn(pm,"shiftPhase").mockImplementation(()=>{});for(const p of actors){vi.spyOn(p,"updateInfo").mockImplementation(async()=>{});if(!p.isPlayer())vi.spyOn(p.battleInfo,"updateBossSegments").mockImplementation(()=>{});}
 let events:string[]=[];vi.spyOn(scene.damageNumberHandler,"add").mockImplementation((p:any,n:any,result:any)=>{events.push(`EV ${actors.indexOf(p)} ${result===HitResult.HEAL?n:-n} ${actors.map(a=>a.hp).join(" ")}`);});
 const attr=allMoves[MoveId.PAIN_SPLIT].getAttrs("HpSplitAttr")[0];if(!attr)throw Error("owning registered attribute required");
 const fd=fs.openSync(process.env.HPSPLIT_OUT??"/tmp/hp-split.txt","w"),write=(rows:string[])=>fs.writeSync(fd,rows.join("\n")+"\n");write([`HP_SPLIT 3 ${N} ${SEED}`]);let denied=0,heals=0,hits=0,shields=0,faints=0;
 const boss=(p:any,k:number)=>`${k} ${+p.isBoss()} ${p.bossSegments??0} ${p.bossSegmentIndex??0} ${+p.hasTrainer()}`;
 for(let i=0;i<N;i++){
  let u=i%4,t=Math.floor(i/4)%4;if(i>=8192){u=i%2;t=2+i%2;}
  scene.gameMode=getGameMode(GameModes.ENDLESS);battle.gameMode=scene.gameMode;battle.waveIndex=201;battle.turn=1;scene.modifiers=[];scene.enemyModifiers=[];scene.arena.tags=[];scene.arena.weather=null;scene.arena.terrain=null;
  for(let k=0;k<4;k++){
   const p=actors[k];p.resetSummonData();p.resetTurnData();p.resetBattleAndWaveData();p.species=speciesDataRegistry.getSpecies([SpeciesId.BULBASAUR,SpeciesId.CHARMANDER,SpeciesId.SQUIRTLE,SpeciesId.MAGIKARP][k]);p.formIndex=0;p.fusionSpecies=null;p.customPokemonData.ability=AbilityId.BALL_FETCH;p.customPokemonData.passive=AbilityId.NONE;p.passive=false;p.summonData.ability=p.customPokemonData.ability;p.nature=0;p.ivs=Array(6).fill(15);p.level=[1,50,256,1000000][(Math.floor(i/64)+k)%4];p.stats=Array(6).fill(0);p.hp=undefined;p.calculateStats();p.status=null;p.switchOutStatus=false;p.moveset=[new PokemonMove(MoveId.PAIN_SPLIT)];p.generateName();
   const mx=p.getMaxHp();p.hp=[0,1,mx,Math.max(1,Math.floor(mx/2)),Math.max(1,mx-1)][(Math.floor(i/16)+k)%5];if(k>=2){p.bossSegments=0;p.bossSegmentIndex=0;}
  }
  if(i>=8192){for(const p of actors){p.level=50;p.calculateStats();p.hp=1;}const p=actors[t];p.bossSegments=[2,3,5,8][Math.floor(i/2)%4];p.hp=p.getMaxHp();p.bossSegmentIndex=p.getBossSegmentIndex();shields++;}
  pm.clearAllPhases();events=[];battle.battleSeed=`hp-split-${SEED}-${i}`;battle.battleSeedState=null;Phaser.Math.RND.sow([`hp-split-${SEED}-${i}`]);battle.randSeedInt(2);
  const rows=[`C ${i} ${u} ${t}`,`R ${rng(Phaser.Math.RND.state())}`,`B ${rng(battle.battleSeedState)}`,`QIN ${pm.phaseQueue.levels.length} ${+pm.phaseQueue.deferredActive} ${pm.phaseQueue.currentLevel}`,...actors.flatMap((p,k)=>[`I ${k} ${snapshot(p).join(" ")}`,`IB ${boss(p,k)}`])];
  rows.push(`COND ${+allMoves[MoveId.PAIN_SPLIT].applyConditions(actors[u],actors[t])}`);
  const result=attr.apply(actors[u],actors[t],allMoves[MoveId.PAIN_SPLIT],[]);denied+=+!result;rows.push(`RET ${+result}`,...actors.flatMap((p,k)=>[`O ${k} ${snapshot(p).join(" ")}`,`OB ${boss(p,k)}`]));
  heals+=events.filter(s=>Number(s.split(" ")[2])>0).length;hits+=events.filter(s=>Number(s.split(" ")[2])<0).length;rows.push(`ECOUNT ${events.length}`,...events);
  const flat=pm.phaseQueue.levels.flat();rows.push(`QCOUNT ${flat.length} ${pm.phaseQueue.levels.length} ${+pm.phaseQueue.deferredActive}`);
  pm.phaseQueue.levels.forEach((lv:any[],l:number)=>lv.forEach((p:any,k:number)=>{
   if(p.phaseName==="DynamicPhaseMarker"&&p.phaseType==="StatStageChangePhase")rows.push(`Q ${l} ${k} S -1 0 -1`);
   else if(p.phaseName==="FaintPhase"){faints++;rows.push(`Q ${l} ${k} F ${p.battlerIndex} ${+p.preventInstantRevive} ${p.source?actors.indexOf(p.source):-1}`);}
   else throw Error(`unexpected native phase ${p.phaseName}`);
  }));
  const dynamic:any[]=[...pm.dynamicQueueManager.dynamicPhaseMap.values()].flatMap((q:any)=>q.queue);rows.push(`DCOUNT ${dynamic.length}`);
  dynamic.forEach((p:any,k:number)=>{const o=p.options;if(p.phaseName!=="StatStageChangePhase")throw Error("unexpected dynamic payload");rows.push(`D ${k} ${p.battlerIndex} ${actors.indexOf(o.sourcePokemon)} ${+p.selfTarget} ${o.changes.length} ${+!!o.ignoreAbilities} ${+!!o.processed} ${o.source??0} ${+(o.message!==undefined)}`,...o.changes.map((c:any)=>`CHANGE ${c.stat} ${c.stages}`));});
  rows.push(`RO ${rng(Phaser.Math.RND.state())}`,`BO ${rng(battle.battleSeedState)}`,"E");write(rows);
 }
 if(denied<100||heals<100||hits<100||shields<128||faints<1)throw Error(`HP Split coverage missing ${denied}/${heals}/${hits}/${shields}/${faints}`);write([`HP_SPLIT_COMPLETE ${N}`]);fs.closeSync(fd);console.log(`HP_SPLIT denied=${denied} heals=${heals} hits=${hits} shieldControls=${shields} faints=${faints}`);
},600000);
