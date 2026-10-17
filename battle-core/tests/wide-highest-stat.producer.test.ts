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

it("native highest-stat tag selection at wide values",async()=>{
 const N=8192,SEED=Number(process.env.HIGHESTSTAT_SEED??777),native=BattleScene.prototype.randBattleSeedInt;
 if(!process.versions.node.startsWith("22."))throw Error("Node22 required");
 const g=new GameManager(new Phaser.Game({type:Phaser.HEADLESS}));g.override.battleStyle("double").ability(AbilityId.BALL_FETCH).enemyAbility(AbilityId.BALL_FETCH).passiveAbility(AbilityId.NONE).enemyPassiveAbility(AbilityId.NONE).enemySpecies(SpeciesId.RATTATA).moveset(MoveId.SPLASH).enemyMoveset(MoveId.SPLASH).disableTrainerWaves().mysteryEncounterChance(0);
 await g.classicMode.startBattle(SpeciesId.RATTATA,SpeciesId.RATTATA);BattleScene.prototype.randBattleSeedInt=native;
 g.override.ability(AbilityId.NONE).enemyAbility(AbilityId.NONE).moveset([]).enemyMoveset([]);
 const scene:any=g.scene,battle:any=scene.currentBattle,pm:any=scene.phaseManager,actors:any[]=[...scene.getPlayerField(),...scene.getEnemyField()];if(actors.length!==4)throw Error("natural doubles required");
 vi.spyOn(pm,"shiftPhase").mockImplementation(()=>{});for(const p of actors)vi.spyOn(p,"updateInfo").mockImplementation(async()=>{});
 const fd=fs.openSync(process.env.HIGHESTSTAT_OUT??"/tmp/highest-stat.txt","w"),write=(rows:string[])=>fs.writeSync(fd,rows.join("\n")+"\n");write([`HIGHEST_STAT 1 ${N} ${SEED}`]);
 const wins=Array(5).fill(0),values=[1,100,2147483647,2147483648,4294967315,1099511627776],types=[BattlerTagType.PROTOSYNTHESIS,BattlerTagType.QUARK_DRIVE];
 for(let i=0;i<N;i++){
  const owner=i%4,mode=Math.floor(i/4)%2,type=types[mode];scene.gameMode=getGameMode(GameModes.ENDLESS);battle.gameMode=scene.gameMode;battle.waveIndex=201;battle.turn=1;scene.modifiers=[];scene.enemyModifiers=[];scene.arena.tags=[];scene.arena.weather=null;scene.arena.terrain=null;
  for(let k=0;k<4;k++){
   const p=actors[k];p.resetSummonData();p.resetTurnData();p.resetBattleAndWaveData();p.species=speciesDataRegistry.getSpecies([SpeciesId.BULBASAUR,SpeciesId.CHARMANDER,SpeciesId.SQUIRTLE,SpeciesId.MAGIKARP][k]);p.formIndex=0;p.fusionSpecies=null;p.customPokemonData.ability=i%2?AbilityId.HUGE_POWER:AbilityId.BALL_FETCH;p.customPokemonData.passive=AbilityId.NONE;p.passive=false;p.summonData.ability=p.customPokemonData.ability;p.nature=0;p.ivs=Array(6).fill(15);p.level=[1,50,256,1000000][(Math.floor(i/64)+k)%4];p.stats=Array(6).fill(0);p.hp=undefined;p.calculateStats();p.hp=p.getMaxHp();p.status=null;p.switchOutStatus=false;p.moveset=[new PokemonMove(MoveId.SPLASH)];p.generateName();
   const base=values[(Math.floor(i/8)+k)%values.length],fav=1+Math.floor(i/48)%5;
   for(let stat=1;stat<=5;stat++)p.summonData.stats[stat]=base*(stat===fav?(i%7===0?1.2:4):1);
   if(i%3===0)for(let stat=1;stat<=7;stat++)p.setStatStage(stat,((Math.floor(i/240)+stat+k)%13)-6);
  }
  pm.clearAllPhases();battle.battleSeed=`highest-${SEED}-${i}`;battle.battleSeedState=null;Phaser.Math.RND.sow([`highest-${SEED}-${i}`]);battle.randSeedInt(2);
  const rows=[`C ${i} ${owner} ${mode} ${+(i%3===0)}`,`W ${scene.waveSeed}`,`R ${rng(Phaser.Math.RND.state())}`,`B ${rng(battle.battleSeedState)}`,...actors.map((p,k)=>`I ${k} ${snapshot(p).join(" ")}`)];
  for(let step=0;step<3;step++){
   const p=actors[owner],result=step===2?p.removeTag(type):p.addTag(type,0,MoveId.NONE,p.id),tag:any=p.getTag(type);
   if(step===0){if(!result||!tag)throw Error("native add must succeed");wins[tag.stat-1]++;}
   rows.push(`STEP ${step} ${result===undefined?-1:+result}`,...actors.map((a,k)=>{const t:any=a.getTag(type);return `TAG ${k} ${+!!t} ${t?.stat??0} ${t?.multiplier??0} ${t?.turnCount??0} ${a.summonData.tags.length}`;}),...actors.map((p,k)=>`O ${k} ${snapshot(p).join(" ")}`));
   rows.push(`EF ${[Stat.ATK,Stat.DEF,Stat.SPATK,Stat.SPDEF,Stat.SPD].map(stat=>p.getEffectiveStat(stat,{simulated:true})).join(" ")}`);
   const queued=pm.phaseQueue.levels.flat();if(queued.some((p:any)=>p.phaseName!=="MessagePhase"))throw Error("unexpected native queue");rows.push(`QCOUNT ${queued.length} ${pm.phaseQueue.levels.length} ${+pm.phaseQueue.deferredActive}`);pm.phaseQueue.levels.forEach((lv:any[],l:number)=>lv.forEach((p:any,k:number)=>rows.push(`Q ${l} ${k} ${hex(p.text)}`)));
  }
  rows.push(`RO ${rng(Phaser.Math.RND.state())}`,`BO ${rng(battle.battleSeedState)}`,"E");write(rows);
 }
 if(wins.some(n=>n<100))throw Error(`missing highest stats ${wins}`);write([`HIGHEST_STAT_COMPLETE ${N}`]);fs.closeSync(fd);console.log(`HIGHEST_STAT wins=${wins}`);
},600000);
