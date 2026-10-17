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

it("native registered stat rearrangement attributes",async()=>{
 const N=12416,SEED=Number(process.env.STATREARRANGE_SEED??777),native=BattleScene.prototype.randBattleSeedInt;
 if(!process.versions.node.startsWith("22."))throw Error("Node22 required");
 const g=new GameManager(new Phaser.Game({type:Phaser.HEADLESS}));g.override.battleStyle("double").ability(AbilityId.BALL_FETCH).enemyAbility(AbilityId.BALL_FETCH).passiveAbility(AbilityId.NONE).enemyPassiveAbility(AbilityId.NONE).enemySpecies(SpeciesId.RATTATA).moveset(MoveId.SPLASH).enemyMoveset(MoveId.SPLASH).disableTrainerWaves().mysteryEncounterChance(0);
 await g.classicMode.startBattle(SpeciesId.RATTATA,SpeciesId.RATTATA);BattleScene.prototype.randBattleSeedInt=native;
 g.override.ability(AbilityId.NONE).enemyAbility(AbilityId.NONE).moveset([]).enemyMoveset([]);
 const scene:any=g.scene,battle:any=scene.currentBattle,pm:any=scene.phaseManager,actors:any[]=[...scene.getPlayerField(),...scene.getEnemyField()];if(actors.length!==4)throw Error("natural doubles required");
 vi.spyOn(pm,"shiftPhase").mockImplementation(()=>{});for(const p of actors)vi.spyOn(p,"updateInfo").mockImplementation(async()=>{});
 const moves=[MoveId.SPEED_SWAP,MoveId.POWER_SHIFT,MoveId.TOPSY_TURVY],classes=["SwapStatAttr","ShiftStatAttr","InvertStatsAttr"] as const,attrs=moves.map((m,k)=>allMoves[m].getAttrs(classes[k])[0]);if(attrs.some(x=>!x))throw Error("registered attributes required");
 const effective=(label:string,i:number)=>actors.flatMap((p,k)=>[0,Math.floor(i/3)%16].map(mask=>{
  const tags=p.summonData.tags,status=p.status,arenaTags=scene.arena.tags,global=Phaser.Math.RND.state(),seed=battle.battleSeedState;
  p.summonData.tags=[...tags,...(mask&1?[getBattlerTag(BattlerTagType.SLOW_START,5,MoveId.NONE,p.id)]:[])];
  if(mask&2)p.status=new Status(StatusEffect.PARALYSIS);
  scene.arena.tags=[...arenaTags,...(mask&4?[getArenaTag(ArenaTagType.TAILWIND,4,MoveId.TAILWIND,p.id,ArenaTagSide.BOTH)]:[]),...(mask&8?[getArenaTag(ArenaTagType.GRASS_WATER_PLEDGE,4,MoveId.GRASS_PLEDGE,p.id,ArenaTagSide.BOTH)]:[])];
  const values=[Stat.ATK,Stat.DEF,Stat.SPATK,Stat.SPDEF,Stat.SPD].map(stat=>p.getEffectiveStat(stat,{simulated:true}));
  p.summonData.tags=tags;p.status=status;scene.arena.tags=arenaTags;
  if(Phaser.Math.RND.state()!==global||battle.battleSeedState!==seed)throw Error("simulated stat probe changed RNG");
  return `EF ${label} ${k} ${mask} ${values.join(" ")}`;
 }));
 const fd=fs.openSync(process.env.STATREARRANGE_OUT??"/tmp/stat-rearrangement.txt","w"),write=(rows:string[])=>fs.writeSync(fd,rows.join("\n")+"\n");write([`STAT_REARRANGE 2 ${N} ${SEED}`]);
 const values=[-9,-1,0,.5,1,2,2147483647,2147483648,1099511627776],counts=[[0,0],[0,0],[0,0]];
 for(let i=0;i<N;i++){
  const mode=Math.floor(i/16)%3,u=i%4,t=Math.floor(i/4)%4,repeats=1+Math.floor(i/48)%3,move=moves[mode];
  scene.gameMode=getGameMode(GameModes.ENDLESS);battle.gameMode=scene.gameMode;battle.waveIndex=201;battle.turn=1;scene.modifiers=[];scene.enemyModifiers=[];scene.arena.tags=[];scene.arena.weather=null;scene.arena.terrain=null;
  for(let k=0;k<4;k++){
   const p=actors[k];p.resetSummonData();p.resetTurnData();p.resetBattleAndWaveData();p.species=speciesDataRegistry.getSpecies([SpeciesId.BULBASAUR,SpeciesId.CHARMANDER,SpeciesId.SQUIRTLE,SpeciesId.MAGIKARP][k]);p.formIndex=0;p.fusionSpecies=null;p.customPokemonData.ability=AbilityId.BALL_FETCH;p.customPokemonData.passive=AbilityId.NONE;p.passive=false;p.summonData.ability=AbilityId.BALL_FETCH;p.nature=0;p.ivs=Array(6).fill(15);p.level=[1,50,255,256,65536,1000000][(Math.floor(i/144)+k)%6];p.stats=Array(6).fill(0);p.hp=undefined;p.calculateStats();p.hp=(Math.floor(i/144)+k)%5===0?0:p.getMaxHp();p.status=null;p.switchOutStatus=false;p.moveset=[new PokemonMove(move)];p.generateName();
   for(let stat=0;stat<6;stat++)p.summonData.stats[stat]=values[(Math.floor(i/48)+stat*2+k)%values.length];
   if(i>=12288)for(let stat=0;stat<6;stat++)p.summonData.stats[stat]=[4294967315,1099511628799,-4294967315,-1099511628799,9007199254740991][(i+k+stat)%5];
   for(let stat=1;stat<=7;stat++)p.setStatStage(stat,((Math.floor(i/432)+stat*3+k*5)%13)-6);
  }
  pm.clearAllPhases();battle.battleSeed=`stat-rearrange-${SEED}-${i}`;battle.battleSeedState=null;Phaser.Math.RND.sow([`stat-rearrange-${SEED}-${i}`]);battle.randSeedInt(2);
  const rows=[`C ${i} ${mode} ${u} ${t} ${repeats}`,`R ${rng(Phaser.Math.RND.state())}`,`B ${rng(battle.battleSeedState)}`,...actors.map((p,k)=>`I ${k} ${snapshot(p).join(" ")}`)];
  rows.push(...effective("I",i));
  for(let j=0;j<repeats;j++){const applied=attrs[mode].apply(actors[u],actors[t],allMoves[move],[]);counts[mode][+applied]++;}
  rows.push(...actors.map((p,k)=>`O ${k} ${snapshot(p).join(" ")}`));
  rows.push(...effective("O",i));
  const queued=pm.phaseQueue.levels.flat();if(queued.some((p:any)=>p.phaseName!=="MessagePhase"))throw Error("unexpected native queue");rows.push(`QCOUNT ${queued.length} ${pm.phaseQueue.levels.length} ${+pm.phaseQueue.deferredActive}`);pm.phaseQueue.levels.forEach((lv:any[],l:number)=>lv.forEach((p:any,k:number)=>rows.push(`Q ${l} ${k} ${hex(p.text)}`)));rows.push(`RO ${rng(Phaser.Math.RND.state())}`,`BO ${rng(battle.battleSeedState)}`,"E");write(rows);
 }
 if(counts.some(c=>c.some(x=>x<100)))throw Error(`missing outcomes ${counts}`);write([`STAT_REARRANGE_COMPLETE ${N}`]);fs.closeSync(fd);console.log(`STAT_REARRANGE outcomes=${JSON.stringify(counts)}`);
},600000);
